#include "AiChatMessageDelegate.h"
#include "shared/services/AppFonts.h"

#include <QAbstractListModel>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QColor>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <utility>

#include "features/aichat/model/AiChatMessageListModel.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/markdown/markdowndocumentmodel.h"

namespace {

constexpr int kViewPaintWidthInset = 4;

QStyleOptionViewItem optionWithPaintWidth(const QStyleOptionViewItem& option)
{
    QStyleOptionViewItem adjusted(option);
    adjusted.rect.setWidth(qMax(1, adjusted.rect.width() - kViewPaintWidthInset));
    return adjusted;
}

struct MarkdownBlockSelection {
    int start = -1;
    int end = -1;
};

class MarkdownBlockModel final : public QAbstractListModel
{
public:
    MarkdownBlockModel(const QList<MarkdownRenderer::Block>* blocks,
                       const QVector<MarkdownBlockSelection>& selections,
                       const QVector<bool>& loadingRows,
                       QObject* parent = nullptr)
        : QAbstractListModel(parent)
        , m_blocks(blocks)
        , m_selections(selections)
        , m_loadingRows(loadingRows)
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() || !m_blocks ? 0 : m_blocks->size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!m_blocks || !index.isValid() || index.row() < 0 || index.row() >= m_blocks->size()) {
            return {};
        }

        const MarkdownRenderer::Block& block = m_blocks->at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case MarkdownDocumentModel::TextRole:
            return block.text;
        case MarkdownDocumentModel::TypeRole:
            return static_cast<int>(block.type);
        case MarkdownDocumentModel::LevelRole:
            return block.level;
        case MarkdownDocumentModel::NumberRole:
            return block.number;
        case MarkdownDocumentModel::BlockRole:
            return QVariant::fromValue(block);
        case MarkdownDocumentModel::SelectionStartRole:
            return index.row() < m_selections.size() ? m_selections.at(index.row()).start : -1;
        case MarkdownDocumentModel::SelectionEndRole:
            return index.row() < m_selections.size() ? m_selections.at(index.row()).end : -1;
        case MarkdownDocumentModel::CodeBlockLoadingRole:
            return index.row() < m_loadingRows.size() && m_loadingRows.at(index.row());
        default:
            return {};
        }
    }

private:
    const QList<MarkdownRenderer::Block>* m_blocks = nullptr;
    QVector<MarkdownBlockSelection> m_selections;
    QVector<bool> m_loadingRows;
};

QColor linkTextColor(bool dark, bool isFromUser, const QColor& textColor)
{
    if (isFromUser) {
        return textColor;
    }

    Q_UNUSED(dark);
    return ThemeManager::instance().color(ThemeColor::AccentLinkText);
}

QString documentTextForLayout(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString markdownTextForLayout(const QString& sourceText)
{
    const QString text = documentTextForLayout(sourceText);
    QString result;
    result.reserve(text.size() + text.count(QLatin1Char('\n')));

    bool inFencedCodeBlock = false;
    int lineStart = 0;
    while (lineStart <= text.size()) {
        const int newline = text.indexOf(QLatin1Char('\n'), lineStart);
        const int lineEnd = newline < 0 ? text.size() : newline;
        const QString line = text.mid(lineStart, lineEnd - lineStart);
        const QString trimmedLine = line.trimmed();

        if (trimmedLine.startsWith(QStringLiteral("```")) ||
                trimmedLine.startsWith(QStringLiteral("~~~"))) {
            inFencedCodeBlock = !inFencedCodeBlock;
        }

        bool escapedListMarker = false;
        if (!inFencedCodeBlock) {
            int markerOffset = 0;
            while (markerOffset < line.size() && line.at(markerOffset).isSpace()) {
                ++markerOffset;
            }
            int markerEnd = markerOffset;
            while (markerEnd < line.size() && line.at(markerEnd).isDigit()) {
                ++markerEnd;
            }

            const bool orderedListMarker = markerEnd > markerOffset &&
                    markerEnd + 1 < line.size() &&
                    line.at(markerEnd) == QLatin1Char('.') &&
                    line.at(markerEnd + 1).isSpace();
            const bool unorderedListMarker = markerOffset + 1 < line.size() &&
                    (line.at(markerOffset) == QLatin1Char('-') ||
                     line.at(markerOffset) == QLatin1Char('+') ||
                     line.at(markerOffset) == QLatin1Char('*')) &&
                    line.at(markerOffset + 1).isSpace();

            if (orderedListMarker) {
                result += line.left(markerEnd);
                result += QLatin1Char('\\');
                result += line.mid(markerEnd);
                escapedListMarker = true;
            } else if (unorderedListMarker) {
                result += line.left(markerOffset);
                result += QLatin1Char('\\');
                result += line.mid(markerOffset);
                escapedListMarker = true;
            } else {
                result += line;
            }
        } else {
            result += line;
        }

        if (newline < 0) {
            break;
        }

        result += escapedListMarker ? QStringLiteral("  \n") : QStringLiteral("\n");
        lineStart = newline + 1;
    }

    return result;
}

void allowWrappedPreformattedBlocks(QTextDocument& document)
{
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        if (!blockFormat.nonBreakableLines()) {
            continue;
        }

        blockFormat.setNonBreakableLines(false);
        QTextCursor cursor(block);
        cursor.setBlockFormat(blockFormat);
    }
}

void addMarkdownParagraphSpacing(QTextDocument& document, const QFont& font)
{
    const qreal paragraphSpacing = QFontMetricsF(font).lineSpacing();
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (!block.next().isValid() || block.textList()) {
            continue;
        }

        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setBottomMargin(paragraphSpacing);
        QTextCursor cursor(block);
        cursor.setBlockFormat(blockFormat);
    }
}

QString textLayoutCacheKey(const QString& text, const QFont& font, int textWidth, bool isFromUser)
{
    QString key = font.toString();
    key.reserve(key.size() + text.size() + 24);
    key += QLatin1Char('\x1f');
    key += QString::number(qMax(1, textWidth));
    key += QLatin1Char('\x1f');
    key += isFromUser ? QLatin1Char('1') : QLatin1Char('0');
    key += QLatin1Char('\x1f');
    key += text;
    return key;
}

QString textDocumentCacheKey(const QString& text,
                             const QFont& font,
                             int textWidth,
                             const QColor& textColor,
                             bool isFromUser,
                             bool dark)
{
    QString key = textLayoutCacheKey(text, font, textWidth, isFromUser);
    key += QLatin1Char('\x1f');
    key += QString::number(textColor.rgba());
    key += QLatin1Char('\x1f');
    key += dark ? QLatin1Char('1') : QLatin1Char('0');
    return key;
}

int textCacheCost(const QString& text)
{
    return qBound(1, text.size() / 512 + 1, 16);
}

ThemeManager::Mode modeFromSettingValue(const QString& value, ThemeManager::Mode fallback)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("light") || normalized == QStringLiteral("浅色模式")) {
        return ThemeManager::Mode::Light;
    }
    if (normalized == QStringLiteral("dark") || normalized == QStringLiteral("深色模式")) {
        return ThemeManager::Mode::Dark;
    }
    if (normalized == QStringLiteral("follow-system") ||
            normalized == QStringLiteral("system") ||
            normalized == QStringLiteral("跟随系统")) {
        return ThemeManager::Mode::FollowSystem;
    }
    return fallback;
}

bool applySettingValue(const QString& action, const QString& value)
{
    const QString normalizedAction = action.trimmed().toLower();
    if (normalizedAction == QStringLiteral("settings.appearance.mode")) {
        ThemeManager::instance().setMode(modeFromSettingValue(value, ThemeManager::Mode::FollowSystem));
        return true;
    }

    if (normalizedAction == QStringLiteral("settings.appearance.theme_color") ||
            normalizedAction == QStringLiteral("settings.appearance.themecolor")) {
        const QColor color(value.trimmed());
        if (!color.isValid()) {
            return false;
        }
        ThemeManager::instance().setThemeColor(color);
        return true;
    }

    return false;
}

