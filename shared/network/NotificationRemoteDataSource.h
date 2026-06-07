#pragma once

#include "shared/network/NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

class NotificationRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static NotificationRemoteDataSource& instance();

    QString markAllRead(const QString& type = {});

signals:
    void markAllReadSucceeded(const QString& requestId, const QString& type);
    void markAllReadFailed(const QString& requestId, const QString& type, const NetworkError& error);

private:
    explicit NotificationRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(NotificationRemoteDataSource)

    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, QString> m_pendingTypes;
};
