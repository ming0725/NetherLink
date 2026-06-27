#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTimer>

#include "shared/network/NetworkTypes.h"
#include "shared/types/RepositoryTypes.h"

class SseClient;

class AiChatStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit AiChatStreamClient(QObject* parent = nullptr);

    bool isRunning() const;
    void start(const QString& conversationId,
               const QString& prompt,
               const QString& clientMessageId = {},
               const AiChatRequestOptions& options = {});
    void cancel();
    void abort();

signals:
    void streamStarted(const QString& streamId,
                       const QString& conversationId,
                       const QString& clientMessageId);
    void chunkReceived(const AiChatStreamChunk& chunk);
    void thinkingStateChanged(bool active);
    void streamStatusChanged(const AiChatStreamStatus& status);
    void titleReceived(const QString& title);
    void assistantMessageReceived(const AiChatMessage& message);
    void cancelled();
    void finished();
    void failed(const NetworkError& error);

private:
    QString newClientMessageId() const;
    void handleStreamEvent(const QString& requestId,
                           const QString& eventName,
                           const QJsonObject& data);
    void handleRealtimeEvent(const QString& type, const QJsonObject& payload);
    void handleStreamFinished(const QString& requestId);
    void handleStreamFailed(const QString& requestId, const NetworkError& error);
    void handleCancelTimeout();
    void processStreamChunk(const QJsonObject& data);
    void processTerminalEvent(const QJsonObject& data, bool isCancelled);
    void sendCancelCommand();
    void emitInactiveStreamStatus();
    static AiChatStreamChunk streamChunkFromObject(const QJsonObject& object);
    static AiChatStreamStatus streamStatusFromObject(const QJsonObject& object);
    static QJsonObject assistantMessageObject(QJsonObject data);
    static QString assistantMessageId(const QJsonObject& message);
    static QString assistantMessageText(const QJsonObject& message);
    static QDateTime assistantMessageTime(const QJsonObject& message);
    static AiChatTrace traceFromObject(const QJsonObject& object);
    static QString streamTitle(const QJsonObject& data);
    bool matchesActiveStreamEvent(const QJsonObject& payload) const;
    bool matchesActiveRealtimeEvent(const QJsonObject& payload) const;
    bool matchesActiveRequest(const QString& requestId) const;
    void clearActiveRequest();

    SseClient* m_sseClient = nullptr;
    QString m_requestId;
    QString m_streamId;
    QString m_clientMessageId;
    QTimer m_cancelTimeoutTimer;
    bool m_running = false;
    bool m_streamStatusActive = false;
    bool m_waitingForTerminalEvent = false;
};
