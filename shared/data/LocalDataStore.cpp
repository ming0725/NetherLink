#include "LocalDataStore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUuid>
#include <QtGlobal>

namespace {

QString writableRootPath()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + QStringLiteral("/NetherLink");
    }
    QDir().mkpath(root);
    return root;
}

QString hashAccountKey(const QString& accountKey)
{
    const QString normalized = accountKey.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    const QByteArray material = normalized.toUtf8()
            + QByteArrayLiteral(":netherlink-local-account-v1");
    return QString::fromLatin1(
            QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex());
}

QByteArray sqlCipherKey()
{
    const QByteArray key = qgetenv("NETHERLINK_SQLCIPHER_KEY");
    return key.trimmed();
}

QString connectionTokenForPath(const QString& path)
{
    thread_local QHash<QString, QString> connectionTokens;
    auto it = connectionTokens.constFind(path);
    if (it != connectionTokens.constEnd()) {
        return it.value();
    }

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    connectionTokens.insert(path, token);
    return token;
}

} // namespace

LocalDataStore& LocalDataStore::instance()
{
    static LocalDataStore store;
    return store;
}

LocalDataStore::LocalDataStore(QObject* parent)
    : QObject(parent)
    , m_dataRootPath(writableRootPath())
    , m_globalDatabasePath(m_dataRootPath + QStringLiteral("/global.sqlite"))
{
}

QString LocalDataStore::activeAccountKey() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeAccountKey;
}

QString LocalDataStore::activeAccountHash() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeAccountHash;
}

QString LocalDataStore::databasePath() const
{
    QMutexLocker locker(&m_mutex);
    const QString accountPath = accountDatabasePath();
    return accountPath.isEmpty() ? m_globalDatabasePath : accountPath;
}

QString LocalDataStore::dataRootPath() const
{
    QMutexLocker locker(&m_mutex);
    const QString accountPath = accountRootPath();
    return accountPath.isEmpty() ? m_dataRootPath : accountPath;
}

bool LocalDataStore::isEncrypted() const
{
    return m_encrypted;
}

QString LocalDataStore::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool LocalDataStore::setActiveAccount(const QString& accountKey)
{
    const QString normalized = accountKey.trimmed();
    const QString accountHash = hashAccountKey(normalized);
    if (accountHash.isEmpty()) {
        return false;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_activeAccountHash != accountHash;
        m_activeAccountKey = normalized;
        m_activeAccountHash = accountHash;
        if (changed) {
            m_accountSchemaReady = false;
        }
    }

    if (changed) {
        emit activeAccountChanged(accountHash);
    }
    return true;
}

void LocalDataStore::clearActiveAccount()
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = !m_activeAccountHash.isEmpty();
        m_activeAccountKey.clear();
        m_activeAccountHash.clear();
        m_accountSchemaReady = false;
    }

    if (changed) {
        emit activeAccountChanged({});
    }
}

QVector<QJsonObject> LocalDataStore::values(const QString& domain)
{
    QMutexLocker locker(&m_mutex);
    QVector<QJsonObject> result;
    const QString path = pathForDomain(domain);
    if (path.isEmpty() || !ensureOpen(path)) {
        return result;
    }

    QSqlDatabase db = databaseForPath(path);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value_json FROM local_records WHERE domain = ? ORDER BY updated_at, key"));
    query.addBindValue(domain);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toByteArray());
        if (document.isObject()) {
            result.push_back(document.object());
        }
    }
    return result;
}

QJsonObject LocalDataStore::value(const QString& domain, const QString& key)
{
    QMutexLocker locker(&m_mutex);
    const QString path = pathForDomain(domain);
    if (path.isEmpty() || !ensureOpen(path)) {
        return {};
    }

    QSqlDatabase db = databaseForPath(path);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value_json FROM local_records WHERE domain = ? AND key = ?"));
    query.addBindValue(domain);
    query.addBindValue(key);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return {};
    }
    if (!query.next()) {
        return {};
    }
    return QJsonDocument::fromJson(query.value(0).toByteArray()).object();
}