bool mightContainSettingBlock(const QString& text)
{
    return text.contains(QStringLiteral("```")) &&
            text.contains(QStringLiteral("setting"), Qt::CaseInsensitive);
}

int firstMarkdownBlockAtY(const QVector<int>& blockOffsets, const QVector<int>& blockHeights, int y)
{
    if (blockOffsets.isEmpty()) {
        return 0;
    }

    const auto begin = blockOffsets.cbegin();
    const auto end = blockOffsets.cend();
    int row = static_cast<int>(std::upper_bound(begin, end, y) - begin) - 1;
    row = qBound(0, row, blockOffsets.size() - 1);
    while (row < blockHeights.size() &&
           blockOffsets.at(row) + blockHeights.at(row) <= y) {
        ++row;
    }
    return qBound(0, row, blockOffsets.size());
}

int lastMarkdownBlockAtY(const QVector<int>& blockOffsets, int y)
{
    if (blockOffsets.isEmpty()) {
        return -1;
    }

    const auto begin = blockOffsets.cbegin();
    const auto end = blockOffsets.cend();
    int row = static_cast<int>(std::upper_bound(begin, end, y) - begin) - 1;
    return qBound(0, row, blockOffsets.size() - 1);
}

int collapsedDocumentHeight(const QTextDocument& document, int maxLines, int fallbackHeight, bool* canExpand)
{
    if (canExpand) {
        *canExpand = false;
    }
    if (maxLines <= 0) {
        return qMax(1, qCeil(document.size().height()));
    }

    int lineCount = 0;
    int collapsedHeight = qMax(1, fallbackHeight);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const QTextLayout* layout = block.layout();
        if (!layout) {
            continue;
        }

        const QPointF blockPosition = document.documentLayout()->blockBoundingRect(block).topLeft();
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            ++lineCount;
            if (lineCount == maxLines) {
                collapsedHeight = qMax(fallbackHeight,
                                       qCeil(blockPosition.y() + line.y() + line.height()));
            } else if (lineCount > maxLines) {
                if (canExpand) {
                    *canExpand = true;
                }
                return collapsedHeight;
            }
        }
    }

    return qMax(collapsedHeight, qCeil(document.size().height()));
}

int interpolatedHeight(int collapsedHeight, int fullHeight, qreal progress)
{
    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    return collapsedHeight + qRound((fullHeight - collapsedHeight) * boundedProgress);
}

qreal painterDevicePixelRatio(QPainter* painter)
{
    if (!painter || !painter->device()) {
        return 1.0;
    }
    return qMax<qreal>(1.0, painter->device()->devicePixelRatioF());
}

QPixmap transformedActionIcon(const QString& source,
                              const QSize& iconSize,
                              qreal dpr,
                              bool invert,
                              bool upsideDown)
{
    QPixmap icon = ImageService::instance().scaled(source,
                                                   iconSize,
                                                   Qt::KeepAspectRatio,
                                                   dpr);
    if (icon.isNull()) {
        return {};
    }

    QImage image = icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (invert) {
        image.invertPixels(QImage::InvertRgb);
    }
    if (upsideDown) {
        QTransform transform;
        transform.scale(-1.0, -1.0);
        image = image.transformed(transform);
    }

    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(icon.devicePixelRatio());
    return result;
}

void drawActionIcon(QPainter* painter,
                    const QString& source,
                    const QRect& buttonRect,
                    const QSize& iconSize,
                    bool invert,
                    bool upsideDown)
{
    const QPixmap icon = transformedActionIcon(source,
                                               iconSize,
                                               painterDevicePixelRatio(painter),
                                               invert,
                                               upsideDown);
    if (icon.isNull()) {
        return;
    }

    QRect target(QPoint(0, 0), iconSize);
    target.moveCenter(buttonRect.center());
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawPixmap(target, icon);
    painter->restore();
}

} // namespace

AiChatMessageDelegate::AiChatMessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    m_textDocumentCache.setMaxCost(96);
    m_textSizeCache.setMaxCost(256);
    m_urlRangesCache.setMaxCost(256);
    m_markdownCache.setMaxCost(128);
    m_markdownLayoutCache.setMaxCost(512);
}

void AiChatMessageDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    const bool dark = ThemeManager::instance().isDark();
    const QColor bubbleColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::AccentBubble)
            : ThemeManager::instance().color(ThemeColor::MessageBubblePeer);
    const bool bubbleSelected = m_bubbleSelectionIndex == index;
    const QColor effectiveBubbleColor = bubbleSelected
            ? (isFromUser ? bubbleColor.darker(118)
                          : ThemeManager::instance().color(ThemeColor::MessageBubblePeerSelected))
            : bubbleColor;
    const QColor textColor = isFromUser
            ? ThemeManager::textColorOn(effectiveBubbleColor)
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor selectionColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent)
            : ThemeManager::instance().color(ThemeColor::AccentTextSelection);

    const LayoutMetrics metrics = layoutMetrics(option, index);
    if (!isFromUser) {
        const MarkdownCacheEntry& markdown = cachedMarkdown(text);
        paintMarkdownMessage(painter,
                             option,
                             index,
                             metrics,
                             markdown,
                             textColor);
        paintAiReplyActions(painter, metrics, index);
        painter->restore();
        return;
    }

    QPainterPath bubblePath;
    bubblePath.addRoundedRect(metrics.bubbleRect, kBubbleRadius, kBubbleRadius);
    painter->fillPath(bubblePath, effectiveBubbleColor);

    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);

    QAbstractTextDocumentLayout::PaintContext paintContext;
    if (m_selectionIndex == index && hasSelection()) {
        QTextCharFormat selectionFormat;
        selectionFormat.setBackground(selectionColor);
        selectionFormat.setForeground(textColor);

        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = QTextCursor(const_cast<QTextDocument*>(&textDocument));
        selection.cursor.setPosition(m_selection.start());
        selection.cursor.setPosition(m_selection.end(), QTextCursor::KeepAnchor);
        selection.format = selectionFormat;
        paintContext.selections.push_back(selection);
    }

    painter->save();
    painter->translate(metrics.textRect.topLeft());
    painter->setClipRect(QRect(QPoint(0, 0), metrics.textRect.size()));
    textDocument.documentLayout()->draw(painter, paintContext);
    painter->restore();

    paintUserMessageChrome(painter, metrics, index, textColor);

    painter->restore();
}

QSize AiChatMessageDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return QSize(option.rect.width(),
                     qMax(0, index.data(AiChatMessageListModel::BottomSpaceHeightRole).toInt()));
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return QSize(option.rect.width(), 0);
    }

    const QStyleOptionViewItem layoutOption = optionWithPaintWidth(option);
    const LayoutMetrics metrics = layoutMetrics(layoutOption, index);
    const int contentBottom = qMax(metrics.bubbleRect.bottom(), metrics.copyButtonRect.bottom());
    const QSize result(option.rect.width(), contentBottom - layoutOption.rect.top() + 1 + kVerticalMargin);
    return result;
}

bool AiChatMessageDelegate::bubbleHitTest(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& viewportPos) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return false;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(metrics.bubbleRect, kBubbleRadius, kBubbleRadius);
    return bubblePath.contains(viewportPos);
}

