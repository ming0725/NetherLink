#pragma once

#include <QObject>
#include <QJsonObject>
#include <QDateTime>
#include "ChatMessage.h"
#include "MessageListItem.h"

class MessageHandler : public QObject
{
    Q_OBJECT
public:
    static MessageHandler& instance();
    void handleReceivedMessage(const QJsonObject& messageObj);
    void setCurrentChatId(const QString& id);
signals:
    void messageReceived(QSharedPointer<ChatMessage> message);
    void unreadMessageReceived(const QString& conversationId, const QString& content, int unreadCount);
    void updateLastMessageTime(const QString& chatId, const QDateTime& timestamp);
private:
    explicit MessageHandler(QObject* parent = nullptr);
    QString currentChatId;  // 当前打开的聊天窗口ID
    QSharedPointer<ChatMessage> createMessage(const QJsonObject& payload);
    QDateTime parseTimestamp(const QString& timestamp);
}; 