QJsonObject LocalDataStore::valueForAccount(const QString& accountKey, const QString& domain, const QString& key)
{
    QMutexLocker locker(&m_mutex);
    const QString accountHash = hashAccountKey(accountKey);
    if (accountHash.isEmpty() || isGlobalDomain(domain)) {
        return {};
    }

    const QString root = QDir(m_dataRootPath + QStringLiteral("/accounts")).filePath(accountHash);
    const QString path = QDir(root).filePath(QStringLiteral("data.sqlite"));
    if (!QFileInfo::exists(path)) {
        return {};
    }

    bool schemaReady = false;
    if (domain.isEmpty() || key.isEmpty() || !ensureOpen(path, accountHash, schemaReady)) {
        return {};
    }

    QSqlDatabase db = databaseForPath(path, accountHash);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value_json FROM local_records WHERE domain = ? AND key = ?"));
    query.addBindValue(domain);
    query.addBindValue(key);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return {};
    }
    if (!query.next()) {
        return {};
    }
    return QJsonDocument::fromJson(query.value(0).toByteArray()).object();
}

bool LocalDataStore::hasDomain(const QString& domain)
{
    QMutexLocker locker(&m_mutex);
    const QString path = pathForDomain(domain);
    if (path.isEmpty() || !ensureOpen(path)) {
        return false;
    }

    QSqlDatabase db = databaseForPath(path);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM local_records WHERE domain = ? LIMIT 1"));
    query.addBindValue(domain);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.next();
}

bool LocalDataStore::upsertValue(const QString& domain, const QString& key, const QJsonObject& value)
{
    bool ok = false;
    {
        QMutexLocker locker(&m_mutex);
        const QString path = pathForDomain(domain);
        if (domain.isEmpty() || key.isEmpty() || path.isEmpty() || !ensureOpen(path)) {
            return false;
        }

        QSqlDatabase db = databaseForPath(path);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO local_records(domain, key, value_json, updated_at) "
                "VALUES(?, ?, ?, ?)"));
        query.addBindValue(domain);
        query.addBindValue(key);
        query.addBindValue(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!query.exec()) {
            setLastError(query.lastError().text());
            return false;
        }
        ok = true;
    }

    if (ok) {
        emit domainChanged(domain);
    }
    return ok;
}

bool LocalDataStore::upsertValueForAccount(const QString& accountKey,
                                           const QString& domain,
                                           const QString& key,
                                           const QJsonObject& value)
{
    bool ok = false;
    {
        QMutexLocker locker(&m_mutex);
        const QString accountHash = hashAccountKey(accountKey);
        if (accountHash.isEmpty() || isGlobalDomain(domain) || domain.isEmpty() || key.isEmpty()) {
            return false;
        }

        const QString root = QDir(m_dataRootPath + QStringLiteral("/accounts")).filePath(accountHash);
        const QString path = QDir(root).filePath(QStringLiteral("data.sqlite"));
        bool schemaReady = false;
        if (!ensureOpen(path, accountHash, schemaReady)) {
            return false;
        }

        QSqlDatabase db = databaseForPath(path, accountHash);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO local_records(domain, key, value_json, updated_at) "
                "VALUES(?, ?, ?, ?)"));
        query.addBindValue(domain);
        query.addBindValue(key);
        query.addBindValue(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!query.exec()) {
            setLastError(query.lastError().text());
            return false;
        }
        ok = true;
    }

    if (ok) {
        emit domainChanged(domain);
    }
    return ok;
}

bool LocalDataStore::removeValue(const QString& domain, const QString& key)
{
    bool ok = false;
    {
        QMutexLocker locker(&m_mutex);
        const QString path = pathForDomain(domain);
        if (path.isEmpty() || !ensureOpen(path)) {
            return false;
        }

        QSqlDatabase db = databaseForPath(path);
        QSqlQuery query(db);
        query.prepare(QStringLiteral("DELETE FROM local_records WHERE domain = ? AND key = ?"));
        query.addBindValue(domain);
        query.addBindValue(key);
        if (!query.exec()) {
            setLastError(query.lastError().text());
            return false;
        }
        ok = true;
    }

    if (ok) {
        emit domainChanged(domain);
    }
    return ok;
}