int AiChatMessageDelegate::characterIndexAt(const QStyleOptionViewItem& option,
                                            const QModelIndex& index,
                                            const QPoint& viewportPos,
                                            bool allowLineWhitespace) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return -1;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return -1;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    const QRect hitRect = allowLineWhitespace
            ? metrics.bubbleRect.adjusted(-2, -2, 2, 2)
            : metrics.textRect;
    if (!hitRect.contains(viewportPos)) {
        return -1;
    }

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    if (!isFromUser) {
        const MarkdownCacheEntry& markdown = cachedMarkdown(text);
        if (markdown.blocks.isEmpty()) {
            return -1;
        }

        const MarkdownLayoutCacheEntry& markdownLayout = cachedMarkdownLayout(markdown,
                                                                              option,
                                                                              messageFont(),
                                                                              metrics.textRect.width());
        const int localY = viewportPos.y() - metrics.textRect.top();
        const int row = firstMarkdownBlockAtY(markdownLayout.blockOffsets,
                                              markdownLayout.blockHeights,
                                              localY);
        if (row >= 0 && row < markdown.blocks.size()) {
            MarkdownBlockModel markdownModel(&markdown.blocks, {}, {});
            QStyleOptionViewItem blockOption(option);
            blockOption.font = messageFont();
            blockOption.palette.setColor(QPalette::Text,
                                         ThemeManager::instance().color(ThemeColor::PrimaryText));
            blockOption.rect = QRect(metrics.textRect.left(),
                                     metrics.textRect.top() + markdownLayout.blockOffsets.at(row),
                                     metrics.textRect.width(),
                                     markdownLayout.blockHeights.at(row));
            const QModelIndex blockIndex = markdownModel.index(row, 0);

            if (blockOption.rect.adjusted(-2, -2, 2, 2).contains(viewportPos)) {
                if (!allowLineWhitespace &&
                        !m_markdownDelegate.hasTextAtPosition(blockOption, blockIndex, viewportPos)) {
                    return -1;
                }

                const int localCursor = m_markdownDelegate.cursorForPosition(blockOption,
                                                                             blockIndex,
                                                                             viewportPos);
                return qBound(0,
                              markdown.blockStartOffsets.value(row) + localCursor,
                              markdown.plainText.size());
            }
        }

        if (allowLineWhitespace) {
            return viewportPos.y() < metrics.textRect.center().y() ? 0 : markdown.plainText.size();
        }
        return -1;
    }

    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = isFromUser
            ? ThemeManager::textColorOn(ThemeManager::instance().color(ThemeColor::AccentBubble))
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);

    const QPointF local = viewportPos - metrics.textRect.topLeft();
    const qreal height = textDocument.size().height();

    if (local.y() < 0) {
        return allowLineWhitespace ? 0 : -1;
    }
    if (local.y() == 0) {
        return 0;
    }
    if (local.y() >= height) {
        return allowLineWhitespace ? textDocument.toPlainText().size() : -1;
    }

    const int textLength = textDocument.toPlainText().size();
    const int lineCursor = SelectableText::lineCursorAt(textDocument,
                                                            local,
                                                            textLength,
                                                            allowLineWhitespace);
    if (lineCursor >= 0) {
        return lineCursor;
    }

    const Qt::HitTestAccuracy accuracy = allowLineWhitespace ? Qt::FuzzyHit : Qt::ExactHit;
    const int cursor = textDocument.documentLayout()->hitTest(local, accuracy);
    if (cursor < 0) {
        const int fallbackCursor = SelectableText::lineCursorAt(textDocument,
                                                                local,
                                                                textLength,
                                                                allowLineWhitespace);
        return fallbackCursor;
    }

    const int boundedCursor = qBound(0, cursor, textLength);
    return boundedCursor;
}

QString AiChatMessageDelegate::urlAt(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return {};
    }

    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return {};
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    const LayoutMetrics metrics = layoutMetrics(option, index);
    if (!metrics.textRect.contains(viewportPos)) {
        return {};
    }

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    if (!isFromUser) {
        const MarkdownCacheEntry& markdown = cachedMarkdown(text);
        const MarkdownLayoutCacheEntry& markdownLayout = cachedMarkdownLayout(markdown,
                                                                              option,
                                                                              messageFont(),
                                                                              metrics.textRect.width());
        const int localY = viewportPos.y() - metrics.textRect.top();
        const int row = firstMarkdownBlockAtY(markdownLayout.blockOffsets,
                                              markdownLayout.blockHeights,
                                              localY);
        if (row >= 0 && row < markdown.blocks.size()) {
            MarkdownBlockModel markdownModel(&markdown.blocks, {}, {});
            QStyleOptionViewItem blockOption(option);
            blockOption.font = messageFont();
            blockOption.palette.setColor(QPalette::Text,
                                         ThemeManager::instance().color(ThemeColor::PrimaryText));
            blockOption.rect = QRect(metrics.textRect.left(),
                                     metrics.textRect.top() + markdownLayout.blockOffsets.at(row),
                                     metrics.textRect.width(),
                                     markdownLayout.blockHeights.at(row));
            const QModelIndex blockIndex = markdownModel.index(row, 0);
            if (blockOption.rect.contains(viewportPos)) {
                const QString markdownLink = m_markdownDelegate.linkAtPosition(blockOption,
                                                                               blockIndex,
                                                                               viewportPos);
                if (!markdownLink.isEmpty()) {
                    return markdownLink;
                }
            }
        }

        const QVector<TextRange> urls = cachedUrlRanges(markdown.plainText);
        for (const TextRange& url : urls) {
            const int end = url.start + url.length;
            if (cursor >= url.start && cursor <= end) {
                return url.text;
            }
        }
        return {};
    }

    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = isFromUser
            ? ThemeManager::textColorOn(ThemeManager::instance().color(ThemeColor::AccentBubble))
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);
    const QString anchor = textDocument.documentLayout()->anchorAt(viewportPos - metrics.textRect.topLeft());
    if (!anchor.isEmpty()) {
        return anchor;
    }

    const QString rendered = renderedPlainText(text, isFromUser);
    const QString normalizedText = documentTextForLayout(text);
    if (rendered == normalizedText) {
        const QVector<TextRange> urls = cachedUrlRanges(text);
        for (const TextRange& url : urls) {
            const int end = url.start + url.length;
            if (cursor >= url.start && cursor <= end) {
                return url.text;
            }
        }
    }

    return {};
}

bool AiChatMessageDelegate::isCodeCopyButtonAt(const QStyleOptionViewItem& option,
                                               const QModelIndex& index,
                                               const QPoint& viewportPos) const
{
    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid || hit.block.type != MarkdownRenderer::BlockType::CodeBlock) {
        return false;
    }

    const QList<MarkdownRenderer::Block> blocks{hit.block};
    MarkdownBlockModel markdownModel(&blocks, {}, {});
    return m_markdownDelegate.isCodeCopyButtonAtPosition(hit.option,
                                                        markdownModel.index(0, 0),
                                                        viewportPos);
}

bool AiChatMessageDelegate::isCodeCopyButtonDisabledAt(const QStyleOptionViewItem& option,
                                                       const QModelIndex& index,
                                                       const QPoint& viewportPos) const
{
    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid || hit.block.type != MarkdownRenderer::BlockType::CodeBlock) {
        return false;
    }

    const bool loading = hit.block.open &&
            index.data(AiChatMessageListModel::MessageIdRole).toString() == m_streamingMessageId;
    if (!loading) {
        return false;
    }

    const QList<MarkdownRenderer::Block> blocks{hit.block};
    const QVector<bool> loadingRows{true};
    MarkdownBlockModel markdownModel(&blocks, {}, loadingRows);
    return m_markdownDelegate.isCodeCopyButtonAtPosition(hit.option,
                                                         markdownModel.index(0, 0),
                                                         viewportPos) &&
            !m_markdownDelegate.isCodeCopyButtonEnabledAtPosition(hit.option,
                                                                  markdownModel.index(0, 0),
                                                                  viewportPos);
}

