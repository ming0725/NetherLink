
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "AiChatItemDelegate.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>

#include "AiChatListModel.h"
#include "AvatarLoader.h"

#include "CurrentUser.h"

#include <QApplication>

#include <QClipboard>
#include <QPainter>

#include "NotificationManager.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
const QString docStyleSheet = "h1 { font-size: 20px; font-weight: bold; margin: 0; }""h2 { font-size: 18px; font-weight: bold; margin: 0; }""h3 { font-size: 16px; font-weight: bold; margin: 0; }""h4 { font-size: 15px; font-weight: bold; margin: 0; }""h5 { font-size: 14px; font-weight: bold; margin: 0; }""h6 { font-size: 13px; font-weight: bold; margin: 0; }""p { font-size: 14px; margin: 0; }""code { font-size: 10px; background: #f6f8fa; }""pre { font-family: monospace; font-size: 13px; }""blockquote { padding-left: calc(2ch); margin: 0; border-left: 3px solid #ccc; }""table { border-collapse: collapse; width: 100%; }""th, td { border: 1px solid #aaa; padding: 4px 8px; }""th { background: #eee; }";

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

AiChatItemDelegate::AiChatItemDelegate(QWidget* parent) : QStyledItemDelegate(parent), m_contextMenu(new TransparentMenu(parent)) {
    // 使用静态头像
    m_userAvatar = AvatarLoader::instance().getAvatar(CurrentUser::instance().getUserId());
    m_aiAvatar.load(":/icon/ds.png");

    // 缩放头像到指定大小
    if (!m_userAvatar.isNull()) {
        m_userAvatar = m_userAvatar.scaled(AVATAR_SIZE, AVATAR_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (!m_aiAvatar.isNull()) {
        m_aiAvatar = m_aiAvatar.scaled(AVATAR_SIZE, AVATAR_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 创建右键菜单
    QAction* copyAction = new QAction(QIcon(":/icon/copy.png"), "复制", this);

    connect(copyAction, &QAction::triggered, this, &AiChatItemDelegate::onCopyMessage);
    m_contextMenu->addAction(copyAction);
}

AiChatItemDelegate::~AiChatItemDelegate() {
    delete m_contextMenu;
}

void AiChatItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 获取数据
    QVariant data = index.data(Qt::UserRole);

    if (data.canConvert <AiChatMessage*>()) {
        // 绘制消息
        AiChatMessage* message = data.value <AiChatMessage*>();

        if (!message)
            return;

        // 计算气泡最大宽度（窗口宽度的70%）
        int maxBubbleWidth = option.rect.width() * 0.7;
        bool isFromMe = message->role() == AiChatMessage::User;

        // 绘制头像
        QRect avatarRect = calculateAvatarRect(option.rect, isFromMe);

        drawAvatar(painter, avatarRect, isFromMe);

        // 计算并绘制气泡
        QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, isFromMe);

        drawBubble(painter, bubbleRect, isFromMe, message);
    }
    painter->restore();
}

void AiChatItemDelegate::drawBubble(QPainter* painter, const QRect& rect, bool isFromMe, const AiChatMessage* message) const {
    // 设置气泡颜色
    QColor bubbleColor;

    if (isFromMe) {
        bubbleColor = message->isSelected() ? QColor(0, 120, 215) : QColor(0, 145, 255);
    } else {
        bubbleColor = QColor(0xffffff);  // AI消息默认白色背景

        if (message->isSelected()) {
            bubbleColor = QColor(220, 220, 220);  // AI消息选中时浅灰色背景
        }
    }
    painter->setBrush(bubbleColor);
    painter->setPen(Qt::NoPen);

    // 绘制气泡背景
    painter->drawRoundedRect(rect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    // 根据消息发送者选择不同的渲染方式
    drawTextMessage(painter, rect, message->content(), isFromMe);
}

void AiChatItemDelegate::drawTextMessage(QPainter* painter, const QRect& rect, const QString& text, bool isFromMe) const {
    QRect textRect = rect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING, -BUBBLE_PADDING, -BUBBLE_PADDING);

    if (isFromMe) {
        // 用户消息：使用普通文本渲染（与ChatItemDelegate一致）
        QTextOption option;

        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QColor textColor = Qt::white;  // 白色文字

        painter->setPen(textColor);

        QFont messageFont = QApplication::font();

        messageFont.setPixelSize(14);
        painter->setFont(messageFont);

        QTextLayout textLayout(text, messageFont);

        textLayout.setTextOption(option);

        // 计算文本布局
        qreal height = 0;

        textLayout.beginLayout();
        forever {QTextLine line = textLayout.createLine();

                 if (!line.isValid())
                     break;
                 line.setLineWidth(textRect.width());
                 line.setPosition(QPointF(0, height));
                 height += line.height();
        } textLayout.endLayout();

        // 绘制文本
        painter->save();
        painter->translate(textRect.left(), textRect.top());
        textLayout.draw(painter, QPointF(0, 0));
        painter->restore();
    } else {
        // AI消息：保持Markdown渲染
        QTextDocument doc;
        int spaceWidth = QFontMetrics(doc.defaultFont()).horizontalAdvance(' ');

        doc.setIndentWidth(spaceWidth * 2);

        // 设置默认样式表来控制Markdown渲染的字体大小
        doc.setDefaultStyleSheet(docStyleSheet);

        // 设置基础字体
        QFont font = QApplication::font();

        font.setPixelSize(14);
        doc.setDefaultFont(font);
        doc.setTextWidth(textRect.width());

        // 去除多余的空格和换行符
        QString processedText = text.trimmed();

        doc.setMarkdown(processedText);

        // 设置文本颜色
        QTextCursor cursor(&doc);

        cursor.select(QTextCursor::Document);

        QTextCharFormat format;

        format.setForeground(Qt::black);
        cursor.mergeCharFormat(format);

        // 绘制文本
        painter->save();
        painter->translate(textRect.topLeft());
        doc.drawContents(painter);
        painter->restore();
    }
}

void AiChatItemDelegate::drawAvatar(QPainter* painter, const QRect& rect, bool isFromMe) const {
    const QPixmap& avatar = isFromMe ? m_userAvatar : m_aiAvatar;

    if (!avatar.isNull()) {
        painter->drawPixmap(rect, avatar);
    }
}

QSize AiChatItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QVariant data = index.data(Qt::UserRole);

    if (data.canConvert <AiChatMessage*>()) {
        AiChatMessage* message = data.value <AiChatMessage*>();

        return (messageSizeHint(option, message));
    } else if (data.canConvert <int>()) {
        int height = data.toInt();

        return (QSize(option.rect.width(), height));
    }
    return (QSize());
}

QSize AiChatItemDelegate::messageSizeHint(const QStyleOptionViewItem& option, const AiChatMessage* message) const {
    if (!message)
        return (QSize());

    // 计算气泡最大宽度（窗口宽度的70%）
    int maxBubbleWidth = option.rect.width() * 0.7;
    bool isFromMe = message->role() == AiChatMessage::User;

    if (isFromMe) {
        // 用户消息：使用普通文本布局计算（与ChatItemDelegate一致）
        QFont messageFont = QApplication::font();

        messageFont.setPixelSize(14);

        QTextLayout textLayout(message->content(), messageFont);
        QTextOption textOption;

        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textLayout.setTextOption(textOption);

        qreal textHeight = 0;

        textLayout.beginLayout();
        forever {QTextLine line = textLayout.createLine();

                 if (!line.isValid())
                     break;
                 line.setLineWidth(maxBubbleWidth - 2 * BUBBLE_PADDING);
                 line.setPosition(QPointF(0, textHeight));
                 textHeight += line.height();
        } textLayout.endLayout();

        int bubbleHeight = qCeil(textHeight) + 2 * BUBBLE_PADDING;

        return (QSize(option.rect.width(), qMax(AVATAR_SIZE, bubbleHeight) + 2 * VERTICAL_MARGIN));
    } else {
        // AI消息：保持使用QTextDocument计算
        QTextDocument doc;
        int spaceWidth = QFontMetrics(doc.defaultFont()).horizontalAdvance(' ');

        doc.setIndentWidth(spaceWidth * 2);

        // 设置默认样式表来控制Markdown渲染的字体大小
        doc.setDefaultStyleSheet(docStyleSheet);

        QFont font = QApplication::font();

        font.setPixelSize(14);
        doc.setDefaultFont(font);

        // 去除多余的空格和换行符
        QString processedText = message->content().trimmed();

        doc.setMarkdown(processedText);

        // 先计算不限制宽度时的理想宽度
        doc.setTextWidth(-1);

        qreal idealWidth = doc.idealWidth();

        // 限制最大宽度，确保不超过允许的范围
        qreal finalWidth = qMin(idealWidth, static_cast <qreal>(maxBubbleWidth - 2 * BUBBLE_PADDING));

        doc.setTextWidth(finalWidth);

        QSizeF docSize = doc.size();
        int bubbleHeight = qCeil(docSize.height()) + 2 * BUBBLE_PADDING;

        return (QSize(option.rect.width(), qMax(AVATAR_SIZE, bubbleHeight) + 2 * VERTICAL_MARGIN));
    }
}

QRect AiChatItemDelegate::calculateBubbleRect(const QRect& contentRect, const AiChatMessage* message, int maxWidth, bool isFromMe) const {
    int bubbleWidth = 0;
    int bubbleHeight = 0;

    if (isFromMe) {
        // 用户消息：使用普通文本布局计算（与ChatItemDelegate一致）
        QFont messageFont = QApplication::font();

        messageFont.setPixelSize(14);

        QTextLayout textLayout(message->content(), messageFont);
        QTextOption textOption;

        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textLayout.setTextOption(textOption);

        qreal textHeight = 0;
        qreal textWidth = 0;

        textLayout.beginLayout();
        forever {QTextLine line = textLayout.createLine();

                 if (!line.isValid())
                     break;
                 line.setLineWidth(maxWidth - 2 * BUBBLE_PADDING);
                 line.setPosition(QPointF(0, textHeight));
                 textHeight += line.height();
                 textWidth = qMax(textWidth, line.naturalTextWidth());
        } textLayout.endLayout();
        bubbleWidth = qCeil(textWidth) + 2 * BUBBLE_PADDING;
        bubbleHeight = qCeil(textHeight) + 2 * BUBBLE_PADDING;
    } else {
        // AI消息：保持使用QTextDocument计算
        QTextDocument doc;
        int spaceWidth = QFontMetrics(doc.defaultFont()).horizontalAdvance(' ');

        doc.setIndentWidth(spaceWidth * 2);

        // 设置默认样式表来控制Markdown渲染的字体大小
        doc.setDefaultStyleSheet(docStyleSheet);

        QFont font = QApplication::font();

        font.setPixelSize(14);
        doc.setDefaultFont(font);

        // 去除多余的空格和换行符
        QString processedText = message->content().trimmed();

        doc.setMarkdown(processedText);

        // 先计算不限制宽度时的理想宽度
        doc.setTextWidth(-1);

        qreal idealWidth = doc.idealWidth();

        // 限制最大宽度，确保不超过允许的范围
        qreal finalWidth = qMin(idealWidth, static_cast <qreal>(maxWidth - 2 * BUBBLE_PADDING));

        doc.setTextWidth(finalWidth);

        QSizeF docSize = doc.size();

        bubbleWidth = qCeil(finalWidth) + 2 * BUBBLE_PADDING;
        bubbleHeight = qCeil(docSize.height()) + 2 * BUBBLE_PADDING;
    }

    // 确保气泡宽度不超过最大宽度
    bubbleWidth = qMin(bubbleWidth, maxWidth);

    // 计算气泡位置
    int x;

    if (isFromMe) {
        x = contentRect.right() - WINDOW_MARGIN - AVATAR_SIZE - AVATAR_BUBBLE_SPACING - bubbleWidth;
    } else {
        x = WINDOW_MARGIN + AVATAR_SIZE + AVATAR_BUBBLE_SPACING;
    }

    int y = contentRect.y() + VERTICAL_MARGIN;

    return (QRect(x, y, bubbleWidth, bubbleHeight));
}

QRect AiChatItemDelegate::calculateAvatarRect(const QRect& contentRect, bool isFromMe) const {
    int x;

    if (isFromMe) {
        // 用户头像：靠右对齐
        x = contentRect.right() - WINDOW_MARGIN - AVATAR_SIZE;
    } else {
        // AI头像：保持不变
        x = WINDOW_MARGIN;
    }

    int y = contentRect.y() + VERTICAL_MARGIN;

    return (QRect(x, y, AVATAR_SIZE, AVATAR_SIZE));
}

bool AiChatItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast <QMouseEvent*>(event);
        QVariant data = index.data(Qt::UserRole);

        // 如果是底部空白项，直接返回false让ListView处理
        if (data.canConvert <int>()) {
            return (false);
        }

        if (data.canConvert <AiChatMessage*>()) {
            AiChatMessage* message = data.value <AiChatMessage*>();

            if (!message)
                return (false);

            // 计算气泡区域
            int maxBubbleWidth = option.rect.width() * 0.7;
            bool isFromMe = message->role() == AiChatMessage::User;

            // 计算头像和气泡的区域
            QRect avatarRect = calculateAvatarRect(option.rect, isFromMe);
            QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, isFromMe);

            // 检查点击是否在气泡或头像内
            bool clickedInBubble = bubbleRect.contains(mouseEvent->pos());
            bool clickedInAvatar = avatarRect.contains(mouseEvent->pos());

            if (clickedInBubble) {
                // 点击在气泡内
                if ((mouseEvent->button() == Qt::LeftButton) || (mouseEvent->button() == Qt::RightButton)) {
                    static_cast <AiChatListModel*>(model)->setSingleSelection(index);

                    if (mouseEvent->button() == Qt::RightButton) {
                        m_selectedText = message->content();
                        m_contextMenu->popup(mouseEvent->globalPosition().toPoint());
                    }
                }
            } else if (!clickedInAvatar) {
                // 点击在非气泡非头像区域，清除选中
                static_cast <AiChatListModel*>(model)->clearSelection();
            }
            return (true);
        }
    }
    return (QStyledItemDelegate::editorEvent(event, model, option, index));
}

void AiChatItemDelegate::onCopyMessage() {
    if (!m_selectedText.isEmpty()) {
        NotificationManager::instance().showMessage("复制成功！", NotificationManager::Success);

        QClipboard* clipboard = QApplication::clipboard();

        clipboard->setText(m_selectedText);

        // 复制后清除选中状态
        if (QWidget* parentWidget = qobject_cast <QWidget*>(parent())) {
            if (QAbstractItemView* view = qobject_cast <QAbstractItemView*>(parentWidget)) {
                if (QAbstractItemModel* model = view->model()) {
                    static_cast <AiChatListModel*>(model)->clearSelection();
                }
            }
        }
        m_selectedText.clear(); // 清空已复制的文本
    }
}
