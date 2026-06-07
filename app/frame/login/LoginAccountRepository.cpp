#include "LoginAccountRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryFunctionOperation.h"
#include "shared/types/RepositoryTypes.h"

#include <QJsonObject>
#include <QMutexLocker>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {

const QString kDefaultLoginPassword(QStringLiteral("netherlink"));

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

QString loginLockPath(const QString& accountId)
{
    const QString normalized = accountId.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(normalized.toUtf8()
                                                       + QByteArrayLiteral(":netherlink-login-lock-v1"),
                                                       QCryptographicHash::Sha256).toHex();
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + QStringLiteral("/NetherLink");
    }
    const QString dirPath = root + QStringLiteral("/login-locks");
    QDir().mkpath(dirPath);
    return QDir(dirPath).filePath(QString::fromLatin1(digest) + QStringLiteral(".lock"));
}

QString statusToString(UserStatus status)
{
    switch (status) {
    case Online:
        return QStringLiteral("online");
    case Mining:
        return QStringLiteral("mining");
    case Flying:
        return QStringLiteral("airplane");
    case Invisible:
        return QStringLiteral("invisible");
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
    if (normalized == QStringLiteral("mining") ||
        normalized == QStringLiteral("busy") ||
        normalized == QStringLiteral("dnd")) {
        return Mining;
    }
    if (normalized == QStringLiteral("airplane") ||
        normalized == QStringLiteral("flying") ||
        normalized == QStringLiteral("away")) {
        return Flying;
    }
    if (normalized == QStringLiteral("invisible")) {
        return Invisible;
    }
    return Offline;
}

LoginAccount accountFromJson(const QJsonObject& object)
{
    LoginAccount account;
    account.userUuid = object.value(QStringLiteral("userUuid")).toString();
    account.accountId = object.value(QStringLiteral("accountId")).toString();
    account.password = object.value(QStringLiteral("password")).toString(kDefaultLoginPassword);
    account.rememberPassword = object.value(QStringLiteral("rememberPassword")).toBool(!account.password.isEmpty());
    if (!account.rememberPassword) {
        account.password.clear();
    }
    account.autoLogin = object.value(QStringLiteral("autoLogin")).toBool();
    account.loggedInOnDevice = false;
    account.accessToken = object.value(QStringLiteral("accessToken")).toString();
    account.refreshToken = object.value(QStringLiteral("refreshToken")).toString();
    account.tokenExpiresAtUtcMs = static_cast<qint64>(
            object.value(QStringLiteral("tokenExpiresAtUtcMs")).toDouble());
    account.displayName = object.value(QStringLiteral("displayName")).toString();
    account.avatarPath = object.value(QStringLiteral("avatarPath")).toString();
    account.avatarSource = object.value(QStringLiteral("avatarSource")).toString();
    account.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    account.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    account.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    account.status = statusFromString(object.value(QStringLiteral("status")).toString());
    account.signature = object.value(QStringLiteral("signature")).toString();
    account.region = object.value(QStringLiteral("region")).toString();
    account.lastLoginOrder = static_cast<qint64>(object.value(QStringLiteral("lastLoginOrder")).toDouble());
    return account;
}

QJsonObject accountToJson(const LoginAccount& account)
{
    return {
            {QStringLiteral("userUuid"), account.userUuid},
            {QStringLiteral("accountId"), account.accountId},
            {QStringLiteral("password"), account.rememberPassword ? account.password : QString()},
            {QStringLiteral("rememberPassword"), account.rememberPassword},
            {QStringLiteral("autoLogin"), account.autoLogin},
            {QStringLiteral("accessToken"), account.accessToken},
            {QStringLiteral("refreshToken"), account.refreshToken},
            {QStringLiteral("tokenExpiresAtUtcMs"), static_cast<double>(account.tokenExpiresAtUtcMs)},
            {QStringLiteral("displayName"), account.displayName},
            {QStringLiteral("avatarPath"), account.avatarPath},
            {QStringLiteral("avatarSource"), account.avatarSource},
            {QStringLiteral("avatarVersion"), account.avatarVersion},
            {QStringLiteral("avatarEtag"), account.avatarEtag},
            {QStringLiteral("avatarContentHash"), account.avatarContentHash},
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

    m_nextLoginOrder = 1;
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
        for (LoginAccount& account : result) {
            account.loggedInOnDevice = isAccountLoggedInOnDevice(account.accountId);
        }
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
        if (index < 0) {
            return LoginAccount{};
        }

        LoginAccount account = m_accounts.at(index);
        account.loggedInOnDevice = isAccountLoggedInOnDevice(account.accountId);
        return account;
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

void LoginAccountRepository::saveAuthenticatedAccount(const LoginAccount& account)
{
    if (account.accountId.trimmed().isEmpty()) {
        return;
    }

    LoginAccount normalized = account;
    normalized.accountId = normalized.accountId.trimmed();
    if (!normalized.rememberPassword) {
        normalized.password.clear();
    }
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, normalized.accountId);
        normalized.lastLoginOrder = m_nextLoginOrder++;
        if (index >= 0) {
            if (normalized.userUuid.isEmpty()) {
                normalized.userUuid = m_accounts.at(index).userUuid;
            }
            if (!normalized.rememberPassword) {
                normalized.password.clear();
            }
            if (normalized.accessToken.isEmpty()) {
                normalized.accessToken = m_accounts.at(index).accessToken;
            }
            if (normalized.refreshToken.isEmpty()) {
                normalized.refreshToken = m_accounts.at(index).refreshToken;
            }
            if (normalized.tokenExpiresAtUtcMs <= 0) {
                normalized.tokenExpiresAtUtcMs = m_accounts.at(index).tokenExpiresAtUtcMs;
            }
            normalized.loggedInOnDevice = isAccountLoggedInOnDevice(normalized.accountId);
            m_accounts[index] = normalized;
        } else {
            normalized.loggedInOnDevice = isAccountLoggedInOnDevice(normalized.accountId);
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

void LoginAccountRepository::recordSuccessfulLogin(const QString& accountId)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId);
        if (index < 0) {
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

void LoginAccountRepository::updateAccountLoginOptions(const QString& accountId,
                                                       bool rememberPassword,
                                                       bool autoLogin,
                                                       const QString& password)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId.trimmed());
        if (index < 0) {
            return;
        }

        LoginAccount& account = m_accounts[index];
        account.rememberPassword = rememberPassword;
        account.autoLogin = autoLogin;
        account.password = rememberPassword ? password : QString();
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               account.accountId,
                                               accountToJson(account));
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}

void LoginAccountRepository::saveAccountTokens(const QString& accountId,
                                               const QString& accessToken,
                                               const QString& refreshToken,
                                               int expiresInSeconds)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId.trimmed());
        if (index < 0) {
            return;
        }

        LoginAccount& account = m_accounts[index];
        account.accessToken = accessToken;
        account.refreshToken = refreshToken;
        account.tokenExpiresAtUtcMs = QDateTime::currentDateTimeUtc()
                                              .addSecs(qMax(0, expiresInSeconds))
                                              .toMSecsSinceEpoch();
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               account.accountId,
                                               accountToJson(account));
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}

