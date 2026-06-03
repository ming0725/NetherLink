#pragma once

#include "NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class RemoteDataBootstrapper : public QObject
{
    Q_OBJECT

public:
    struct FetchSpec {
        QString domain;
        QString path;
        QString arrayKey;
        QStringList keyFields;
        QVariantMap query;
        bool clearBeforeStore = true;
        bool paged = false;
    };

    static RemoteDataBootstrapper& instance();

    bool isRunning() const;
    void syncAll();

signals:
    void syncStarted();
    void domainSynced(const QString& domain, int itemCount);
    void domainSyncFailed(const QString& domain, const NetworkError& error);
    void syncFinished(bool success);

private:
    explicit RemoteDataBootstrapper(QObject* parent = nullptr);
    Q_DISABLE_COPY(RemoteDataBootstrapper)

    void enqueue(const FetchSpec& spec);
    void handleSuccess(const QString& requestId, const NetworkResponse& response);
    void handleFailure(const QString& requestId, const NetworkError& error);
    void finishRequest(const QString& requestId);
    void cacheResponse(const FetchSpec& spec, const NetworkResponse& response, int* itemCount);
    void enqueueNextPageIfNeeded(const FetchSpec& spec, const QJsonObject& root, int itemCount);

    QHash<QString, FetchSpec> m_requests;
    QSet<QString> m_activeDomains;
    bool m_running = false;
    bool m_failed = false;
};
