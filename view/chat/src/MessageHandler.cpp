#include "MessageHandler.h"
#include "MessageRepository.h"
#include "UserRepository.h"
#include "GroupRepository.h"
#include "CurrentUser.h"
#include <QJsonDocument>
#include <QDebug>

MessageHandler& MessageHandler::instance()
{
    static MessageHandler handler;
    return handler;
}

MessageHandler::MessageHandler(QObject* parent)
    : QObject(parent)
{
}

void MessageHandler::handleReceivedMessage(const QJsonObject& messageObj)
{
    QJsonObject payload = messageObj["payload"].toObject();
    
    // 创建消息对象
    auto message = createMessage(payload);
    if (!message) {
        return;
    }

    // 获取会话ID
    QString fromId = payload["from"].toString();
    QString conversationId;
    bool isGroup = payload["is_group"].toBool();

    if (isGroup) {
        // 群聊消息，使用群ID
        conversationId = payload["conversation"].toString();
    } else {
        // 私聊消息，直接使用发送者ID
        conversationId = fromId;
    }

    qDebug() << "收到消息，会话ID:" << conversationId << "当前聊天ID:" << currentChatId;

    // 1. 保存到数据库
    MessageRepository::instance().addMessage(conversationId, message);

    // 2. 处理消息显示
    if (conversationId == currentChatId) {
        // 如果是当前聊天窗口，直接发送消息到ChatArea
        emit messageReceived(message);
        // 更新最后一条消息时间
        emit updateLastMessageTime(conversationId, message->getTimestamp());
    } else {
        // 如果不是当前窗口，更新未读消息计数和最后一条消息
        emit unreadMessageReceived(conversationId, message->getContent(), 1);
        emit updateLastMessageTime(conversationId, message->getTimestamp());
    }
}

QSharedPointer<ChatMessage> MessageHandler::createMessage(const QJsonObject& payload)
{
    QString content = payload["content"].toString();
    QString fromId = payload["from"].toString();
    QString type = payload["type"].toString();
    int gid = payload["conversation"].toString().toInt();
    bool isGroup = payload["is_group"].toBool();
    QDateTime timestamp = parseTimestamp(payload["timestamp"].toString());

    // 获取发送者信息
    auto user = UserRepository::instance().getUser(fromId);
    QString senderName = user.nick.isEmpty() ? fromId : user.nick;
    GroupRole role = GroupRepository::instance().getMemberRole(gid,fromId);
    QSharedPointer<ChatMessage> message;
    if (type == "text") {
        message = QSharedPointer<TextMessage>(new TextMessage(
            content,
            false,  // isFromMe
            fromId,
            isGroup,
            senderName,
            role
        ));
        message->setTimestamp(timestamp);
        return message;
    }
    // 其他类型的消息处理...
    return nullptr;
}

QDateTime MessageHandler::parseTimestamp(const QString& timestamp)
{
    // 解析ISO 8601格式的时间戳，包含时区信息
    QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
    if (dt.isValid()) {
        return dt.toLocalTime();  // 转换为本地时间
    }
    // 如果解析失败，返回当前时间
    qWarning() << "Failed to parse timestamp:" << timestamp;
    return QDateTime::currentDateTime();
}

void MessageHandler::setCurrentChatId(const QString& id)
{
    currentChatId = id;
} 