bool AiChatMessageDelegate::isSettingActionButtonAt(const QStyleOptionViewItem& option,
                                                    const QModelIndex& index,
                                                    const QPoint& viewportPos) const
{
    if (!mightContainSettingBlock(index.data(AiChatMessageListModel::TextRole).toString())) {
        return false;
    }

    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid || hit.block.type != MarkdownRenderer::BlockType::SettingBlock) {
        return false;
    }

    const QList<MarkdownRenderer::Block> blocks{hit.block};
    MarkdownBlockModel markdownModel(&blocks, {}, {});
    return m_markdownDelegate.isSettingActionButtonAtPosition(hit.option,
                                                              markdownModel.index(0, 0),
                                                              viewportPos);
}

bool AiChatMessageDelegate::toggleSettingActionAt(const QStyleOptionViewItem& option,
                                                  const QModelIndex& index,
                                                  const QPoint& viewportPos)
{
    if (!mightContainSettingBlock(index.data(AiChatMessageListModel::TextRole).toString())) {
        return false;
    }

    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid ||
            hit.block.type != MarkdownRenderer::BlockType::SettingBlock) {
        return false;
    }

    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    QSet<int>& revokedRows = m_revokedSettingRowsByMessage[messageId];
    if (revokedRows.contains(hit.row)) {
        if (!applySettingValue(hit.block.settingAction, hit.block.settingValueText)) {
            return false;
        }
        revokedRows.remove(hit.row);
        if (revokedRows.isEmpty()) {
            m_revokedSettingRowsByMessage.remove(messageId);
        }
    } else {
        if (!applySettingValue(hit.block.settingAction, hit.block.settingPreviousValueText)) {
            return false;
        }
        revokedRows.insert(hit.row);
    }
    return true;
}

int AiChatMessageDelegate::codeCopyBlockRowAt(const QStyleOptionViewItem& option,
                                              const QModelIndex& index,
                                              const QPoint& viewportPos) const
{
    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid ||
            hit.block.type != MarkdownRenderer::BlockType::CodeBlock ||
            !isCodeCopyButtonAt(option, index, viewportPos) ||
            isCodeCopyButtonDisabledAt(option, index, viewportPos)) {
        return -1;
    }

    return hit.row;
}

QString AiChatMessageDelegate::codeBlockTextAt(const QStyleOptionViewItem& option,
                                               const QModelIndex& index,
                                               const QPoint& viewportPos) const
{
    const MarkdownBlockHit hit = markdownBlockAt(option, index, viewportPos);
    if (!hit.valid ||
            hit.block.type != MarkdownRenderer::BlockType::CodeBlock ||
            !isCodeCopyButtonAt(option, index, viewportPos) ||
            isCodeCopyButtonDisabledAt(option, index, viewportPos)) {
        return {};
    }

    return hit.block.text;
}

AiChatMessageDelegate::MessageAction AiChatMessageDelegate::messageActionAt(
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QPoint& viewportPos) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return MessageAction::None;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    if (index.data(AiChatMessageListModel::IsFromUserRole).toBool()) {
        if (metrics.copyButtonRect.contains(viewportPos)) {
            return MessageAction::Copy;
        }
        if (metrics.userCanExpand && metrics.expandRect.contains(viewportPos)) {
            return MessageAction::ToggleExpansion;
        }
        return MessageAction::None;
    }

    const QVector<QPair<MessageAction, QRect>> actions = aiReplyActionRects(metrics, index);
    for (const QPair<MessageAction, QRect>& action : actions) {
        if (action.second.contains(viewportPos)) {
            return action.first;
        }
    }
    return MessageAction::None;
}

bool AiChatMessageDelegate::selectWordAt(const QStyleOptionViewItem& option,
                                         const QModelIndex& index,
                                         const QPoint& viewportPos)
{
    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return false;
    }

    const QString text = renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString(),
                                           index.data(AiChatMessageListModel::IsFromUserRole).toBool());
    const SelectableText::Range range = SelectableText::wordRangeAt(text, cursor);
    if (!range.isValid()) {
        return false;
    }

    setSelection(index, range.start, range.end);
    return hasSelection();
}

void AiChatMessageDelegate::setMessageFeedback(const QString& messageId, MessageFeedback feedback)
{
    if (messageId.isEmpty()) {
        return;
    }

    if (feedback == MessageFeedback::None) {
        m_messageFeedback.remove(messageId);
        return;
    }

    m_messageFeedback.insert(messageId, feedback);
}

AiChatMessageDelegate::MessageFeedback AiChatMessageDelegate::messageFeedback(
        const QString& messageId) const
{
    return m_messageFeedback.value(messageId, MessageFeedback::None);
}

void AiChatMessageDelegate::setStreamingMessageId(const QString& messageId)
{
    if (m_streamingMessageId == messageId) {
        return;
    }
    m_streamingMessageId = messageId;
    emit streamingMessageIdChanged();
}

QString AiChatMessageDelegate::streamingMessageId() const
{
    return m_streamingMessageId;
}

bool AiChatMessageDelegate::hasStreamingOpenCodeBlock(const QModelIndex& index) const
{
    if (m_streamingMessageId.isEmpty() ||
            !index.isValid() ||
            index.data(AiChatMessageListModel::MessageIdRole).toString() != m_streamingMessageId) {
        return false;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return false;
    }

    const MarkdownCacheEntry& markdown = cachedMarkdown(text);
    for (const MarkdownRenderer::Block& block : markdown.blocks) {
        if (block.type == MarkdownRenderer::BlockType::CodeBlock && block.open) {
            return true;
        }
    }
    return false;
}

