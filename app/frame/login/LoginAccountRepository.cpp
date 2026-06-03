#include "LoginAccountRepository.h"

#include "app/state/CurrentUserProfileRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryFunctionOperation.h"
#include "shared/types/RepositoryTypes.h"

#include <QJsonObject>
#include <QMutexLocker>
#include <QRandomGenerator>

#include <algorithm>
#include <utility>

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

QString statusToString(UserStatus status)
{
    switch (status) {
    case Online:
        return QStringLiteral("online");
    case Mining:
        return QStringLiteral("mining");
    case Flying:
        return QStringLiteral("flying");
    case Offline:
    default:
        return QStringLiteral("offline");
    }
}

UserStatus statusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("online")) {
        return Online;
    }
    if (normalized == QStringLiteral("mining")) {
        return Mining;
    }
    if (normalized == QStringLiteral("flying")) {
        return Flying;
    }
    return Offline;
}

LoginAccount accountFromJson(const QJsonObject& object)
{
    LoginAccount account;
    account.accountId = object.value(QStringLiteral("accountId")).toString();
    account.password = object.value(QStringLiteral("password")).toString(kDefaultLoginPassword);
    account.displayName = object.value(QStringLiteral("displayName")).toString();
    account.avatarPath = object.value(QStringLiteral("avatarPath")).toString();
    account.status = statusFromString(object.value(QStringLiteral("status")).toString());
    account.signature = object.value(QStringLiteral("signature")).toString();
    account.region = object.value(QStringLiteral("region")).toString();
    account.lastLoginOrder = static_cast<qint64>(object.value(QStringLiteral("lastLoginOrder")).toDouble());
    return account;
}

QJsonObject accountToJson(const LoginAccount& account)
{
    return {
            {QStringLiteral("accountId"), account.accountId},
            {QStringLiteral("password"), account.password},
            {QStringLiteral("displayName"), account.displayName},
            {QStringLiteral("avatarPath"), account.avatarPath},
            {QStringLiteral("status"), statusToString(account.status)},
            {QStringLiteral("signature"), account.signature},
            {QStringLiteral("region"), account.region},
            {QStringLiteral("lastLoginOrder"), static_cast<double>(account.lastLoginOrder)}
    };
}

} // namespace

LoginAccountRepository::LoginAccountRepository(QObject* parent)
    : QObject(parent)
{
    LocalDataStore& store = LocalDataStore::instance();
    if (store.hasDomain(QStringLiteral("login_accounts"))) {
        for (const QJsonObject& object : store.values(QStringLiteral("login_accounts"))) {
            const LoginAccount account = accountFromJson(object);
            if (account.accountId.isEmpty()) {
                continue;
            }
            m_accounts.push_back(account);
            m_nextLoginOrder = qMax(m_nextLoginOrder, account.lastLoginOrder + 1);
        }
        return;
    }

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
        m_accounts.push_back(accountFromUser(user, 0));
    }

    m_nextLoginOrder = 2;
    for (const LoginAccount& account : std::as_const(m_accounts)) {
        store.upsertValue(QStringLiteral("login_accounts"), account.accountId, accountToJson(account));
    }
}

LoginAccountRepository& LoginAccountRepository::instance()
{
    static LoginAccountRepository repo;
    return repo;
}

QVector<LoginAccount> LoginAccountRepository::requestLoginAccounts(const LoginAccountListRequest& query) const
{
    auto handler = [this](const LoginAccountListRequest& request) {
        QMutexLocker locker(&m_mutex);
        QVector<LoginAccount> result = sortedByRecentLogin(m_accounts);
        const int limit = request.limit < 0 ? result.size() : qMax(0, request.limit);
        return result.mid(0, limit);
    };

    return RepositoryFunctionOperation<LoginAccountListRequest, QVector<LoginAccount>, decltype(handler)>(handler)
            .request(query);
}

LoginAccount LoginAccountRepository::requestLoginAccount(const LoginAccountDetailRequest& query) const
{
    auto handler = [this](const LoginAccountDetailRequest& request) {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, request.accountId);
        return index >= 0 ? m_accounts.at(index) : LoginAccount{};
    };

    return RepositoryFunctionOperation<LoginAccountDetailRequest, LoginAccount, decltype(handler)>(handler)
            .request(query);
}

int LoginAccountRepository::requestLoginAccountCount() const
{
    auto handler = [this](const EmptyRequest&) {
        QMutexLocker locker(&m_mutex);
        return m_accounts.size();
    };

    return RepositoryFunctionOperation<EmptyRequest, int, decltype(handler)>(handler)
            .request({});
}

bool LoginAccountRepository::validateCredentials(const LoginCredentialRequest& query) const
{
    auto handler = [this](const LoginCredentialRequest& request) {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, request.accountId);
        return index >= 0 && m_accounts.at(index).password == request.password;
    };

    return RepositoryFunctionOperation<LoginCredentialRequest, bool, decltype(handler)>(handler)
            .request(query);
}

bool LoginAccountRepository::validateCredentials(const QString& accountId, const QString& password) const
{
    return validateCredentials({accountId, password});
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
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               account.accountId,
                                               accountToJson(account));
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

void LoginAccountRepository::saveAuthenticatedAccount(const LoginAccount& account)
{
    if (account.accountId.trimmed().isEmpty()) {
        return;
    }

    LoginAccount normalized = account;
    normalized.accountId = normalized.accountId.trimmed();
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, normalized.accountId);
        normalized.lastLoginOrder = m_nextLoginOrder++;
        if (index >= 0) {
            m_accounts[index] = normalized;
        } else {
            m_accounts.push_back(normalized);
        }
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               normalized.accountId,
                                               accountToJson(normalized));
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
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
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               m_accounts[index].accountId,
                                               accountToJson(m_accounts[index]));
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
        LocalDataStore::instance().removeValue(QStringLiteral("login_accounts"), accountId);
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}
