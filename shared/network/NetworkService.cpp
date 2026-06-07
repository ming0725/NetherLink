#include "NetworkService.h"

#include "AuthSession.h"
#include "HttpClient.h"
#include "RealtimeClient.h"
#include "RemoteDataBootstrapper.h"
#include "UploadClient.h"
#include "shared/data/LocalDataStore.h"

namespace {

QString accountKeyForAuthResult(const AuthResult& result)
{
    return result.user.userUuid.isEmpty() ? result.user.userId : result.user.userUuid;
}

} // namespace

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
        HttpClient::instance().clearAuthenticationBlock();
        AuthSession::instance().setLoginAccountId(result.user.userId.isEmpty()
                                                  ? result.user.userUuid
                                                  : result.user.userId);
        LocalDataStore::instance().setActiveAccount(accountKeyForAuthResult(result));
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
                HttpClient::instance().clearAuthenticationBlock();
                AuthSession::instance().setLoginAccountId(result.user.userId.isEmpty()
                                                          ? result.user.userUuid
                                                          : result.user.userId);
                LocalDataStore::instance().setActiveAccount(accountKeyForAuthResult(result));
                emit registerSucceeded(requestId, result);
                RemoteDataBootstrapper::instance().syncAll();
                startRealtime();
            });
    connect(&AuthApiClient::instance(), &AuthApiClient::registerFailed, this, &NetworkService::registerFailed);
    connect(&AuthApiClient::instance(), &AuthApiClient::refreshSucceeded, this, [this](const QString& requestId,
                                                                                      const AuthTokenResult& result) {
        const RestoreContext context = m_pendingRestores.take(requestId);
        if (context.loginAccountId.isEmpty()) {
            return;
        }

        m_sessionExpiryNotified = false;
        HttpClient::instance().clearAuthenticationBlock();
        AuthSession::instance().setLoginAccountId(context.loginAccountId);
        LocalDataStore::instance().setActiveAccount(context.accountKey.isEmpty()
                                                    ? context.loginAccountId
                                                    : context.accountKey);
        emit sessionRestoreSucceeded(requestId, result);
        RemoteDataBootstrapper::instance().syncAll();
        startRealtime();
    });
    connect(&AuthApiClient::instance(), &AuthApiClient::refreshFailed, this, [this](const QString& requestId,
                                                                                   const NetworkError& error) {
        if (!m_pendingRestores.contains(requestId)) {
            return;
        }
        m_pendingRestores.remove(requestId);
        emit sessionRestoreFailed(requestId, error);
    });
    connect(&AuthApiClient::instance(), &AuthApiClient::logoutFinished, this, [this](const QString& requestId,
                                                                                    bool success,
                                                                                    const NetworkError& error) {
        emit logoutFinished(requestId, success, error);
    });
    connect(&RealtimeClient::instance(), &RealtimeClient::stateChanged, this, &NetworkService::realtimeStateChanged);
    connect(&RealtimeClient::instance(), &RealtimeClient::connectionError, this, &NetworkService::realtimeConnectionError);
    connect(&RealtimeClient::instance(), &RealtimeClient::sessionRevoked, this, [this](const QString&,
                                                                                      const QString& message) {
        const QString accountId = AuthSession::instance().loginAccountId();
        NetworkError error;
        error.httpStatus = 401;
        error.code = QStringLiteral("TOKEN_INVALID");
        error.message = message.isEmpty()
                ? QStringLiteral("账号已在其他设备登录，当前登录已下线")
                : message;
        m_sessionExpiryNotified = true;
        HttpClient::instance().blockAuthenticatedRequests(error);
        AuthSession::instance().clear();
        LocalDataStore::instance().clearActiveAccount();
        emit sessionRevoked(accountId, error.message);
    });
    connect(&HttpClient::instance(), &HttpClient::authRefreshFailed, this, [this](const NetworkError& error) {
        stopRealtime();
        LocalDataStore::instance().clearActiveAccount();
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

QString NetworkService::restoreSession(const QString& loginAccountId,
                                       const QString& accountKey,
                                       const QString& refreshToken)
{
    AuthSession::instance().setLoginAccountId(loginAccountId);
    const QString requestId = AuthApiClient::instance().refreshSession(refreshToken);
    m_pendingRestores.insert(requestId, {loginAccountId, accountKey});
    return requestId;
}

QString NetworkService::logout()
{
    const QString requestId = AuthApiClient::instance().logout();
    updatePresence(QStringLiteral("offline"));
    stopRealtime();
    AuthSession::instance().clear();
    LocalDataStore::instance().clearActiveAccount();
    return requestId;
}

void NetworkService::startRealtime()
{
    RealtimeClient::instance().connectToServer();
}

void NetworkService::stopRealtime()
{
    RealtimeClient::instance().disconnectFromServer();
}

void NetworkService::updatePresence(const QString& status)
{
    RealtimeClient::instance().sendPresenceUpdate(status);
}
