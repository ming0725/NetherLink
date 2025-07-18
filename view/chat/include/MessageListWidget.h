// MessageListWidget.h
#pragma once
#include "CustomScrollArea.h"
#include "MessageListItem.h"
#include <QDateTime>
#include <QVector>

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