bool LocalDataStore::clearDomain(const QString& domain)
{
    bool ok = false;
    {
        QMutexLocker locker(&m_mutex);
        const QString path = pathForDomain(domain);
        if (path.isEmpty() || !ensureOpen(path)) {
            return false;
        }

        QSqlDatabase db = databaseForPath(path);
        QSqlQuery query(db);
        query.prepare(QStringLiteral("DELETE FROM local_records WHERE domain = ?"));
        query.addBindValue(domain);
        if (!query.exec()) {
            setLastError(query.lastError().text());
            return false;
        }
        ok = true;
    }

    if (ok) {
        emit domainChanged(domain);
    }
    return ok;
}

bool LocalDataStore::isGlobalDomain(const QString& domain) const
{
    return domain == QStringLiteral("login_accounts");
}

QString LocalDataStore::accountRootPath() const
{
    if (m_activeAccountHash.isEmpty()) {
        return {};
    }
    return QDir(m_dataRootPath + QStringLiteral("/accounts")).filePath(m_activeAccountHash);
}

QString LocalDataStore::accountDatabasePath() const
{
    const QString root = accountRootPath();
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("data.sqlite"));
}

QString LocalDataStore::pathForDomain(const QString& domain) const
{
    if (isGlobalDomain(domain)) {
        return m_globalDatabasePath;
    }
    return accountDatabasePath();
}

QString LocalDataStore::connectionNameForPath(const QString& path) const
{
    return connectionNameForPath(path, m_activeAccountHash);
}

QString LocalDataStore::connectionNameForPath(const QString& path, const QString& accountHash) const
{
    const QString threadToken = connectionTokenForPath(path);
    if (path == m_globalDatabasePath) {
        return QStringLiteral("netherlink_global_store_%1").arg(threadToken);
    }

    return QStringLiteral("netherlink_account_store_%1_%2").arg(accountHash, threadToken);
}

QSqlDatabase LocalDataStore::databaseForPath(const QString& path)
{
    return databaseForPath(path, m_activeAccountHash);
}

QSqlDatabase LocalDataStore::databaseForPath(const QString& path, const QString& accountHash)
{
    const QString name = connectionNameForPath(path, accountHash);
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    return db;
}

bool LocalDataStore::ensureOpen(const QString& path)
{
    QSqlDatabase db = databaseForPath(path);
    if (!db.isOpen() && !db.open()) {
        setLastError(db.lastError().text());
        return false;
    }

    const QByteArray key = sqlCipherKey();
    if (!key.isEmpty()) {
        QSqlQuery keyQuery(db);
        keyQuery.exec(QStringLiteral("PRAGMA key = '%1'").arg(QString::fromUtf8(key).replace('\'', QLatin1String("''"))));
        QSqlQuery cipherProbe(db);
        m_encrypted = cipherProbe.exec(QStringLiteral("PRAGMA cipher_version")) && cipherProbe.next();
    }

    if (path == m_globalDatabasePath) {
        return ensureSchema(db, m_globalSchemaReady);
    }
    return ensureSchema(db, m_accountSchemaReady);
}

bool LocalDataStore::ensureOpen(const QString& path, const QString& accountHash, bool& schemaReady)
{
    QSqlDatabase db = databaseForPath(path, accountHash);
    if (!db.isOpen() && !db.open()) {
        setLastError(db.lastError().text());
        return false;
    }

    const QByteArray key = sqlCipherKey();
    if (!key.isEmpty()) {
        QSqlQuery keyQuery(db);
        keyQuery.exec(QStringLiteral("PRAGMA key = '%1'").arg(QString::fromUtf8(key).replace('\'', QLatin1String("''"))));
        QSqlQuery cipherProbe(db);
        m_encrypted = cipherProbe.exec(QStringLiteral("PRAGMA cipher_version")) && cipherProbe.next();
    }

    return ensureSchema(db, schemaReady);
}

bool LocalDataStore::ensureSchema(QSqlDatabase& db, bool& schemaReady)
{
    if (schemaReady) {
        return true;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS local_records ("
                "domain TEXT NOT NULL,"
                "key TEXT NOT NULL,"
                "value_json TEXT NOT NULL,"
                "updated_at TEXT NOT NULL,"
                "PRIMARY KEY(domain, key))"))) {
        setLastError(query.lastError().text());
        return false;
    }

    if (!query.exec(QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_local_records_domain "
                "ON local_records(domain)"))) {
        setLastError(query.lastError().text());
        return false;
    }

    schemaReady = true;
    return true;
}

void LocalDataStore::setLastError(const QString& error) const
{
    m_lastError = error;
}
