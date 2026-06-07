#include "AuthApiClient.h"

#include "AuthSession.h"
#include "HttpClient.h"
#include "shared/services/AvatarSource.h"

#include <QJsonObject>

namespace {

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString avatarFileIdFrom(const QJsonObject& object)
{
    const QJsonObject avatar = object.value(QStringLiteral("avatar")).toObject();
    const QString nestedFileId = firstString(avatar, {QStringLiteral("fileId"), QStringLiteral("id")});
    if (!nestedFileId.isEmpty()) {
        return nestedFileId;
    }

    return firstString(object, {QStringLiteral("avatarFileId"),
                                QStringLiteral("avatar_file_id"),
                                QStringLiteral("fileId")});
}

AuthUser authUserFromObject(const QJsonObject& object)
{
    const QJsonObject presence = object.value(QStringLiteral("presence")).toObject();
    AuthUser user;
    user.userUuid = object.value(QStringLiteral("userUuid")).toString();
    user.userId = object.value(QStringLiteral("userId")).toString(user.userUuid);
    user.nickName = object.value(QStringLiteral("nickName")).toString(user.userId);
    user.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    user.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    user.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = object.value(QStringLiteral("avatarPath")).toString(
                object.value(QStringLiteral("avatarUrl")).toString());
    }
    user.avatarPath = AvatarSource::versioned(
            avatarPath,
            user.avatarVersion,
            user.avatarEtag,
            user.avatarContentHash);
    user.signature = object.value(QStringLiteral("signature")).toString();
    user.region = object.value(QStringLiteral("region")).toString();
    user.status = presence.value(QStringLiteral("status")).toString(
            object.value(QStringLiteral("status")).toString());
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
    qRegisterMetaType<AuthTokenResult>("AuthTokenResult");

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

QString AuthApiClient::refreshSession(const QString& refreshToken)
{
    const QJsonObject body{
            {QStringLiteral("refreshToken"), refreshToken},
            {QStringLiteral("deviceId"), AuthSession::instance().deviceId()}
    };
    const QString requestId = HttpClient::instance().send(authRequest(QStringLiteral("/auth/refresh"), body));
    m_pendingRequests.insert(requestId, RequestKind::Refresh);
    return requestId;
}

QString AuthApiClient::logout()
{
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/auth/logout"),
            {{QStringLiteral("refreshToken"), AuthSession::instance().refreshToken()}});
    request.requiresAuth = false;
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

    if (kind == RequestKind::Refresh) {
        const AuthTokenResult result = authTokenResultFromObject(response.object());
        if (!result.isValid()) {
            NetworkError error;
            error.httpStatus = response.httpStatus;
            error.code = QStringLiteral("AUTH_REFRESH_RESPONSE_INVALID");
            error.message = QStringLiteral("Refresh response is missing required fields.");
            error.requestId = response.requestId;
            emit refreshFailed(requestId, error);
            return;
        }

        AuthSession::instance().setTokens(result.accessToken, result.refreshToken, result.expiresIn);
        emit refreshSucceeded(requestId, result);
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
    case RequestKind::Refresh:
        emit refreshFailed(requestId, error);
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

AuthTokenResult AuthApiClient::authTokenResultFromObject(const QJsonObject& object) const
{
    AuthTokenResult result;
    result.accessToken = object.value(QStringLiteral("accessToken")).toString();
    result.refreshToken = object.value(QStringLiteral("refreshToken")).toString();
    result.expiresIn = object.value(QStringLiteral("expiresIn")).toInt(900);
    return result;
}