QRect AiChatMessageDelegate::streamingCodeBlockUpdateRect(const QStyleOptionViewItem& option,
                                                          const QModelIndex& index) const
{
    if (m_streamingMessageId.isEmpty() ||
            !index.isValid() ||
            index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool() ||
            index.data(AiChatMessageListModel::IsFromUserRole).toBool() ||
            index.data(AiChatMessageListModel::MessageIdRole).toString() != m_streamingMessageId) {
        return {};
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return {};
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    const MarkdownCacheEntry& markdown = cachedMarkdown(text);
    if (markdown.blocks.isEmpty()) {
        return {};
    }

    const MarkdownLayoutCacheEntry& markdownLayout = cachedMarkdownLayout(markdown,
                                                                          option,
                                                                          messageFont(),
                                                                          metrics.textRect.width());
    QVector<bool> loadingRows(markdown.blocks.size());
    bool hasLoadingRow = false;
    for (int row = 0; row < markdown.blocks.size(); ++row) {
        const MarkdownRenderer::Block& block = markdown.blocks.at(row);
        const bool loading = block.type == MarkdownRenderer::BlockType::CodeBlock && block.open;
        loadingRows[row] = loading;
        hasLoadingRow = hasLoadingRow || loading;
    }

    if (!hasLoadingRow) {
        return {};
    }

    MarkdownBlockModel markdownModel(&markdown.blocks, {}, loadingRows);
    const QRect viewportRect = option.widget ? option.widget->rect() : option.rect;
    QRect updateRect;
    for (int row = 0; row < markdown.blocks.size(); ++row) {
        if (!loadingRows.at(row)) {
            continue;
        }

        QStyleOptionViewItem blockOption(option);
        blockOption.font = messageFont();
        blockOption.rect = QRect(metrics.textRect.left(),
                                 metrics.textRect.top() + markdownLayout.blockOffsets.at(row),
                                 metrics.textRect.width(),
                                 markdownLayout.blockHeights.at(row));
        blockOption.state &= ~(QStyle::State_Selected |
                               QStyle::State_MouseOver |
                               QStyle::State_HasFocus);
        blockOption.state |= QStyle::State_Enabled;

        const QModelIndex blockIndex = markdownModel.index(row, 0);
        const QRect blockUpdate = m_markdownDelegate.codeBlockLoadingUpdateRect(blockOption, blockIndex);
        if (!blockUpdate.isValid() || !viewportRect.intersects(blockUpdate)) {
            continue;
        }

        updateRect = updateRect.isNull() ? blockUpdate : updateRect.united(blockUpdate);
    }

    return updateRect;
}

void AiChatMessageDelegate::setCopiedCodeBlock(const QModelIndex& index, int blockRow)
{
    m_copiedCodeMessageIndex = QPersistentModelIndex(index);
    m_copiedCodeBlockRow = blockRow;
}

void AiChatMessageDelegate::clearCopiedCodeBlock()
{
    m_copiedCodeMessageIndex = QPersistentModelIndex();
    m_copiedCodeBlockRow = -1;
}

void AiChatMessageDelegate::setUserMessageExpanded(const QString& messageId, bool expanded)
{
    if (messageId.isEmpty()) {
        return;
    }

    if (expanded) {
        if (!m_userMessageExpansionProgress.contains(messageId)) {
            m_userMessageExpansionProgress.insert(messageId, 1.0);
        }
        return;
    }

    m_userMessageExpansionProgress.remove(messageId);
}

bool AiChatMessageDelegate::isUserMessageExpanded(const QString& messageId) const
{
    return !messageId.isEmpty() && m_userMessageExpansionProgress.contains(messageId);
}

void AiChatMessageDelegate::setUserMessageExpansionProgress(const QString& messageId, qreal progress)
{
    if (messageId.isEmpty()) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    if (boundedProgress <= 0.0001) {
        m_userMessageExpansionProgress.insert(messageId, 0.0);
        return;
    }
    m_userMessageExpansionProgress.insert(messageId, boundedProgress);
}

qreal AiChatMessageDelegate::userMessageExpansionProgress(const QString& messageId) const
{
    return m_userMessageExpansionProgress.value(messageId, 0.0);
}

void AiChatMessageDelegate::notifySizeHintChanged(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    emit sizeHintChanged(index);
}

void AiChatMessageDelegate::setUserCopyButtonOpacity(const QString& messageId, qreal opacity)
{
    if (messageId.isEmpty()) {
        return;
    }

    const qreal boundedOpacity = qBound(0.0, opacity, 1.0);
    if (boundedOpacity <= 0.001) {
        m_userCopyButtonOpacity.remove(messageId);
        return;
    }
    m_userCopyButtonOpacity.insert(messageId, boundedOpacity);
}

qreal AiChatMessageDelegate::userCopyButtonOpacity(const QString& messageId) const
{
    return m_userCopyButtonOpacity.value(messageId, 0.0);
}

void AiChatMessageDelegate::setSelection(const QModelIndex& index, int anchor, int cursor)
{
    m_selectionIndex = QPersistentModelIndex(index);
    m_bubbleSelectionIndex = QPersistentModelIndex();
    const int textLength = renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString(),
                                             index.data(AiChatMessageListModel::IsFromUserRole).toBool()).size();
    m_selection.set(anchor, cursor, textLength);
}

void AiChatMessageDelegate::clearSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selection.clear();
}

void AiChatMessageDelegate::setBubbleSelection(const QModelIndex& index)
{
    clearSelection();
    m_bubbleSelectionIndex = QPersistentModelIndex(index);
}

void AiChatMessageDelegate::clearBubbleSelection()
{
    m_bubbleSelectionIndex = QPersistentModelIndex();
}

bool AiChatMessageDelegate::hasSelection() const
{
    return m_selectionIndex.isValid() && m_selection.hasSelection();
}

bool AiChatMessageDelegate::hasBubbleSelection() const
{
    return m_bubbleSelectionIndex.isValid();
}

bool AiChatMessageDelegate::selectionContains(const QModelIndex& index, int cursor) const
{
    if (!hasSelection() || m_selectionIndex != index || cursor < 0) {
        return false;
    }

    return m_selection.contains(cursor);
}

QString AiChatMessageDelegate::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }

    const QString text = renderedPlainText(m_selectionIndex.data(AiChatMessageListModel::TextRole).toString(),
                                           m_selectionIndex.data(AiChatMessageListModel::IsFromUserRole).toBool());
    return m_selection.selectedText(text);
}

QString AiChatMessageDelegate::renderedText(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }

    return renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString(),
                             index.data(AiChatMessageListModel::IsFromUserRole).toBool());
}

QPersistentModelIndex AiChatMessageDelegate::selectionIndex() const
{
    return m_selectionIndex;
}

