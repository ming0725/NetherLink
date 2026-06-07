#include "NotificationRemoteDataSource.h"

#include "shared/network/HttpClient.h"

#include <QJsonObject>

NotificationRemoteDataSource& NotificationRemoteDataSource::instance()
{
    static NotificationRemoteDataSource dataSource;
    return dataSource;
}

NotificationRemoteDataSource::NotificationRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &NotificationRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &NotificationRemoteDataSource::handleRequestFailed);
}

QString NotificationRemoteDataSource::markAllRead(const QString& type)
{
    QJsonObject body;
    if (!type.isEmpty()) {
        body.insert(QStringLiteral("type"), type);
    }

    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/notifications/read-all"),
                                                  body);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingTypes.insert(requestId, type);
    return requestId;
}

void NotificationRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse&)
{
    if (!m_pendingTypes.contains(requestId)) {
        return;
    }

    emit markAllReadSucceeded(requestId, m_pendingTypes.take(requestId));
}

void NotificationRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingTypes.contains(requestId)) {
        return;
    }

    emit markAllReadFailed(requestId, m_pendingTypes.take(requestId), error);
}
