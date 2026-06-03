#include "AuthApiClient.h"

#include "AuthSession.h"
#include "HttpClient.h"

#include <QJsonObject>

namespace {

AuthUser authUserFromObject(const QJsonObject& object)
{
    AuthUser user;
    user.userUuid = object.value(QStringLiteral("userUuid")).toString();
    user.userId = object.value(QStringLiteral("userId")).toString(user.userUuid);
    user.nickName = object.value(QStringLiteral("nickName")).toString(user.userId);
    user.avatarPath = object.value(QStringLiteral("avatarPath")).toString();
    user.signature = object.value(QStringLiteral("signature")).toString();
    user.region = object.value(QStringLiteral("region")).toString();
    user.status = object.value(QStringLiteral("status")).toString();
    user.version = object.value(QStringLiteral("version")).toInt();
    user.etag = object.value(QStringLiteral("etag")).toString();
    return user;
}

NetworkRequest authRequest(const QString& path, const QJsonObject& body)
{
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post, path, body);
    request.requiresAuth = false;
    request.maxRetries = 0;
    return request;
}

} // namespace

AuthApiClient& AuthApiClient::instance()
{
    static AuthApiClient client;
    return client;
}

AuthApiClient::AuthApiClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<AuthResult>("AuthResult");

    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &AuthApiClient::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &AuthApiClient::handleRequestFailed);
}

QString AuthApiClient::login(const QString& account, const QString& password)
{
    const QJsonObject body{
            {QStringLiteral("account"), account},
            {QStringLiteral("password"), password},
            {QStringLiteral("deviceId"), AuthSession::instance().deviceId()},
            {QStringLiteral("deviceName"), AuthSession::instance().deviceName()}
    };
    const QString requestId = HttpClient::instance().send(authRequest(QStringLiteral("/auth/login"), body));
    m_pendingRequests.insert(requestId, RequestKind::Login);
    return requestId;
}

QString AuthApiClient::registerAccount(const QString& email,
                                       const QString& password,
                                       const QString& displayName,
                                       const QString& userId)
{
    const QJsonObject body{
            {QStringLiteral("email"), email},
            {QStringLiteral("password"), password},
            {QStringLiteral("displayName"), displayName},
            {QStringLiteral("userId"), userId},
            {QStringLiteral("gender"), QStringLiteral("unspecified")},
            {QStringLiteral("deviceId"), AuthSession::instance().deviceId()},
            {QStringLiteral("deviceName"), AuthSession::instance().deviceName()}
    };
    const QString requestId = HttpClient::instance().send(authRequest(QStringLiteral("/auth/register"), body));
    m_pendingRequests.insert(requestId, RequestKind::Register);
    return requestId;
}

QString AuthApiClient::logout()
{
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/auth/logout"),
            {{QStringLiteral("refreshToken"), AuthSession::instance().refreshToken()}});
    request.maxRetries = 0;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingRequests.insert(requestId, RequestKind::Logout);
    return requestId;
}

void AuthApiClient::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }

    const RequestKind kind = m_pendingRequests.take(requestId);
    if (kind == RequestKind::Logout) {
        emit logoutFinished(requestId, true, {});
        return;
    }

    const AuthResult result = authResultFromObject(response.object());
    if (!result.isValid()) {
        NetworkError error;
        error.httpStatus = response.httpStatus;
        error.code = QStringLiteral("AUTH_RESPONSE_INVALID");
        error.message = QStringLiteral("Auth response is missing required fields.");
        error.requestId = response.requestId;
        if (kind == RequestKind::Login) {
            emit loginFailed(requestId, error);
        } else {
            emit registerFailed(requestId, error);
        }
        return;
    }

    AuthSession::instance().setTokens(result.accessToken, result.refreshToken, result.expiresIn);
    if (kind == RequestKind::Login) {
        emit loginSucceeded(requestId, result);
    } else {
        emit registerSucceeded(requestId, result);
    }
}

void AuthApiClient::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }

    const RequestKind kind = m_pendingRequests.take(requestId);
    switch (kind) {
    case RequestKind::Login:
        emit loginFailed(requestId, error);
        break;
    case RequestKind::Register:
        emit registerFailed(requestId, error);
        break;
    case RequestKind::Logout:
        emit logoutFinished(requestId, false, error);
        break;
    }
}

AuthResult AuthApiClient::authResultFromObject(const QJsonObject& object) const
{
    AuthResult result;
    result.user = authUserFromObject(object.value(QStringLiteral("user")).toObject());
    result.preferences = object.value(QStringLiteral("preferences")).toObject();
    result.accessToken = object.value(QStringLiteral("accessToken")).toString();
    result.refreshToken = object.value(QStringLiteral("refreshToken")).toString();
    result.expiresIn = object.value(QStringLiteral("expiresIn")).toInt(900);
    return result;
}