void LoginAccountRepository::clearAccountTokens(const QString& accountId)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId.trimmed());
        if (index < 0) {
            return;
        }

        LoginAccount& account = m_accounts[index];
        account.accessToken.clear();
        account.refreshToken.clear();
        account.tokenExpiresAtUtcMs = 0;
        account.autoLogin = false;
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               account.accountId,
                                               accountToJson(account));
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}

void LoginAccountRepository::clearAccountSessionAndPassword(const QString& accountId)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId.trimmed());
        if (index < 0) {
            return;
        }

        LoginAccount& account = m_accounts[index];
        account.accessToken.clear();
        account.refreshToken.clear();
        account.tokenExpiresAtUtcMs = 0;
        account.autoLogin = false;
        account.rememberPassword = false;
        account.password.clear();
        LocalDataStore::instance().upsertValue(QStringLiteral("login_accounts"),
                                               account.accountId,
                                               accountToJson(account));
        changed = true;
    }

    if (changed) {
        emit loginAccountsChanged();
    }
}

bool LoginAccountRepository::isAccountLoggedInOnDevice(const QString& accountId) const
{
    const QString path = loginLockPath(accountId);
    if (path.isEmpty()) {
        return false;
    }

    QLockFile probe(path);
    if (probe.tryLock(0)) {
        probe.unlock();
        return false;
    }

    if (probe.removeStaleLockFile() && probe.tryLock(0)) {
        probe.unlock();
        return false;
    }

    return true;
}

bool LoginAccountRepository::setAccountLoggedInOnDevice(const QString& accountId, bool loggedIn)
{
    bool changed = false;
    bool lockChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        const int index = indexOfAccount(m_accounts, accountId.trimmed());
        if (index < 0) {
            return false;
        }

        const QString normalizedAccountId = m_accounts.at(index).accountId;
        if (loggedIn) {
            if (!m_activeLoginLocks.contains(normalizedAccountId)) {
                const QString path = loginLockPath(normalizedAccountId);
                if (path.isEmpty()) {
                    return false;
                }

                auto lock = QSharedPointer<QLockFile>::create(path);
                if (!lock->tryLock(0)) {
                    lock->removeStaleLockFile();
                    if (!lock->tryLock(0)) {
                        return false;
                    }
                }
                m_activeLoginLocks.insert(normalizedAccountId, lock);
                lockChanged = true;
            }
        } else {
            auto lock = m_activeLoginLocks.take(normalizedAccountId);
            if (lock) {
                lock->unlock();
                lockChanged = true;
            }
        }

        const bool active = isAccountLoggedInOnDevice(normalizedAccountId);
        if (m_accounts.at(index).loggedInOnDevice != active) {
            m_accounts[index].loggedInOnDevice = active;
            changed = true;
        }
    }

    if (changed || lockChanged) {
        emit loginAccountsChanged();
    }
    return loggedIn ? lockChanged || isAccountLoggedInOnDevice(accountId) : true;
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
