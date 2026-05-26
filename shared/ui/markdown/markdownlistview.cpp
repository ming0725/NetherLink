#include "markdownlistview.h"

#include "shared/ui/GlobalNotification.h"
#include "shared/theme/ThemeManager.h"

#include "markdowndelegate.h"
#include "markdowndocumentmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVariant>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include <utility>

namespace {

constexpr int kMinFontPixelSize = 10;
constexpr int kMaxFontPixelSize = 42;
constexpr int kWheelStepDelta = 120;
constexpr int kPixelWheelStepDelta = 40;

int effectiveFontPixelSize(const QFont &font)
{
    if (font.pixelSize() > 0) {
        return font.pixelSize();
    }
    return qMax(kMinFontPixelSize, QFontInfo(font).pixelSize());
}

ThemeManager::Mode modeFromValue(const QString &value, ThemeManager::Mode fallback)
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

} // namespace

MarkdownListView::MarkdownListView(QWidget *parent)
    : QListView(parent)
    , m_model(new MarkdownDocumentModel(this))
    , m_copyResetTimer(new QTimer(this))
{
    setModel(m_model);
    setItemDelegate(new MarkdownDelegate(this));
    setUniformItemSizes(false);
    setLayoutMode(QListView::Batched);
    setBatchSize(64);
    setWordWrap(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setCursor(Qt::ArrowCursor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setFrameShape(QFrame::NoFrame);
    setViewportMargins(0, 8, 0, 8);

    m_copyResetTimer->setSingleShot(true);
    connect(m_copyResetTimer, &QTimer::timeout, this, &MarkdownListView::clearCopiedCodeIndex);
}

void MarkdownListView::setMarkdown(const QString &markdown)
{
    m_model->setMarkdown(markdown);
    clearCopiedCodeIndex();
    m_revokedSettingRows.clear();
    syncRevokedSettingRowsProperty();
}

MarkdownDocumentModel *MarkdownListView::markdownModel() const
{
    return m_model;
}

void MarkdownListView::setMarkdownFontPixelSize(int pixelSize)
{
    const int boundedSize = qBound(kMinFontPixelSize, pixelSize, kMaxFontPixelSize);
    QFont nextFont = font();
    if (effectiveFontPixelSize(nextFont) == boundedSize && nextFont.pixelSize() == boundedSize) {
        return;
    }

    nextFont.setPixelSize(boundedSize);
    setFont(nextFont);
    doItemsLayout();
    viewport()->update();
}

void MarkdownListView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    doItemsLayout();
}

bool MarkdownListView::handleZoomWheelEvent(QWheelEvent *event)
{
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        m_zoomWheelDelta = 0;
        return false;
    }

    int delta = event->angleDelta().y();
    int stepDelta = kWheelStepDelta;
    if (delta == 0) {
        delta = event->pixelDelta().y();
        stepDelta = kPixelWheelStepDelta;
    }

    if (delta != 0) {
        m_zoomWheelDelta += delta;
        const int steps = m_zoomWheelDelta / stepDelta;
        m_zoomWheelDelta %= stepDelta;
        if (steps != 0) {
            setMarkdownFontPixelSize(effectiveFontPixelSize(font()) + steps);
        }
    } else if (event->phase() == Qt::ScrollBegin || event->phase() == Qt::ScrollEnd) {
        m_zoomWheelDelta = 0;
    }

    event->accept();
    return true;
}

bool MarkdownListView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Wheel && handleZoomWheelEvent(static_cast<QWheelEvent *>(event))) {
        return true;
    }

    return QListView::viewportEvent(event);
}

void MarkdownListView::wheelEvent(QWheelEvent *event)
{
    if (handleZoomWheelEvent(event)) {
        return;
    }

    QListView::wheelEvent(event);
}

void MarkdownListView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        if (m_model->hasSelection()) {
            QApplication::clipboard()->setText(m_model->selectedText());
        }
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::SelectAll)) {
        m_model->selectAll();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        m_model->clearSelection();
        event->accept();
        return;
    }

    QListView::keyPressEvent(event);
}

