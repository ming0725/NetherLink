#pragma once

#include "NetworkTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

namespace NetworkLog {

void installApplicationMessageHandler();
QString logFilePath();

void httpRequest(const QString& requestId,
                 const NetworkRequest& request,
                 const QUrl& url,
                 int attempt);
void httpResponse(const QString& requestId,
                  const NetworkRequest& request,
                  const NetworkResponse& response,
                  qint64 elapsedMs);
void httpError(const QString& requestId,
               const NetworkRequest& request,
               const NetworkError& error,
               qint64 elapsedMs);
void httpRetry(const QString& requestId, int nextAttempt, int delayMs);
void authRefreshQueued(int queuedCount);

void sseRequest(const QString& requestId, const NetworkRequest& request, const QUrl& url);
void sseEvent(const QString& requestId, const QString& eventName, const QJsonObject& payload);
void sseFinished(const QString& requestId, int httpStatus);
void sseError(const QString& requestId, const NetworkError& error);

void realtimeState(const QString& state);
void realtimeOpening(const QUrl& url, const QString& state, int attempt, qint64 lastEventSeq, const QString& lastEventId);
void realtimeConnected(const QUrl& url);
void realtimeClosed(const QString& reason,
                    const QUrl& url,
                    int closeCode = 0,
                    const QString& closeReason = {},
                    const QString& socketError = {},
                    int httpStatus = 0);
void realtimeError(int socketError, int httpStatus, const QString& message, const QUrl& url);
void realtimeReconnect(int attempt, int delayMs);
void realtimeSend(const QJsonObject& message, const QUrl& url);
void realtimeSendSkipped(const QJsonObject& message, int socketState, const QUrl& url);
void realtimeInvalidJson(const QString& error, qsizetype messageSize);
void realtimeEvent(const RealtimeEvent& event);
void realtimeClientError(const QString& code, const QString& message);

QString redactedUrl(QUrl url);

} // namespace NetworkLog
