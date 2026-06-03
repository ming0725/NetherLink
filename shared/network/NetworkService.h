#pragma once

#include "AuthApiClient.h"
#include "NetworkTypes.h"
#include "RealtimeClient.h"

#include <QObject>
#include <QString>

class NetworkService : public QObject
{
    Q_OBJECT

public:
    static NetworkService& instance();

    BackendEnvironment environment() const;
    void configure(const BackendEnvironment& environment);

    QString login(const QString& account, const QString& password);
    QString registerAccount(const QString& email,
                            const QString& password,
                            const QString& displayName,
                            const QString& userId);
    QString logout();

    void startRealtime();
    void stopRealtime();

signals:
    void loginSucceeded(const QString& requestId, const AuthResult& result);
    void loginFailed(const QString& requestId, const NetworkError& error);
    void registerSucceeded(const QString& requestId, const AuthResult& result);
    void registerFailed(const QString& requestId, const NetworkError& error);
    void logoutFinished(const QString& requestId, bool success, const NetworkError& error);
    void realtimeStateChanged(RealtimeClient::State state);
    void realtimeConnectionError(const QString& message);
    void sessionExpired(const NetworkError& error);

private:
    explicit NetworkService(QObject* parent = nullptr);
    Q_DISABLE_COPY(NetworkService)

    BackendEnvironment m_environment;
    bool m_sessionExpiryNotified = false;
};
