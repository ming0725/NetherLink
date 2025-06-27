#include "MessageRepository.h"
#include "UserRepository.h"
#include "GroupRepository.h"
#include "DatabaseManager.h"
#include "CurrentUser.h"
#include <algorithm>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
}

MessageRepository& MessageRepository::instance()
{
    static MessageRepository repo;
    return repo;
}

bool MessageRepository::ensureDatabaseOpen() {
    QString userId = CurrentUser::instance().getUserId();
    if (userId.isEmpty()) {
        qWarning() << "Current user ID is not set";
        return false;
    }
    return DatabaseManager::instance().initUserDatabase(userId);
}

QSharedPointer<ChatMessage> MessageRepository::createMessageFromQuery(QSqlQuery& query, bool isGroupChat) {
    QString content = query.value("content").toString();
    QString senderId = query.value("sender_id").toString();
    QDateTime timestamp = QDateTime::fromString(query.value("timestamp").toString(), Qt::ISODate);
    QString type = query.value("type").toString();
    QJsonDocument extraDoc = QJsonDocument::fromJson(query.value("extra").toString().toUtf8());
    QJsonObject extra = extraDoc.object();
    bool isFromMe = (senderId == CurrentUser::instance().getUserId());
    
    QString senderName;
    GroupRole role = GroupRole::Member;  // 默认角色
    
    // 根据消息类型获取发送者信息
    bool msgIsGroupChat = query.value("isGroupChat").toBool();
    if (msgIsGroupChat) {
        // 获取群组信息和成员信息
        QString groupId = query.value("conversation").toString();
        Group group = GroupRepository::instance().getGroup(groupId);
        bool foundMember = false;
        
        // 在群成员中查找发送者
        for (const GroupMember& member : group.members) {
            if (member.uid == senderId) {
                foundMember = true;
                // 优先使用群昵称，如果没有则使用用户名，如果用户名也为空则使用用户ID
                if (!member.nickname.isEmpty()) {
                    senderName = member.nickname;
                } else if (!member.name.isEmpty()) {
                    senderName = member.name;
                } else {
                    senderName = senderId;
                }
                
                // 设置角色
                if (senderId == group.ownerId) {
                    role = GroupRole::Owner;
                } else if (member.role == "admin") {
                    role = GroupRole::Admin;
                }
                break;
            }
        }
        
        // 如果在群成员中找不到发送者（可能是已退群的成员）
        if (!foundMember) {
            // 尝试从用户仓库获取基本信息
            auto user = UserRepository::instance().getUser(senderId);
            if (!user.nick.isEmpty()) {
                senderName = user.nick + "(已退群)";
            } else {
                senderName = senderId + "(已退群)";
            }
            role = GroupRole::Member;
            qDebug() << "Message sender" << senderId << "not found in group" << groupId;
        }
    } else {
        // 单聊消息，直接获取用户昵称
        auto user = UserRepository::instance().getUser(senderId);
        senderName = user.nick.isEmpty() ? senderId : user.nick;
    }
    
    QSharedPointer<ChatMessage> message;
    
    if (type == "text") {
        message = QSharedPointer<ChatMessage>(new TextMessage(
            content, isFromMe, senderId, msgIsGroupChat, senderName, role));
    } else if (type == "image") {
        QPixmap pixmap;
        QString localPath;
        if (extra.contains("local_path")) {
            localPath = extra["local_path"].toString();
            pixmap.load(localPath);
        }
        auto imageMsg = QSharedPointer<ImageMessage>(new ImageMessage(
            pixmap, isFromMe, senderId, msgIsGroupChat, senderName, role));
        imageMsg->setLocalPath(localPath);
        message = imageMsg;
    } else {
        message = QSharedPointer<ChatMessage>(new TextMessage(
            content, isFromMe, senderId, msgIsGroupChat, senderName, role));
    }
    
    message->setTimestamp(timestamp);
    return message;
}