void MarkdownListView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QListView::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    QModelIndex copyHitIndex = indexAt(event->pos());
    if (copyHitIndex.isValid()) {
        const auto *markdownDelegate = qobject_cast<const MarkdownDelegate *>(itemDelegateForIndex(copyHitIndex));
        if (markdownDelegate &&
            markdownDelegate->isSettingActionButtonAtPosition(optionForIndex(copyHitIndex), copyHitIndex, event->pos())) {
            toggleSettingAction(copyHitIndex);
            event->accept();
            return;
        }

        if (markdownDelegate &&
            markdownDelegate->isCodeCopyButtonAtPosition(optionForIndex(copyHitIndex), copyHitIndex, event->pos())) {
            const QVariant value = copyHitIndex.data(MarkdownDocumentModel::BlockRole);
            if (value.canConvert<MarkdownRenderer::Block>()) {
                QApplication::clipboard()->setText(value.value<MarkdownRenderer::Block>().text);
                setCopiedCodeIndex(copyHitIndex);
                GlobalNotification::showSuccess(this, QStringLiteral("复制成功"));
            }
            event->accept();
            return;
        }
    }

    QModelIndex hitIndex;
    const QString link = linkAtViewportPosition(event->pos(), &hitIndex);
    if (!link.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromUserInput(link));
        event->accept();
        return;
    }

    if (!hasTextAtViewportPosition(event->pos(), &hitIndex)) {
        m_model->clearSelection();
        return;
    }

    const int cursor = cursorForViewportPosition(event->pos(), &hitIndex);
    m_anchorRow = hitIndex.row();
    m_anchorCursor = cursor;
    m_selecting = true;
    m_model->setSelectionRange(m_anchorRow, m_anchorCursor, m_anchorRow, m_anchorCursor);
    event->accept();
}

void MarkdownListView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting) {
        updateCursorForViewportPosition(event->pos());
        QListView::mouseMoveEvent(event);
        return;
    }

    QModelIndex hitIndex;
    const int cursor = cursorForViewportPosition(event->pos(), &hitIndex);
    if (hitIndex.isValid()) {
        m_model->setSelectionRange(m_anchorRow, m_anchorCursor, hitIndex.row(), cursor);
    }
    event->accept();
}

void MarkdownListView::leaveEvent(QEvent *event)
{
    viewport()->setCursor(Qt::ArrowCursor);
    QListView::leaveEvent(event);
}

void MarkdownListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        event->accept();
        return;
    }

    QListView::mouseReleaseEvent(event);
}

int MarkdownListView::cursorForViewportPosition(const QPoint &position, QModelIndex *hitIndex) const
{
    QModelIndex index = indexAt(position);
    if (!index.isValid() && m_model->rowCount() > 0) {
        index = position.y() < 0 ? m_model->index(0, 0) : m_model->index(m_model->rowCount() - 1, 0);
    }

    if (hitIndex) {
        *hitIndex = index;
    }

    if (!index.isValid()) {
        return 0;
    }

    const auto *markdownDelegate = qobject_cast<const MarkdownDelegate *>(itemDelegateForIndex(index));
    if (!markdownDelegate) {
        return 0;
    }

    return markdownDelegate->cursorForPosition(optionForIndex(index), index, position);
}

bool MarkdownListView::hasTextAtViewportPosition(const QPoint &position, QModelIndex *hitIndex) const
{
    const QModelIndex index = indexAt(position);
    if (hitIndex) {
        *hitIndex = index;
    }

    if (!index.isValid()) {
        return false;
    }

    const auto *markdownDelegate = qobject_cast<const MarkdownDelegate *>(itemDelegateForIndex(index));
    if (!markdownDelegate) {
        return false;
    }

    return markdownDelegate->hasTextAtPosition(optionForIndex(index), index, position);
}

QString MarkdownListView::linkAtViewportPosition(const QPoint &position, QModelIndex *hitIndex) const
{
    const QModelIndex index = indexAt(position);
    if (hitIndex) {
        *hitIndex = index;
    }

    if (!index.isValid()) {
        return {};
    }

    const auto *markdownDelegate = qobject_cast<const MarkdownDelegate *>(itemDelegateForIndex(index));
    if (!markdownDelegate) {
        return {};
    }

    return markdownDelegate->linkAtPosition(optionForIndex(index), index, position);
}

