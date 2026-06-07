#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "shared/types/User.h"

class QLockFile;

struct LoginAccount {
    QString userUuid;
    QString accountId;
    QString password;
    bool rememberPassword = true;
    bool autoLogin = false;
    bool loggedInOnDevice = false;
    QString accessToken;
    QString refreshToken;
    qint64 tokenExpiresAtUtcMs = 0;
    QString displayName;
    QString avatarPath;
    QString avatarSource;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    UserStatus status = Offline;
    QString signature;
    QString region;
    qint64 lastLoginOrder = 0;
};

struct LoginAccountListRequest {
    int limit = 4;
};

struct LoginAccountDetailRequest {
    QString accountId;
};

struct LoginCredentialRequest {
    QString accountId;
    QString password;
};

class LoginAccountRepository : public QObject
{
    Q_OBJECT
public:
    static LoginAccountRepository& instance();

    QVector<LoginAccount> requestLoginAccounts(const LoginAccountListRequest& query = {}) const;
    LoginAccount requestLoginAccount(const LoginAccountDetailRequest& query) const;
    int requestLoginAccountCount() const;
    void saveAuthenticatedAccount(const LoginAccount& account);
    void recordSuccessfulLogin(const QString& accountId);
    void updateAccountLoginOptions(const QString& accountId,
                                   bool rememberPassword,
                                   bool autoLogin,
                                   const QString& password = {});
    void saveAccountTokens(const QString& accountId,
                           const QString& accessToken,
                           const QString& refreshToken,
                           int expiresInSeconds);
    void clearAccountTokens(const QString& accountId);
    void clearAccountSessionAndPassword(const QString& accountId);
    bool isAccountLoggedInOnDevice(const QString& accountId) const;
    bool setAccountLoggedInOnDevice(const QString& accountId, bool loggedIn);
    void removeLoginAccount(const QString& accountId);

signals:
    void loginAccountsChanged();

private:
    explicit LoginAccountRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(LoginAccountRepository)

    QVector<LoginAccount> m_accounts;
    QHash<QString, QSharedPointer<QLockFile>> m_activeLoginLocks;
    qint64 m_nextLoginOrder = 0;
    mutable QMutex m_mutex;
};
