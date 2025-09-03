/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_AI_CHAT_AI_CHAT_ITEM_DELEGATE

#define INCLUDE_VIEW_AI_CHAT_AI_CHAT_ITEM_DELEGATE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QStyledItemDelegate>

#include "View/AiChat/AiChatMessage.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

    public:
        explicit AiChatItemDelegate(QWidget* parent = nullptr);

        ~AiChatItemDelegate();

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    private slots:
        void onCopyMessage();

    private:
        void drawBubble(QPainter* painter, const QRect& rect, bool isFromMe, const AiChatMessage* message) const;

        void drawTextMessage(QPainter* painter, const QRect& rect, const QString& text, bool isFromMe) const;

        void drawAvatar(QPainter* painter, const QRect& rect, bool isFromMe) const;

        QRect calculateBubbleRect(const QRect& contentRect, const AiChatMessage* message, int maxWidth, bool isFromMe) const;

        QRect calculateAvatarRect(const QRect& contentRect, bool isFromMe) const;

        QSize messageSizeHint(const QStyleOptionViewItem& option, const AiChatMessage* message) const;

        QPixmap m_userAvatar;
        QPixmap m_aiAvatar;
        QString m_selectedText;
        static constexpr int AVATAR_SIZE = 32;            // 头像大小
        static constexpr int WINDOW_MARGIN = 10; // 窗口边距
        static constexpr int AVATAR_BUBBLE_SPACING = 12; // 头像和气泡之间的间距
        static constexpr int VERTICAL_MARGIN = 10; // 垂直边距
        static constexpr int BUBBLE_PADDING = 12; // 气泡内边距
        static constexpr int BUBBLE_RADIUS = 10; // 气泡圆角
        static constexpr int MAX_BUBBLE_WIDTH = 600; // 气泡最大宽度
};

#endif /* INCLUDE_VIEW_AI_CHAT_AI_CHAT_ITEM_DELEGATE */
