/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_CHAT_MESSAGE_LIST_ITEM

#define INCLUDE_VIEW_CHAT_MESSAGE_LIST_ITEM

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Data/AvatarLoader.h"
#include "Data/MessageData.h"
#include "NotificationBadge.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MessageListItem : public QWidget {
    Q_OBJECT

    public:
        explicit MessageListItem(const MessageDataPtr& data, QWidget* parent = nullptr);

        QSize sizeHint() const override;

        void setSelected(bool select);

        bool isSelected() const;

        const QString& getChatID() const {
            return (data->id);
        }

        const QDateTime& getLastTime() const {
            return (data->timestamp);
        }

        bool isGroup() const {
            return (data->isGroup);
        }

        MessageDataPtr getData() const {
            return (data);
        }

        void setData(MessageDataPtr msgData) {
            data = msgData;
        }

        void setUnreadCount(int count);

        int getUnreadCount() {
            return (badge->getCount());
        }

    signals:
        void itemClicked(MessageListItem*);

    protected:
        void resizeEvent(QResizeEvent*) override;

        void paintEvent(QPaintEvent*) override;

        void enterEvent(QEnterEvent*) override;

        void leaveEvent(QEvent*) override;

        void mousePressEvent(QMouseEvent*) override;

    private:
        void setupUI();

    private:
        AvatarLabel* avatarLabel;
        NotificationBadge* badge;
        MessageDataPtr data;
        bool selected = false;
        bool hovered = false;
        QRect avatarRect;
        QRect nameRect;
        QRect textRect;
        QRect timeRect;
        QRect badgeRect;
        static const int avatarSize = 48;
        static const int leftPad = 12;
        static const int spacing = 12;
        static const int between = 8;
};

#endif /* INCLUDE_VIEW_CHAT_MESSAGE_LIST_ITEM */
