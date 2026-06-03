#include "NetworkTypes.h"

#include <QJsonValue>
#include <QUrlQuery>

namespace {

QString normalizedPath(const QString& path)
{
    return path.startsWith(QLatin1Char('/')) ? path : QStringLiteral("/") + path;
}

void addQueryItems(QUrlQuery& urlQuery, const QVariantMap& query)
{
    for (auto it = query.constBegin(); it != query.constEnd(); ++it) {
        if (!it.value().isValid() || it.value().isNull()) {
            continue;
        }
        urlQuery.addQueryItem(it.key(), it.value().toString());
    }
}

} // namespace

QUrl BackendEnvironment::apiUrl(const QString& path, const QVariantMap& query) const
{
    QUrl url(baseUrl);
    const QString prefix = apiPrefix.startsWith(QLatin1Char('/')) ? apiPrefix : QStringLiteral("/") + apiPrefix;
    url.setPath(prefix + normalizedPath(path));

    QUrlQuery urlQuery(url);
    addQueryItems(urlQuery, query);
    url.setQuery(urlQuery);
    return url;
}

QUrl BackendEnvironment::realtimeUrl(const QString& accessToken,
                                     qint64 lastEventSeq,
                                     const QString& lastEventId) const
{
    QUrl url(baseUrl);
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    const QString prefix = apiPrefix.startsWith(QLatin1Char('/')) ? apiPrefix : QStringLiteral("/") + apiPrefix;
    url.setPath(prefix + QStringLiteral("/ws"));

    QUrlQuery query(url);
    if (!accessToken.isEmpty()) {
        query.addQueryItem(QStringLiteral("accessToken"), accessToken);
    }
    if (lastEventSeq > 0) {
        query.addQueryItem(QStringLiteral("lastEventSeq"), QString::number(lastEventSeq));
    }
    if (!lastEventId.isEmpty()) {
        query.addQueryItem(QStringLiteral("lastEventId"), lastEventId);
    }
    url.setQuery(query);
    return url;
}

NetworkRequest NetworkRequest::json(HttpMethod method,
                                    const QString& path,
                                    const QJsonObject& body,
                                    const QVariantMap& query)
{
    NetworkRequest request;
    request.method = method;
    request.path = path;
    request.query = query;
    if (method != HttpMethod::Get && !body.isEmpty()) {
        request.body = QJsonDocument(body);
        request.hasJsonBody = true;
    }
    request.headers.insert("Accept", "application/json");
    return request;
}

QByteArray NetworkRequest::verb() const
{
    switch (method) {
    case HttpMethod::Get:
        return "GET";
    case HttpMethod::Post:
        return "POST";
    case HttpMethod::Patch:
        return "PATCH";
    case HttpMethod::Delete:
        return "DELETE";
    }
    return "GET";
}

QByteArray NetworkRequest::idempotencyKey() const
{
    const QByteArray explicitHeader = headers.value("Idempotency-Key");
    if (!explicitHeader.isEmpty()) {
        return explicitHeader;
    }
    if (!body.isObject()) {
        return {};
    }
    const QJsonObject object = body.object();
    const QString clientOperationId = object.value(QStringLiteral("clientOperationId")).toString();
    if (!clientOperationId.isEmpty()) {
        return clientOperationId.toUtf8();
    }
    const QString clientMessageId = object.value(QStringLiteral("clientMessageId")).toString();
    return clientMessageId.toUtf8();
}

bool NetworkRequest::hasIdempotencyHint() const
{
    return !idempotencyKey().isEmpty();
}

bool NetworkRequest::isWrite() const
{
    return method == HttpMethod::Post || method == HttpMethod::Patch || method == HttpMethod::Delete;
}

QJsonObject NetworkResponse::object() const
{
    return body.isObject() ? body.object() : QJsonObject{};
}

bool NetworkError::isTokenExpired() const
{
    return code == QStringLiteral("TOKEN_EXPIRED");
}

bool NetworkError::isAuthFailure() const
{
    return httpStatus == 401 ||
           code == QStringLiteral("TOKEN_MISSING") ||
           code == QStringLiteral("TOKEN_INVALID") ||
           code == QStringLiteral("TOKEN_EXPIRED");
}

bool NetworkError::isRetryableServiceError() const
{
    return httpStatus == 502 ||
           httpStatus == 503 ||
           httpStatus == 504 ||
           code == QStringLiteral("SERVICE_NOT_READY") ||
           code == QStringLiteral("OPERATION_IN_PROGRESS");
}

bool RealtimeEvent::isControlEvent() const
{
    return eventSeq <= 0 ||
           type == QStringLiteral("realtime.ready") ||
           type == QStringLiteral("realtime.pong") ||
           type == QStringLiteral("event.replay") ||
           type == QStringLiteral("chat.message.sent") ||
           type == QStringLiteral("client.error");
}

bool RealtimeEvent::requiresFullSync() const
{
    return type == QStringLiteral("sync.required");
}

RealtimeEvent RealtimeEvent::fromJson(const QJsonObject& object)
{
    RealtimeEvent event;
    event.eventId = object.value(QStringLiteral("eventId")).toString();
    event.eventSeq = static_cast<qint64>(object.value(QStringLiteral("eventSeq")).toDouble());
    event.type = object.value(QStringLiteral("type")).toString();
    event.emittedAt = QDateTime::fromString(object.value(QStringLiteral("emittedAt")).toString(), Qt::ISODateWithMs);
    if (!event.emittedAt.isValid()) {
        event.emittedAt = QDateTime::fromString(object.value(QStringLiteral("emittedAt")).toString(), Qt::ISODate);
    }
    event.payload = object.value(QStringLiteral("payload")).toObject();
    event.raw = object;
    return event;
}
