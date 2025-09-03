/* guard ------------------------------------------------------------------ 80 // ! ---------------------------- 120 */

#ifndef INCLUDE_DATA_MESSAGE_REPOSITORY

#define INCLUDE_DATA_MESSAGE_REPOSITORY

/* include ---------------------------------------------------------------- 80 // ! ---------------------------- 120 */

#include <QSqlQuery>

#include "Entity/ChatMessage.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MessageRepository : public QObject {
    Q_OBJECT

    public:
        static MessageRepository& instance();

        // 获取某会话的全部消息
        QVector <QSharedPointer <ChatMessage> > getMessages(const QString&, bool);

        // 获取某会话的最后一条消息（可能为 nullptr）
        QSharedPointer <ChatMessage> getLastMessage(const QString&, bool);

    public slots:
        // 添加一条消息到会话（单聊或群聊），会发 lastMessageChanged
        void addMessage(const QString& conversationId, QSharedPointer <ChatMessage> message);

        // 删除某会话中索引为 index 的消息，之后发 lastMessageChanged
        void removeMessage(const QString& conversationId, int index, bool isGroupChat);

    signals:
        // conversationId 对应的最后一条消息已更新（nullptr 表示已无消息）
        void lastMessageChanged(const QString& conversationId, QSharedPointer <ChatMessage> lastMessage);

    private:
        explicit MessageRepository(QObject* parent = nullptr);

        Q_DISABLE_COPY(MessageRepository)

        // 确保数据库连接已打开
        bool ensureDatabaseOpen();

        // 创建消息对象
        QSharedPointer <ChatMessage> createMessageFromQuery(QSqlQuery&, bool);
};

#endif /* INCLUDE_DATA_MESSAGE_REPOSITORY */