void MarkdownListView::updateCursorForViewportPosition(const QPoint &position)
{
    const QModelIndex hitIndex = indexAt(position);
    if (hitIndex.isValid()) {
        const auto *markdownDelegate = qobject_cast<const MarkdownDelegate *>(itemDelegateForIndex(hitIndex));
        if (markdownDelegate &&
            (markdownDelegate->isCodeCopyButtonAtPosition(optionForIndex(hitIndex), hitIndex, position) ||
             markdownDelegate->isSettingActionButtonAtPosition(optionForIndex(hitIndex), hitIndex, position))) {
            viewport()->setCursor(Qt::PointingHandCursor);
            return;
        }
    }

    if (!linkAtViewportPosition(position).isEmpty()) {
        viewport()->setCursor(Qt::PointingHandCursor);
        return;
    }

    viewport()->setCursor(hasTextAtViewportPosition(position) ? Qt::IBeamCursor : Qt::ArrowCursor);
}

QStyleOptionViewItem MarkdownListView::optionForIndex(const QModelIndex &index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    option.widget = this;
    option.index = index;
    return option;
}

void MarkdownListView::setCopiedCodeIndex(const QModelIndex &index)
{
    if (m_copiedCodeIndex.isValid()) {
        viewport()->update(visualRect(m_copiedCodeIndex));
    }

    m_copiedCodeIndex = QPersistentModelIndex(index);
    const int copiedRow = m_copiedCodeIndex.isValid() ? m_copiedCodeIndex.row() : -1;
    setProperty("markdownCopiedCodeRow", copiedRow);
    viewport()->setProperty("markdownCopiedCodeRow", copiedRow);
    if (m_copiedCodeIndex.isValid()) {
        viewport()->update(visualRect(m_copiedCodeIndex));
        m_copyResetTimer->start(3000);
    }
}

void MarkdownListView::clearCopiedCodeIndex()
{
    if (m_copyResetTimer) {
        m_copyResetTimer->stop();
    }

    const QPersistentModelIndex previousIndex = m_copiedCodeIndex;
    m_copiedCodeIndex = QPersistentModelIndex();
    setProperty("markdownCopiedCodeRow", -1);
    viewport()->setProperty("markdownCopiedCodeRow", -1);

    if (previousIndex.isValid()) {
        viewport()->update(visualRect(previousIndex));
    }
}

void MarkdownListView::toggleSettingAction(const QModelIndex &index)
{
    const QVariant value = index.data(MarkdownDocumentModel::BlockRole);
    if (!value.canConvert<MarkdownRenderer::Block>()) {
        return;
    }

    const MarkdownRenderer::Block block = value.value<MarkdownRenderer::Block>();
    if (block.type != MarkdownRenderer::BlockType::SettingBlock ||
        block.settingAction != QStringLiteral("settings.appearance.mode")) {
        return;
    }

    const bool revoked = m_revokedSettingRows.contains(index.row());
    if (revoked) {
        ThemeManager::instance().setMode(modeFromValue(block.settingValueText,
                                                       ThemeManager::Mode::Dark));
        m_revokedSettingRows.remove(index.row());
        GlobalNotification::showSuccess(this, QStringLiteral("已重新应用设置"));
    } else {
        ThemeManager::instance().setMode(modeFromValue(block.settingPreviousValueText,
                                                       ThemeManager::Mode::FollowSystem));
        m_revokedSettingRows.insert(index.row());
        GlobalNotification::showSuccess(this, QStringLiteral("已撤回设置"));
    }

    syncRevokedSettingRowsProperty();
    viewport()->update(visualRect(index));
}

void MarkdownListView::syncRevokedSettingRowsProperty()
{
    QVariantList rows;
    rows.reserve(m_revokedSettingRows.size());
    for (int row : std::as_const(m_revokedSettingRows)) {
        rows.push_back(row);
    }

    setProperty("markdownRevokedSettingRows", rows);
    viewport()->setProperty("markdownRevokedSettingRows", rows);
}
