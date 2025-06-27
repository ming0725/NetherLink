#pragma once

#include <QString>
#include <QDateTime>
#include <QSharedPointer>
#include <QUrl>

// 消息类型枚举
enum class MessageType {
    Text,
    Image,
    TimeHeader
};

// 基础消息数据结构
struct MessageData {
    QString id;              // 消息ID
    QString senderId;        // 发送者ID
    QString senderName;      // 发送者名称
    QString content;         // 消息内容
    QDateTime timestamp;     // 时间戳
    MessageType type;        // 消息类型
    bool isGroup;           // 是否群消息
    QUrl avatarUrl;         // 头像URL
    bool isRead;            // 是否已读
    int unreadCount;        // 未读数（用于会话列表）
    
    MessageData() : type(MessageType::Text), isGroup(false), isRead(true), unreadCount(0) {}
    virtual ~MessageData() = default;
};

using MessageDataPtr = QSharedPointer<MessageData>; 