AiChatMessageDelegate::LayoutMetrics AiChatMessageDelegate::layoutMetrics(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const
{
    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    const int bubbleMaxWidth = maxBubbleWidth(option.rect.width());
    const int maxTextWidth = qMax(1, bubbleMaxWidth - kBubblePadding * 2);
    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    if (!isFromUser) {
        const int markdownWidth = qMax(1,
                                       option.rect.width() - kHorizontalMargin * 2 +
                                               kMarkdownHorizontalInset * 2);
        const QSize markdownSize = markdownDocumentSize(cachedMarkdown(text),
                                                        option,
                                                        messageFont(),
                                                        markdownWidth);
        const int x = option.rect.left() + kHorizontalMargin - kMarkdownHorizontalInset;
        const int y = option.rect.top() + kVerticalMargin;
        const bool hasActions = isAiReplyActionVisible(index, MessageAction::Copy) ||
                isAiReplyActionVisible(index, MessageAction::Refresh) ||
                isAiReplyActionVisible(index, MessageAction::Like) ||
                isAiReplyActionVisible(index, MessageAction::Dislike);
        const int actionHeight = hasActions ? kActionTopMargin + kActionButtonSize : 0;
        const QRect markdownRect(x, y, markdownSize.width(), markdownSize.height());
        const QRect outerRect(x,
                              y,
                              markdownSize.width(),
                              markdownSize.height() + actionHeight);
        return {outerRect, markdownRect, markdownSize};
    }

    const TextSizeCacheEntry& textMeasure = cachedTextSize(text, messageFont(), maxTextWidth, isFromUser);
    const int visibleTextHeight = textMeasure.canExpand
            ? interpolatedHeight(textMeasure.collapsedHeight,
                                 textMeasure.fullSize.height(),
                                 userMessageExpansionProgress(
                                         index.data(AiChatMessageListModel::MessageIdRole).toString()))
            : textMeasure.fullSize.height();
    const QSize textSize(textMeasure.fullSize.width(), visibleTextHeight);
    const int expandHeight = textMeasure.canExpand ? kUserExpandTopGap + kUserExpandHeight : 0;
    const int bubbleWidth = qMin(textMeasure.fullSize.width() + kBubblePadding * 2,
                                 bubbleMaxWidth);
    const int bubbleHeight = visibleTextHeight + kBubblePadding * 2 + expandHeight;
    const int x = isFromUser
            ? option.rect.right() - kHorizontalMargin - bubbleWidth + 1
            : option.rect.left() + kHorizontalMargin;
    const QRect bubbleRect(x, option.rect.top() + kVerticalMargin, bubbleWidth, bubbleHeight);
    const QRect textRect(bubbleRect.left() + kBubblePadding,
                         bubbleRect.top() + kBubblePadding,
                         qMax(1, bubbleRect.width() - kBubblePadding * 2),
                         qMax(1, visibleTextHeight));
    QRect expandRect;
    if (textMeasure.canExpand) {
        expandRect = QRect(textRect.left(),
                           textRect.bottom() + 1 + kUserExpandTopGap,
                           textRect.width(),
                           kUserExpandHeight);
    }
    const QRect copyButtonRect(bubbleRect.right() - kUserCopyButtonSize + 1,
                               bubbleRect.bottom() + 1 + kUserCopyButtonTopMargin,
                               kUserCopyButtonSize,
                               kUserCopyButtonSize);

    return {bubbleRect, textRect, textSize, expandRect, copyButtonRect, textMeasure.canExpand};
}

void AiChatMessageDelegate::configureTextDocument(QTextDocument& document,
                                                  const QString& text,
                                                  const QFont& font,
                                                  int textWidth,
                                                  const QColor& textColor,
                                                  bool isFromUser,
                                                  bool dark) const
{
    document.setUndoRedoEnabled(false);
    document.setDocumentMargin(0);
    document.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(textOption);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (isFromUser) {
        document.setPlainText(documentTextForLayout(text));
    } else {
        document.setMarkdown(markdownTextForLayout(text));
        allowWrappedPreformattedBlocks(document);
        addMarkdownParagraphSpacing(document, font);
    }
#else
    document.setPlainText(documentTextForLayout(text));
#endif
    document.setDefaultTextOption(textOption);
    document.setTextWidth(qMax(1, textWidth));

    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat textFormat;
    textFormat.setForeground(textColor);
    cursor.mergeCharFormat(textFormat);

    const QColor urlColor = linkTextColor(dark, isFromUser, textColor);
    QTextCharFormat linkFormat;
    linkFormat.setForeground(urlColor);
    linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    const int characterCount = qMax(0, document.characterCount() - 1);
    for (int position = 0; position < characterCount; ++position) {
        QTextCursor charCursor(&document);
        charCursor.setPosition(position);
        charCursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        if (charCursor.charFormat().isAnchor()) {
            charCursor.mergeCharFormat(linkFormat);
        }
    }

    if (document.toPlainText() == documentTextForLayout(text)) {
        const QVector<TextRange> urls = cachedUrlRanges(text);
        for (const TextRange& url : urls) {
            cursor.clearSelection();
            cursor.setPosition(url.start);
            cursor.setPosition(url.start + url.length, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(linkFormat);
        }
    }
}

QSize AiChatMessageDelegate::textDocumentSize(const QString& text,
                                              const QFont& font,
                                              int maxTextWidth,
                                              bool isFromUser) const
{
    return cachedTextSize(text, font, maxTextWidth, isFromUser).fullSize;
}

const AiChatMessageDelegate::TextSizeCacheEntry& AiChatMessageDelegate::cachedTextSize(
        const QString& text,
        const QFont& font,
        int maxTextWidth,
        bool isFromUser) const
{
    const QString key = textLayoutCacheKey(text, font, maxTextWidth, isFromUser);
    if (const TextSizeCacheEntry* entry = m_textSizeCache.object(key)) {
        return *entry;
    }

    QTextDocument textDocument;
    configureTextDocument(textDocument,
                          text,
                          font,
                          maxTextWidth,
                          ThemeManager::instance().color(ThemeColor::PrimaryText),
                          isFromUser,
                          false);

    const int width = qMin(qCeil(textDocument.idealWidth()), maxTextWidth);
    const int height = qCeil(textDocument.size().height());

    auto* entry = new TextSizeCacheEntry;
    entry->fullSize = QSize(qCeil(width), qCeil(height));
    entry->collapsedHeight = height;
    if (isFromUser) {
        const int fallbackLineHeight = QFontMetrics(font).lineSpacing();
        entry->collapsedHeight = collapsedDocumentHeight(textDocument,
                                                         kUserCollapsedMaxLines,
                                                         fallbackLineHeight,
                                                         &entry->canExpand);
        entry->collapsedHeight = qMin(entry->collapsedHeight, entry->fullSize.height());
    }
    m_textSizeCache.insert(key, entry, textCacheCost(text));
    return *m_textSizeCache.object(key);
}

const QTextDocument& AiChatMessageDelegate::cachedTextDocument(const QString& text,
                                                               const QFont& font,
                                                               int textWidth,
                                                               const QColor& textColor,
                                                               bool isFromUser,
                                                               bool dark) const
{
    const QString key = textDocumentCacheKey(text, font, textWidth, textColor, isFromUser, dark);
    if (const TextDocumentCacheEntry* entry = m_textDocumentCache.object(key)) {
        return entry->document;
    }

    auto* entry = new TextDocumentCacheEntry;
    configureTextDocument(entry->document,
                          text,
                          font,
                          textWidth,
                          textColor,
                          isFromUser,
                          dark);
    m_textDocumentCache.insert(key, entry, textCacheCost(text));
    return m_textDocumentCache.object(key)->document;
}

QVector<AiChatMessageDelegate::TextRange> AiChatMessageDelegate::cachedUrlRanges(
        const QString& text) const
{
    if (const UrlRangesCacheEntry* entry = m_urlRangesCache.object(text)) {
        return entry->ranges;
    }

    auto* entry = new UrlRangesCacheEntry;
    entry->ranges = urlRanges(text);
    const QVector<TextRange> ranges = entry->ranges;
    m_urlRangesCache.insert(text, entry, textCacheCost(text));
    return ranges;
}

QVector<AiChatMessageDelegate::TextRange> AiChatMessageDelegate::urlRanges(const QString& text) const
{
    static const QRegularExpression urlRegex(
            QStringLiteral(R"(\b((?:https?://|www\.)[A-Za-z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+))"),
            QRegularExpression::CaseInsensitiveOption);

    QVector<TextRange> ranges;
    QRegularExpressionMatchIterator it = urlRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString rawUrl = match.captured(1);
        const QString url = SelectableText::trimmedUrlText(rawUrl);
        if (url.isEmpty()) {
            continue;
        }

        TextRange range;
        range.start = match.capturedStart(1);
        range.length = url.size();
        range.text = url;
        ranges.push_back(range);
    }
    return ranges;
}

const AiChatMessageDelegate::MarkdownCacheEntry& AiChatMessageDelegate::cachedMarkdown(
        const QString& text) const
{
    const QString normalizedText = documentTextForLayout(text);
    if (const MarkdownCacheEntry* entry = m_markdownCache.object(normalizedText)) {
        return *entry;
    }

    auto* entry = new MarkdownCacheEntry;
    entry->sourceText = normalizedText;
    entry->blocks = MarkdownRenderer::parseBlocks(normalizedText);
    entry->blockStartOffsets.reserve(entry->blocks.size());

    for (int row = 0; row < entry->blocks.size(); ++row) {
        if (entry->blocks.at(row).type == MarkdownRenderer::BlockType::SettingBlock) {
            entry->hasSettingBlocks = true;
        }
        if (row > 0) {
            entry->plainText += QLatin1Char('\n');
        }
        entry->blockStartOffsets.append(entry->plainText.size());
        entry->plainText += entry->blocks.at(row).text;
    }

    m_markdownCache.insert(normalizedText, entry, textCacheCost(normalizedText));
    return *m_markdownCache.object(normalizedText);
}

const AiChatMessageDelegate::MarkdownLayoutCacheEntry& AiChatMessageDelegate::cachedMarkdownLayout(
        const MarkdownCacheEntry& entry,
        const QStyleOptionViewItem& option,
        const QFont& font,
        int maxTextWidth) const
{
    QString key = font.toString();
    key += QLatin1Char('\x1f');
    key += QString::number(qMax(1, maxTextWidth));
    key += QLatin1Char('\x1f');
    key += QString::number(option.widget ? qMax(1, option.widget->logicalDpiY()) : 96);
    key += QLatin1Char('\x1f');
    key += QString::number(qHash(entry.sourceText));
    key += QLatin1Char(':');
    key += QString::number(entry.sourceText.size());

    if (const MarkdownLayoutCacheEntry* cached = m_markdownLayoutCache.object(key)) {
        return *cached;
    }

    auto* layout = new MarkdownLayoutCacheEntry;
    layout->blockHeights.reserve(entry.blocks.size());
    layout->blockOffsets.reserve(entry.blocks.size());

    MarkdownBlockModel markdownModel(&entry.blocks, {}, {});
    int height = 0;
    for (int row = 0; row < entry.blocks.size(); ++row) {
        QStyleOptionViewItem blockOption(option);
        blockOption.font = font;
        blockOption.rect = QRect(0, 0, qMax(1, maxTextWidth), 1);
        blockOption.state &= ~(QStyle::State_Selected |
                               QStyle::State_MouseOver |
                               QStyle::State_HasFocus);
        blockOption.state |= QStyle::State_Enabled;

        const QModelIndex blockIndex = markdownModel.index(row, 0);
        const int blockHeight = m_markdownDelegate.sizeHint(blockOption, blockIndex).height();
        layout->blockOffsets.append(height);
        layout->blockHeights.append(blockHeight);
        height += blockHeight;
    }

    layout->size = QSize(qMax(1, maxTextWidth), height);
    const int cost = qBound(1,
                            entry.sourceText.size() / 1024 + layout->blockHeights.size() / 24 + 1,
                            64);
    m_markdownLayoutCache.insert(key, layout, cost);
    return *m_markdownLayoutCache.object(key);
}

QSize AiChatMessageDelegate::markdownDocumentSize(const MarkdownCacheEntry& entry,
                                                  const QStyleOptionViewItem& option,
                                                  const QFont& font,
                                                  int maxTextWidth) const
{
    if (entry.blocks.isEmpty()) {
        return QSize(qMax(1, maxTextWidth), 0);
    }

    return cachedMarkdownLayout(entry, option, font, maxTextWidth).size;
}

AiChatMessageDelegate::MarkdownBlockHit AiChatMessageDelegate::markdownBlockAt(
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QPoint& viewportPos) const
{
    MarkdownBlockHit hit;
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool() ||
            index.data(AiChatMessageListModel::IsFromUserRole).toBool()) {
        return hit;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return hit;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    const MarkdownCacheEntry& markdown = cachedMarkdown(text);
    if (markdown.blocks.isEmpty()) {
        return hit;
    }

    const MarkdownLayoutCacheEntry& markdownLayout = cachedMarkdownLayout(markdown,
                                                                          option,
                                                                          messageFont(),
                                                                          metrics.textRect.width());
    const int localY = viewportPos.y() - metrics.textRect.top();
    const int row = firstMarkdownBlockAtY(markdownLayout.blockOffsets,
                                          markdownLayout.blockHeights,
                                          localY);
    if (row < 0 || row >= markdown.blocks.size()) {
        return hit;
    }

    QStyleOptionViewItem blockOption(option);
    blockOption.font = messageFont();
    blockOption.palette.setColor(QPalette::Text,
                                 ThemeManager::instance().color(ThemeColor::PrimaryText));
    blockOption.rect = QRect(metrics.textRect.left(),
                             metrics.textRect.top() + markdownLayout.blockOffsets.at(row),
                             metrics.textRect.width(),
                             markdownLayout.blockHeights.at(row));
    blockOption.state &= ~(QStyle::State_Selected |
                           QStyle::State_MouseOver |
                           QStyle::State_HasFocus);
    blockOption.state |= QStyle::State_Enabled;

    hit.valid = blockOption.rect.adjusted(0, -30, 0, 0).contains(viewportPos);
    hit.row = row;
    hit.block = markdown.blocks.at(row);
    hit.option = blockOption;
    return hit;
}

void AiChatMessageDelegate::paintMarkdownMessage(QPainter* painter,
                                                 const QStyleOptionViewItem& option,
                                                 const QModelIndex& index,
                                                 const LayoutMetrics& metrics,
                                                 const MarkdownCacheEntry& entry,
                                                 const QColor& textColor) const
{
    if (entry.blocks.isEmpty()) {
        return;
    }

    const MarkdownLayoutCacheEntry& markdownLayout = cachedMarkdownLayout(entry,
                                                                          option,
                                                                          messageFont(),
                                                                          metrics.textRect.width());
    const QRect viewportRect = option.widget ? option.widget->rect() : option.rect;
    const QRect visibleRect = viewportRect.intersected(option.rect);
    constexpr int kVisibleBlockBuffer = 96;
    const int visibleTop = qMax(0, visibleRect.top() - metrics.textRect.top() - kVisibleBlockBuffer);
    const int visibleBottom = qMin(markdownLayout.size.height(),
                                   visibleRect.bottom() - metrics.textRect.top() + kVisibleBlockBuffer);
    if (visibleBottom < 0 || visibleTop > markdownLayout.size.height()) {
        return;
    }

    const int firstRow = firstMarkdownBlockAtY(markdownLayout.blockOffsets,
                                               markdownLayout.blockHeights,
                                               visibleTop);
    const int lastRow = lastMarkdownBlockAtY(markdownLayout.blockOffsets, visibleBottom);
    if (firstRow < 0 || lastRow < firstRow || firstRow >= entry.blocks.size()) {
        return;
    }

    QVector<MarkdownBlockSelection> selections;
    if (m_selectionIndex == index && hasSelection()) {
        selections.resize(entry.blocks.size());
        const int selectionStart = m_selection.start();
        const int selectionEnd = m_selection.end();
        for (int row = firstRow; row <= qMin(lastRow, entry.blocks.size() - 1); ++row) {
            const int blockStart = entry.blockStartOffsets.value(row);
            const int blockEnd = blockStart + entry.blocks.at(row).text.size();
            const int localStart = qMax(selectionStart, blockStart) - blockStart;
            const int localEnd = qMin(selectionEnd, blockEnd) - blockStart;
            if (localEnd > localStart) {
                selections[row] = {localStart, localEnd};
            }
        }
    }

    QVector<bool> loadingRows;
    const bool isStreamingMessage = !m_streamingMessageId.isEmpty() &&
            index.data(AiChatMessageListModel::MessageIdRole).toString() == m_streamingMessageId;
    if (isStreamingMessage) {
        loadingRows.resize(entry.blocks.size());
        for (int row = 0; row < entry.blocks.size(); ++row) {
            const MarkdownRenderer::Block& block = entry.blocks.at(row);
            loadingRows[row] = block.type == MarkdownRenderer::BlockType::CodeBlock && block.open;
        }
    }

    MarkdownBlockModel markdownModel(&entry.blocks, selections, loadingRows);
    QVariant previousCopiedRowProperty;
    QVariant previousRevokedRowsProperty;
    QWidget* optionWidget = const_cast<QWidget*>(option.widget);
    if (optionWidget) {
        previousCopiedRowProperty = optionWidget->property("markdownCopiedCodeRow");
        const int copiedRow = m_copiedCodeMessageIndex == index ? m_copiedCodeBlockRow : -1;
        optionWidget->setProperty("markdownCopiedCodeRow", copiedRow);

        if (entry.hasSettingBlocks) {
            previousRevokedRowsProperty = optionWidget->property("markdownRevokedSettingRows");
            QVariantList revokedRows;
            const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
            const auto rowsIt = m_revokedSettingRowsByMessage.constFind(messageId);
            if (rowsIt != m_revokedSettingRowsByMessage.cend()) {
                revokedRows.reserve(rowsIt->size());
                for (int row : *rowsIt) {
                    revokedRows.push_back(row);
                }
            }
            optionWidget->setProperty("markdownRevokedSettingRows", revokedRows);
        }
    }

    for (int row = firstRow; row <= qMin(lastRow, entry.blocks.size() - 1); ++row) {
        QStyleOptionViewItem blockOption(option);
        blockOption.font = messageFont();
        blockOption.palette.setColor(QPalette::Text, textColor);
        blockOption.rect = QRect(metrics.textRect.left(),
                                 metrics.textRect.top() + markdownLayout.blockOffsets.at(row),
                                 metrics.textRect.width(),
                                 markdownLayout.blockHeights.at(row));
        blockOption.state &= ~(QStyle::State_Selected |
                               QStyle::State_MouseOver |
                               QStyle::State_HasFocus);
        blockOption.state |= QStyle::State_Enabled;

        const QModelIndex blockIndex = markdownModel.index(row, 0);
        m_markdownDelegate.paint(painter, blockOption, blockIndex);
    }

    if (optionWidget) {
        optionWidget->setProperty("markdownCopiedCodeRow", previousCopiedRowProperty);
        if (entry.hasSettingBlocks) {
            optionWidget->setProperty("markdownRevokedSettingRows", previousRevokedRowsProperty);
        }
    }
}

void AiChatMessageDelegate::paintUserMessageChrome(QPainter* painter,
                                                   const LayoutMetrics& metrics,
                                                   const QModelIndex& index,
                                                   const QColor& textColor) const
{
    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    if (metrics.userCanExpand && !metrics.expandRect.isEmpty()) {
        const qreal progress = userMessageExpansionProgress(messageId);
        const bool expanded = isUserMessageExpanded(messageId) && progress > 0.001;
        QColor controlColor = textColor;
        controlColor.setAlphaF(0.82);

        painter->save();
        painter->setPen(controlColor);
        painter->setFont(messageFont());
        const QString label = expanded ? QStringLiteral("收起") : QStringLiteral("展开");
        const QFontMetrics metricsFont(messageFont());
        const int labelWidth = metricsFont.horizontalAdvance(label);
        const QRect labelRect(metrics.expandRect.left(),
                              metrics.expandRect.top(),
                              qMin(labelWidth + 4, metrics.expandRect.width()),
                              metrics.expandRect.height());
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);

        const int chevronSize = 8;
        const int chevronLeft = labelRect.right() + 5;
        const int chevronTop = metrics.expandRect.top() + (metrics.expandRect.height() - chevronSize) / 2;
        if (chevronLeft + chevronSize <= metrics.expandRect.right()) {
            QPen pen(controlColor, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            QPainterPath chevron;
            if (expanded) {
                chevron.moveTo(chevronLeft, chevronTop + chevronSize * 0.64);
                chevron.lineTo(chevronLeft + chevronSize * 0.5, chevronTop + chevronSize * 0.36);
                chevron.lineTo(chevronLeft + chevronSize, chevronTop + chevronSize * 0.64);
            } else {
                chevron.moveTo(chevronLeft, chevronTop + chevronSize * 0.36);
                chevron.lineTo(chevronLeft + chevronSize * 0.5, chevronTop + chevronSize * 0.64);
                chevron.lineTo(chevronLeft + chevronSize, chevronTop + chevronSize * 0.36);
            }
            painter->drawPath(chevron);
        }
        painter->restore();
    }

    const qreal copyOpacity = userCopyButtonOpacity(messageId);
    if (copyOpacity <= 0.001 || metrics.copyButtonRect.isEmpty()) {
        return;
    }

    painter->save();
    painter->setOpacity(copyOpacity);
    drawActionIcon(painter,
                   QStringLiteral(":/resources/icon/copy.svg"),
                   metrics.copyButtonRect,
                   QSize(kActionIconSize, kActionIconSize),
                   ThemeManager::instance().isDark(),
                   false);
    painter->restore();
}

QString AiChatMessageDelegate::renderedPlainText(const QString& text, bool isFromUser) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (isFromUser) {
        return documentTextForLayout(text);
    }

    return cachedMarkdown(text).plainText;
#else
    return isFromUser ? documentTextForLayout(text) : cachedMarkdown(text).plainText;
#endif
}

