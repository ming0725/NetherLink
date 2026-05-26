#include "LoginAccountRepository.h"

#include <QMutexLocker>

#include <algorithm>

namespace {

QVector<LoginAccount> sortedByRecentLogin(QVector<LoginAccount> accounts)
{
    std::sort(accounts.begin(), accounts.end(), [](const LoginAccount& lhs, const LoginAccount& rhs) {
        if (lhs.lastLoginOrder != rhs.lastLoginOrder) {
            return lhs.lastLoginOrder > rhs.lastLoginOrder;
        }
        return lhs.accountId < rhs.accountId;
    });
    return accounts;
}

int indexOfAccount(const QVector<LoginAccount>& accounts, const QString& accountId)
{
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).accountId == accountId) {
            return i;
        }
    }
    return -1;
}

} // namespace

LoginAccountRepository::LoginAccountRepository(QObject* parent)
    : QObject(parent)
{
    m_accounts = {
            {QStringLiteral("10086"), QStringLiteral("netherlink"), QStringLiteral(":/resources/avatar/0.jpg"), 6},
            {QStringLiteral("10087"), QStringLiteral("redstone"), QStringLiteral(":/resources/avatar/1.jpg"), 5},
            {QStringLiteral("10088"), QStringLiteral("diamond"), QStringLiteral(":/resources/avatar/2.jpg"), 4},
            {QStringLiteral("10089"), QStringLiteral("portal"), QStringLiteral(":/resources/avatar/3.jpg"), 3},
            {QStringLiteral("10090"), QStringLiteral("mining"), QStringLiteral(":/resources/avatar/4.jpg"), 2},
            {QStringLiteral("10091"), QStringLiteral("flying"), QStringLiteral(":/resources/avatar/5.jpg"), 1},
    };
    m_nextLoginOrder = 7;
}

LoginAccountRepository& LoginAccountRepository::instance()
{
    static LoginAccountRepository repo;
    return repo;
}

QVector<LoginAccount> LoginAccountRepository::requestLoginAccounts(const LoginAccountListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    QVector<LoginAccount> result = sortedByRecentLogin(m_accounts);
    const int limit = query.limit < 0 ? result.size() : qMax(0, query.limit);
    return result.mid(0, limit);
}

LoginAccount LoginAccountRepository::requestLoginAccount(const LoginAccountDetailRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    const int index = indexOfAccount(m_accounts, query.accountId);
    return index >= 0 ? m_accounts.at(index) : LoginAccount{};
}

int LoginAccountRepository::requestLoginAccountCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_accounts.size();
}

bool LoginAccountRepository::validateCredentials(const QString& accountId, const QString& password) const
{
    QMutexLocker locker(&m_mutex);
    const int index = indexOfAccount(m_accounts, accountId);
    return index >= 0 && m_accounts.at(index).password == password;
}

void LoginAccountRepository::recordSuccessfulLogin(const QString& accountId, const QString& password)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId);
        if (index < 0 || m_accounts.at(index).password != password) {
            return;
        }

        m_accounts[index].lastLoginOrder = m_nextLoginOrder++;
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}

void LoginAccountRepository::removeLoginAccount(const QString& accountId)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId);
        if (index < 0) {
            return;
        }

        m_accounts.removeAt(index);
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}