QVector<QSharedPointer<ChatMessage>> MessageRepository::getMessages(const QString& conversationId, bool isGroupChat)
{
    QVector<QSharedPointer<ChatMessage>> messages;
    
    if (!ensureDatabaseOpen()) {
        return messages;
    }
    
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);
    query.prepare("SELECT * FROM message WHERE conversation = ? ORDER BY timestamp DESC LIMIT 20");
    query.addBindValue(conversationId);
    
    if (query.exec()) {
        while (query.next()) {
            QSharedPointer<ChatMessage> message = createMessageFromQuery(query, isGroupChat);
            messages.append(message);
        }
    } else {
        qWarning() << "Failed to get messages for conversation:" << conversationId 
                   << "Error:" << query.lastError().text();
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

QSharedPointer<ChatMessage> MessageRepository::getLastMessage(const QString& conversationId, bool isGroupChat)
{
    if (!ensureDatabaseOpen()) {
        return QSharedPointer<ChatMessage>(nullptr);
    }
    
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);
    query.prepare("SELECT * FROM message WHERE conversation = ? ORDER BY timestamp DESC LIMIT 1");
    query.addBindValue(conversationId);
    
    if (query.exec() && query.next()) {
        return createMessageFromQuery(query, isGroupChat);
    }
    
    return QSharedPointer<ChatMessage>(nullptr);
}

void MessageRepository::addMessage(const QString& conversationId,
                                   QSharedPointer<ChatMessage> message)
{
    if (!message) {
        return;
    }
    if (!ensureDatabaseOpen()) {
        qDebug() << "消息存入数据库失败，原因：数据库未打开！";
        return;
    }
    
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);
    
    // 生成唯一消息ID
    qint64 messageId = QDateTime::currentMSecsSinceEpoch();
    
    // 准备额外信息
    QJsonObject extra;
    if (message->getType() == MessageType::Image) {
        auto imageMsg = qSharedPointerCast<ImageMessage>(message);
        if (imageMsg) {
            // 保存图片的本地路径
            extra["local_path"] = imageMsg->getLocalPath();
        }
    }
    
    QString messageType;
    switch (message->getType()) {
        case MessageType::Text:
            messageType = "text";
            break;
        case MessageType::Image:
            messageType = "image";
            break;
        default:
            messageType = "text";
    }
    
    query.prepare("INSERT INTO message (id, conversation, sender_id, timestamp, type, content, extra, isGroupChat) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    
    query.addBindValue(messageId);
    query.addBindValue(conversationId);
    query.addBindValue(message->getSenderId());
    query.addBindValue(message->getTimestamp().toString(Qt::ISODate));
    query.addBindValue(messageType);
    query.addBindValue(message->getContent());
    query.addBindValue(QJsonDocument(extra).toJson(QJsonDocument::Compact));
    query.addBindValue(message->isInGroupChat());

    if (query.exec()) {
        emit lastMessageChanged(conversationId, message);
    } else {
        qWarning() << "Failed to add message:" << query.lastError().text();
    }
}

void MessageRepository::removeMessage(const QString& conversationId, int index, bool isGroupChat)
{
    if (!ensureDatabaseOpen()) {
        return;
    }
    
    // 首先获取该会话的所有消息
    QVector<QSharedPointer<ChatMessage>> messages = getMessages(conversationId, isGroupChat);
    
    // 检查索引是否有效
    if (index < 0 || index >= messages.size()) {
        qWarning() << "Invalid message index:" << index;
        return;
    }
    
    // 获取要删除的消息的时间戳，用于在数据库中定位
    QDateTime timestamp = messages[index]->getTimestamp();
    
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);
    query.prepare("DELETE FROM message WHERE conversation = ? AND timestamp = ?");
    query.addBindValue(conversationId);
    query.addBindValue(timestamp.toString(Qt::ISODate));
    
    if (query.exec()) {
        // 获取最新的消息并发出信号
        QSharedPointer<ChatMessage> lastMsg = getLastMessage(conversationId, isGroupChat);
        emit lastMessageChanged(conversationId, lastMsg);
    } else {
        qWarning() << "Failed to remove message:" << query.lastError().text();
    }
}