QFont AiChatMessageDelegate::messageFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(14);
    return font;
}

int AiChatMessageDelegate::maxBubbleWidth(int itemWidth) const
{
    const int availableWidth = qMax(0, itemWidth - kHorizontalMargin * 2);
    return qMin(static_cast<int>(itemWidth * 0.72), availableWidth);
}

bool AiChatMessageDelegate::isAiReplyActionVisible(const QModelIndex& index,
                                                   MessageAction action) const
{
    if (!index.isValid() ||
            index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool() ||
            index.data(AiChatMessageListModel::IsFromUserRole).toBool()) {
        return false;
    }

    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    if (!messageId.isEmpty() && messageId == m_streamingMessageId) {
        return false;
    }

    if (action != MessageAction::Refresh) {
        return action == MessageAction::Copy ||
                action == MessageAction::Like ||
                action == MessageAction::Dislike;
    }

    const QAbstractItemModel* itemModel = index.model();
    if (!itemModel) {
        return false;
    }

    return index.row() == itemModel->rowCount() - 2;
}

QVector<QPair<AiChatMessageDelegate::MessageAction, QRect>>
AiChatMessageDelegate::aiReplyActionRects(const LayoutMetrics& metrics,
                                          const QModelIndex& index) const
{
    QVector<MessageAction> visibleActions;
    visibleActions.reserve(4);
    const QVector<MessageAction> orderedActions{
            MessageAction::Copy,
            MessageAction::Refresh,
            MessageAction::Like,
            MessageAction::Dislike
    };
    for (MessageAction action : orderedActions) {
        if (isAiReplyActionVisible(index, action)) {
            visibleActions.append(action);
        }
    }

    QVector<QPair<MessageAction, QRect>> rects;
    rects.reserve(visibleActions.size());
    const int contentLeft = metrics.textRect.left() + kMarkdownHorizontalInset;
    const int y = metrics.textRect.bottom() + 1 + kActionTopMargin;
    int x = contentLeft;
    for (MessageAction action : visibleActions) {
        rects.append(qMakePair(action, QRect(x, y, kActionButtonSize, kActionButtonSize)));
        x += kActionButtonSize + kActionGap;
    }
    return rects;
}

