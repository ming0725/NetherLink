#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

struct LoginAccount {
    QString accountId;
    QString password;
    QString avatarPath;
    qint64 lastLoginOrder = 0;
};

struct LoginAccountListRequest {
    int limit = 4;
};

struct LoginAccountDetailRequest {
    QString accountId;
};

class LoginAccountRepository : public QObject
{
    Q_OBJECT
public:
    static LoginAccountRepository& instance();

    QVector<LoginAccount> requestLoginAccounts(const LoginAccountListRequest& query = {}) const;
    LoginAccount requestLoginAccount(const LoginAccountDetailRequest& query) const;
    int requestLoginAccountCount() const;
    bool validateCredentials(const QString& accountId, const QString& password) const;

    void recordSuccessfulLogin(const QString& accountId, const QString& password);
    void removeLoginAccount(const QString& accountId);

signals:
    void loginAccountsChanged();

private:
    explicit LoginAccountRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(LoginAccountRepository)

    QVector<LoginAccount> m_accounts;
    qint64 m_nextLoginOrder = 0;
    mutable QMutex m_mutex;
};
