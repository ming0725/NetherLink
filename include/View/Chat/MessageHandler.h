/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_CHAT_MESSAGE_HANDLER
#define INCLUDE_VIEW_CHAT_MESSAGE_HANDLER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Entity/ChatMessage.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MessageHandler : public QObject {
    Q_OBJECT

    public:
        static MessageHandler& instance();

        void handleReceivedMessage(const QJsonObject& messageObj);

        void setCurrentChatId(const QString& id);

    signals:
        void messageReceived(QSharedPointer <ChatMessage> message);

        void unreadMessageReceived(const QString& conversationId, const QString& content, int unreadCount);

        void updateLastMessageTime(const QString& chatId, const QDateTime& timestamp);

    private:
        explicit MessageHandler(QObject* parent = nullptr);

        QString currentChatId; // 当前打开的聊天窗口ID

        QSharedPointer <ChatMessage> createMessage(const QJsonObject& payload);

        QDateTime parseTimestamp(const QString& timestamp);

};

#endif /* INCLUDE_VIEW_CHAT_MESSAGE_HANDLER */