void AiChatMessageDelegate::paintAiReplyActions(QPainter* painter,
                                                const LayoutMetrics& metrics,
                                                const QModelIndex& index) const
{
    const QVector<QPair<MessageAction, QRect>> actions = aiReplyActionRects(metrics, index);
    if (actions.isEmpty()) {
        return;
    }

    const bool dark = ThemeManager::instance().isDark();
    const bool invertIcon = dark;
    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    const MessageFeedback feedback = messageFeedback(messageId);
    const QSize iconSize(kActionIconSize, kActionIconSize);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    for (const QPair<MessageAction, QRect>& action : actions) {
        const QRect buttonRect = action.second;
        QString iconSource;
        bool upsideDown = false;
        switch (action.first) {
        case MessageAction::Copy:
            iconSource = QStringLiteral(":/resources/icon/copy.svg");
            break;
        case MessageAction::Refresh:
            iconSource = QStringLiteral(":/resources/icon/refresh.svg");
            break;
        case MessageAction::Like:
            iconSource = feedback == MessageFeedback::Liked
                    ? QStringLiteral(":/resources/icon/liked.png")
                    : QStringLiteral(":/resources/icon/like.png");
            break;
        case MessageAction::Dislike:
            iconSource = feedback == MessageFeedback::Disliked
                    ? QStringLiteral(":/resources/icon/liked.png")
                    : QStringLiteral(":/resources/icon/like.png");
            upsideDown = true;
            break;
        case MessageAction::None:
            break;
        case MessageAction::ToggleExpansion:
            break;
        }

        if (!iconSource.isEmpty()) {
            drawActionIcon(painter, iconSource, buttonRect, iconSize, invertIcon, upsideDown);
        }
    }
    painter->restore();
}
