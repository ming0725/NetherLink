#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include "shared/network/NetworkTypes.h"

class SseClient;

class AiChatStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit AiChatStreamClient(QObject* parent = nullptr);

    bool isRunning() const;
    void start(const QString& conversationId,
               const QString& prompt,
               const QString& clientMessageId = {});
    void cancel();

signals:
    void chunkReceived(const QString& chunk);
    void assistantMessageReceived(const QString& messageId,
                                  const QString& text,
                                  const QDateTime& time);
    void finished();
    void failed(const NetworkError& error);

private:
    QString newClientMessageId() const;
    void handleStreamEvent(const QString& requestId,
                           const QString& eventName,
                           const QJsonObject& data);
    void handleStreamFinished(const QString& requestId);
    void handleStreamFailed(const QString& requestId, const NetworkError& error);
    static QJsonObject assistantMessageObject(QJsonObject data);
    static QString assistantMessageId(const QJsonObject& message);
    static QString assistantMessageText(const QJsonObject& message);
    static QDateTime assistantMessageTime(const QJsonObject& message);
    bool matchesActiveRequest(const QString& requestId) const;
    void clearActiveRequest();

    SseClient* m_sseClient = nullptr;
    QString m_requestId;
    QString m_clientMessageId;
    bool m_running = false;
};
