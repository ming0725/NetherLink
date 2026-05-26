#include "LoginAccountRepository.h"

#include "app/state/CurrentUserProfileRepository.h"
#include "features/friend/data/UserRepository.h"

#include <QMutexLocker>
#include <QRandomGenerator>

#include <algorithm>

namespace {

const QString kDefaultLoginPassword(QStringLiteral("netherlink"));
constexpr int kAvatarCount = 11;

QString randomAvatarPath()
{
    return QStringLiteral(":/resources/avatar/%1.jpg").arg(QRandomGenerator::global()->bounded(kAvatarCount));
}

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

LoginAccount accountFromProfile(const CurrentUserProfile& profile, qint64 lastLoginOrder)
{
    LoginAccount account;
    account.accountId = profile.userId;
    account.password = kDefaultLoginPassword;
    account.displayName = profile.nickName;
    account.avatarPath = profile.avatarPath;
    account.status = profile.status;
    account.signature = profile.signature;
    account.region = profile.region;
    account.lastLoginOrder = lastLoginOrder;
    return account;
}

LoginAccount accountFromUser(const User& user, qint64 lastLoginOrder)
{
    LoginAccount account;
    account.accountId = user.id;
    account.password = kDefaultLoginPassword;
    account.displayName = user.nick;
    account.avatarPath = user.avatarPath;
    account.status = user.status;
    account.signature = user.signature;
    account.region = user.region;
    account.lastLoginOrder = lastLoginOrder;
    return account;
}

} // namespace

LoginAccountRepository::LoginAccountRepository(QObject* parent)
    : QObject(parent)
{
    const CurrentUserProfile defaultProfile =
            CurrentUserProfileRepository::instance().requestCurrentUserProfile({QStringLiteral("u007")});
    if (!defaultProfile.userId.isEmpty()) {
        m_accounts.push_back(accountFromProfile(defaultProfile, 1));
    }

    const QVector<User> users = UserRepository::instance().requestAllUsers();
    for (const User& user : users) {
        if (user.id.isEmpty() || indexOfAccount(m_accounts, user.id) >= 0) {
            continue;
        }
        m_accounts.push_back(accountFromUser(user, user.id.startsWith(QStringLiteral("perf_")) ? -1 : 0));
    }

    m_nextLoginOrder = 2;
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

bool LoginAccountRepository::registerAccount(const QString& email, const QString& password, const QString& displayName)
{
    const QString accountId = email.trimmed().toLower();
    const QString nickname = displayName.trimmed();
    if (accountId.isEmpty() || password.isEmpty() || nickname.isEmpty()) {
        return false;
    }
    if (!UserRepository::instance().requestUserDetail({accountId}).id.isEmpty()) {
        return false;
    }

    LoginAccount account;
    account.accountId = accountId;
    account.password = password;
    account.displayName = nickname;
    account.avatarPath = randomAvatarPath();
    account.status = Online;
    account.signature = QStringLiteral("刚刚注册 NetherLink 账号");
    account.region = QString();

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (indexOfAccount(m_accounts, accountId) >= 0) {
            return false;
        }

        account.lastLoginOrder = m_nextLoginOrder++;
        m_accounts.push_back(account);
        changed = true;
    }

    User user;
    user.id = account.accountId;
    user.nick = account.displayName;
    user.avatarPath = account.avatarPath;
    user.status = account.status;
    user.signature = account.signature;
    user.isFriend = false;
    user.region = account.region;
    UserRepository::instance().saveUser(user);

    CurrentUserProfile profile;
    profile.userId = account.accountId;
    profile.nickName = account.displayName;
    profile.avatarPath = account.avatarPath;
    profile.status = account.status;
    profile.signature = account.signature;
    profile.region = account.region;
    CurrentUserProfileRepository::instance().saveCurrentUserProfile(profile);

    if (changed) {
        emit loginAccountsChanged();
    }
    return true;
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
