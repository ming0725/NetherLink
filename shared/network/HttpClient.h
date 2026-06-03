#pragma once

#include "NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QSharedPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class HttpClient : public QObject
{
    Q_OBJECT

public:
    static HttpClient& instance();

    BackendEnvironment environment() const;
    void setEnvironment(const BackendEnvironment& environment);

    QString send(const NetworkRequest& request);

public slots:
    void refreshAccessToken();

signals:
    void requestStarted(const QString& requestId, const NetworkRequest& request);
    void requestSucceeded(const QString& requestId, const NetworkResponse& response);
    void requestFailed(const QString& requestId, const NetworkError& error);
    void authRefreshSucceeded();
    void authRefreshFailed(const NetworkError& error);

private:
    explicit HttpClient(QObject* parent = nullptr);
    Q_DISABLE_COPY(HttpClient)

    struct Operation;

    void startOperation(const QSharedPointer<Operation>& operation);
    void handleReply(const QSharedPointer<Operation>& operation, QNetworkReply* reply);
    void retryLater(const QSharedPointer<Operation>& operation);
    void queueForTokenRefresh(const QSharedPointer<Operation>& operation);
    void flushRefreshQueue(bool success, const NetworkError& error = {});
    bool shouldRetry(const Operation& operation, const NetworkError& error) const;
    QNetworkRequest buildNetworkRequest(const NetworkRequest& request) const;
    NetworkResponse responseFromReply(QNetworkReply* reply) const;
    NetworkError errorFromReply(QNetworkReply* reply, const QByteArray& body) const;

    QNetworkAccessManager* m_manager = nullptr;
    BackendEnvironment m_environment;
    QHash<QString, QSharedPointer<Operation>> m_operations;
    QVector<QSharedPointer<Operation>> m_refreshQueue;
    bool m_refreshing = false;
};
