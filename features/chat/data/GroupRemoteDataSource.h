#pragma once

#include "shared/network/NetworkTypes.h"
#include "shared/types/Group.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>

class GroupRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static GroupRemoteDataSource& instance();

    QString updateGroup(const Group& group);
    QString updateMySettings(const Group& group);
    QString leaveGroup(const QString& groupId, const QString& currentUserUuid);

signals:
    void groupUpdated(const QString& requestId, const Group& group);
    void groupMySettingsUpdated(const QString& requestId, const Group& group);
    void groupLeft(const QString& requestId, const QString& groupId);
    void groupUpdateFailed(const QString& requestId, const QString& groupId, const NetworkError& error);
    void groupMySettingsUpdateFailed(const QString& requestId, const QString& groupId, const NetworkError& error);
    void groupLeaveFailed(const QString& requestId, const QString& groupId, const NetworkError& error);

private:
    enum class Action {
        UpdateGroup,
        UpdateMySettings,
        LeaveGroup
    };

    struct PendingOperation {
        Action action = Action::UpdateGroup;
        QString groupId;
        Group group;
    };

    explicit GroupRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRemoteDataSource)

    QString sendOperation(Action action,
                          const QString& path,
                          const QJsonObject& body,
                          PendingOperation pending,
                          HttpMethod method = HttpMethod::Patch,
                          const QVariantMap& query = {});
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, PendingOperation> m_pendingOperations;
};
