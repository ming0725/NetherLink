#pragma once

#include "NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QJsonObject>
#include <QString>

struct AuthUser {
    QString userUuid;
    QString userId;
    QString nickName;
    QString avatarPath;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    QString signature;
    QString region;
    QString status;
    int version = 0;
    QString etag;

    bool isValid() const { return !userId.isEmpty() || !userUuid.isEmpty(); }
};

struct AuthResult {
    AuthUser user;
    QJsonObject preferences;
    QString accessToken;
    QString refreshToken;
    int expiresIn = 0;

    bool isValid() const { return user.isValid() && !accessToken.isEmpty(); }
};

class AuthApiClient : public QObject
{
    Q_OBJECT

public:
    static AuthApiClient& instance();

    QString login(const QString& account, const QString& password);
    QString registerAccount(const QString& email,
                            const QString& password,
                            const QString& displayName,
                            const QString& userId);
    QString logout();

signals:
    void loginSucceeded(const QString& requestId, const AuthResult& result);
    void loginFailed(const QString& requestId, const NetworkError& error);
    void registerSucceeded(const QString& requestId, const AuthResult& result);
    void registerFailed(const QString& requestId, const NetworkError& error);
    void logoutFinished(const QString& requestId, bool success, const NetworkError& error);

private:
    enum class RequestKind {
        Login,
        Register,
        Logout
    };

    explicit AuthApiClient(QObject* parent = nullptr);
    Q_DISABLE_COPY(AuthApiClient)

    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);
    AuthResult authResultFromObject(const QJsonObject& object) const;

    QHash<QString, RequestKind> m_pendingRequests;
};

Q_DECLARE_METATYPE(AuthResult)
