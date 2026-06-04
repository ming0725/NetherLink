#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

class QSqlDatabase;

class LocalDataStore : public QObject {
    Q_OBJECT

public:
    static LocalDataStore& instance();

    QString activeAccountKey() const;
    QString activeAccountHash() const;
    QString databasePath() const;
    QString dataRootPath() const;
    bool isEncrypted() const;
    QString lastError() const;

    bool setActiveAccount(const QString& accountKey);
    void clearActiveAccount();
    QVector<QJsonObject> values(const QString& domain);
    QJsonObject value(const QString& domain, const QString& key);
    bool hasDomain(const QString& domain);
    bool upsertValue(const QString& domain, const QString& key, const QJsonObject& value);
    bool removeValue(const QString& domain, const QString& key);
    bool clearDomain(const QString& domain);

signals:
    void activeAccountChanged(const QString& accountHash);
    void domainChanged(const QString& domain);

private:
    explicit LocalDataStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(LocalDataStore)

    bool isGlobalDomain(const QString& domain) const;
    QString accountRootPath() const;
    QString accountDatabasePath() const;
    QString pathForDomain(const QString& domain) const;
    QString connectionNameForPath(const QString& path) const;
    QSqlDatabase databaseForPath(const QString& path);
    bool ensureOpen(const QString& path);
    bool ensureSchema(QSqlDatabase& db, bool& schemaReady);
    void setLastError(const QString& error) const;

    mutable QMutex m_mutex;
    mutable QString m_lastError;
    QString m_dataRootPath;
    QString m_globalDatabasePath;
    QString m_activeAccountKey;
    QString m_activeAccountHash;
    bool m_globalSchemaReady = false;
    bool m_accountSchemaReady = false;
    bool m_encrypted = false;
};
