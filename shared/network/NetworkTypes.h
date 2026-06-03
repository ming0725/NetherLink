#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QUrl>
#include <QVariantMap>

enum class HttpMethod {
    Get,
    Post,
    Patch,
    Delete
};

struct BackendEnvironment {
    QUrl baseUrl = QUrl(QStringLiteral("http://localhost:8080"));
    QString apiPrefix = QStringLiteral("/api/v1");

    QUrl apiUrl(const QString& path, const QVariantMap& query = {}) const;
    QUrl realtimeUrl(const QString& accessToken,
                     qint64 lastEventSeq = 0,
                     const QString& lastEventId = {}) const;
};

struct NetworkRequest {
    HttpMethod method = HttpMethod::Get;
    QString path;
    QVariantMap query;
    QHash<QByteArray, QByteArray> headers;
    QJsonDocument body;
    QByteArray rawBody;
    QByteArray contentType;
    bool hasJsonBody = false;
    bool requiresAuth = true;
    bool expectsJson = true;
    int timeoutMs = 15000;
    int maxRetries = 3;

    static NetworkRequest json(HttpMethod method,
                               const QString& path,
                               const QJsonObject& body = {},
                               const QVariantMap& query = {});

    QByteArray verb() const;
    QByteArray idempotencyKey() const;
    bool hasIdempotencyHint() const;
    bool isWrite() const;
};

struct NetworkResponse {
    int httpStatus = 0;
    QHash<QByteArray, QByteArray> headers;
    QJsonDocument body;
    QByteArray rawBody;
    QString etag;
    QString requestId;

    QJsonObject object() const;
};

struct NetworkError {
    int httpStatus = 0;
    QString code;
    QString message;
    QString requestId;
    QJsonObject details;
    QByteArray rawBody;

    bool isTokenExpired() const;
    bool isAuthFailure() const;
    bool isRetryableServiceError() const;
};

struct RealtimeEvent {
    QString eventId;
    qint64 eventSeq = 0;
    QString type;
    QDateTime emittedAt;
    QJsonObject payload;
    QJsonObject raw;

    bool isControlEvent() const;
    bool requiresFullSync() const;
    static RealtimeEvent fromJson(const QJsonObject& object);
};

Q_DECLARE_METATYPE(NetworkResponse)
Q_DECLARE_METATYPE(NetworkError)
Q_DECLARE_METATYPE(RealtimeEvent)
Q_DECLARE_METATYPE(NetworkRequest)
