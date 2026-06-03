#include "NetworkService.h"

#include "AuthSession.h"
#include "HttpClient.h"
#include "RealtimeClient.h"
#include "RemoteDataBootstrapper.h"
#include "UploadClient.h"

NetworkService& NetworkService::instance()
{
    static NetworkService service;
    return service;
}

NetworkService::NetworkService(QObject* parent)
    : QObject(parent)
{
    configure(m_environment);

    connect(&AuthApiClient::instance(), &AuthApiClient::loginSucceeded, this, [this](const QString& requestId,
                                                                                    const AuthResult& result) {
        m_sessionExpiryNotified = false;
        emit loginSucceeded(requestId, result);
        RemoteDataBootstrapper::instance().syncAll();
        startRealtime();
    });
    connect(&AuthApiClient::instance(), &AuthApiClient::loginFailed, this, &NetworkService::loginFailed);
    connect(&AuthApiClient::instance(),
            &AuthApiClient::registerSucceeded,
            this,
            [this](const QString& requestId, const AuthResult& result) {
                m_sessionExpiryNotified = false;
                emit registerSucceeded(requestId, result);
                RemoteDataBootstrapper::instance().syncAll();
                startRealtime();
            });
    connect(&AuthApiClient::instance(), &AuthApiClient::registerFailed, this, &NetworkService::registerFailed);
    connect(&AuthApiClient::instance(), &AuthApiClient::logoutFinished, this, [this](const QString& requestId,
                                                                                    bool success,
                                                                                    const NetworkError& error) {
        stopRealtime();
        AuthSession::instance().clear();
        emit logoutFinished(requestId, success, error);
    });
    connect(&RealtimeClient::instance(), &RealtimeClient::stateChanged, this, &NetworkService::realtimeStateChanged);
    connect(&RealtimeClient::instance(), &RealtimeClient::connectionError, this, &NetworkService::realtimeConnectionError);
    connect(&HttpClient::instance(), &HttpClient::authRefreshFailed, this, [this](const NetworkError& error) {
        stopRealtime();
        if (m_sessionExpiryNotified) {
            return;
        }
        m_sessionExpiryNotified = true;
        emit sessionExpired(error);
    });
}

BackendEnvironment NetworkService::environment() const
{
    return m_environment;
}

void NetworkService::configure(const BackendEnvironment& environment)
{
    m_environment = environment;
    HttpClient::instance().setEnvironment(environment);
    RealtimeClient::instance().setEnvironment(environment);
    UploadClient::instance().setEnvironment(environment);
}

QString NetworkService::login(const QString& account, const QString& password)
{
    return AuthApiClient::instance().login(account, password);
}

QString NetworkService::registerAccount(const QString& email,
                                        const QString& password,
                                        const QString& displayName,
                                        const QString& userId)
{
    return AuthApiClient::instance().registerAccount(email, password, displayName, userId);
}

QString NetworkService::logout()
{
    return AuthApiClient::instance().logout();
}

void NetworkService::startRealtime()
{
    RealtimeClient::instance().connectToServer();
}

void NetworkService::stopRealtime()
{
    RealtimeClient::instance().disconnectFromServer();
}
