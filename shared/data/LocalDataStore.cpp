#include "LocalDataStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
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

QByteArray sqlCipherKey()
{
    const QByteArray key = qgetenv("NETHERLINK_SQLCIPHER_KEY");
    return key.trimmed();
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
    , m_databasePath(m_dataRootPath + QStringLiteral("/netherlink-local.sqlite"))
{
}

QString LocalDataStore::databasePath() const
{
    return m_databasePath;
}

QString LocalDataStore::dataRootPath() const
{
    return m_dataRootPath;
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

QVector<QJsonObject> LocalDataStore::values(const QString& domain)
{
    QMutexLocker locker(&m_mutex);
    QVector<QJsonObject> result;
    if (!ensureOpen()) {
        return result;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT value_json FROM local_records WHERE domain = ? ORDER BY key"));
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
    if (!ensureOpen()) {
        return {};
    }

    QSqlQuery query(database());
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
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(database());
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
    QMutexLocker locker(&m_mutex);
    if (domain.isEmpty() || key.isEmpty() || !ensureOpen()) {
        return false;
    }

    QSqlQuery query(database());
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
    return true;
}

bool LocalDataStore::removeValue(const QString& domain, const QString& key)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM local_records WHERE domain = ? AND key = ?"));
    query.addBindValue(domain);
    query.addBindValue(key);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool LocalDataStore::clearDomain(const QString& domain)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM local_records WHERE domain = ?"));
    query.addBindValue(domain);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

QSqlDatabase LocalDataStore::database()
{
    const QString name = connectionName();
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(m_databasePath);
    return db;
}

bool LocalDataStore::ensureOpen()
{
    QSqlDatabase db = database();
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

    return ensureSchema();
}

bool LocalDataStore::ensureSchema()
{
    if (m_schemaReady) {
        return true;
    }

    QSqlQuery query(database());
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

    m_schemaReady = true;
    return true;
}

QString LocalDataStore::connectionName() const
{
    return QStringLiteral("netherlink_local_store");
}

void LocalDataStore::setLastError(const QString& error) const
{
    m_lastError = error;
}
