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

    QString databasePath() const;
    QString dataRootPath() const;
    bool isEncrypted() const;
    QString lastError() const;

    QVector<QJsonObject> values(const QString& domain);
    QJsonObject value(const QString& domain, const QString& key);
    bool hasDomain(const QString& domain);
    bool upsertValue(const QString& domain, const QString& key, const QJsonObject& value);
    bool removeValue(const QString& domain, const QString& key);
    bool clearDomain(const QString& domain);

private:
    explicit LocalDataStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(LocalDataStore)

    QSqlDatabase database();
    bool ensureOpen();
    bool ensureSchema();
    QString connectionName() const;
    void setLastError(const QString& error) const;

    mutable QMutex m_mutex;
    mutable QString m_lastError;
    QString m_dataRootPath;
    QString m_databasePath;
    bool m_schemaReady = false;
    bool m_encrypted = false;
};
