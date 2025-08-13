// MessageListWidget.h

#ifndef INCLUDE_VIEW_CHAT_MESSAGE_LIST_WIDGET

#define INCLUDE_VIEW_CHAT_MESSAGE_LIST_WIDGET

/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Components/CustomScrollArea.h"
#include "View/Chat/MessageListItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MessageListWidget : public CustomScrollArea {
    Q_OBJECT

    public:
        explicit MessageListWidget(QWidget* parent = nullptr);

        void addMessage(const MessageDataPtr& data);

        void clearMessages();

        MessageListItem* getSelectedItem() const {
            return (selectItem);
        }

        void updateMessages();

    signals:
        void itemClicked(MessageListItem* item);

    protected:
        void layoutContent() override;

        using CustomScrollArea::contentWidget;

    private slots:
        void onItemClicked(MessageListItem*);

        void onUnreadMessageReceived(const QString& conversationId, const QString& content, int unreadCount);

        void onLastMessageTimeUpdated(const QString& chatId, const QDateTime& timestamp);

    private:
        MessageListItem* findItemById(const QString& id);

        QVector <MessageListItem*> m_items;
        MessageListItem* selectItem = nullptr;
};

#endif /* INCLUDE_VIEW_CHAT_MESSAGE_LIST_WIDGET */
