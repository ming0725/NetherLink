#pragma once

#include "shared/network/NetworkTypes.h"
#include "shared/types/Group.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class GroupRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static GroupRemoteDataSource& instance();

    QString createGroup(const QString& name, const QStringList& memberIds);
    QString updateGroup(const Group& group);
    QString updateMySettings(const Group& group);
    QString leaveGroup(const QString& groupId, const QString& currentUserUuid);
    QString addMembers(const Group& group, const QStringList& userIds);
    QString updateMemberNickname(const Group& group, const QString& userId, const QString& nickname);
    QString setMemberAdmin(const Group& group, const QString& userId, bool admin);
    QString removeMember(const Group& group, const QString& userId);
    QStringList removeMembers(const Group& group, const QStringList& userIds);
    QString transferOwner(const Group& group, const QString& userId);

signals:
    void groupCreated(const QString& requestId, const Group& group);
    void groupUpdated(const QString& requestId, const Group& group);
    void groupMySettingsUpdated(const QString& requestId, const Group& group);
    void groupLeft(const QString& requestId, const QString& groupId);
    void groupCreateFailed(const QString& requestId, const NetworkError& error);
    void groupUpdateFailed(const QString& requestId, const QString& groupId, const NetworkError& error);
    void groupMySettingsUpdateFailed(const QString& requestId, const QString& groupId, const NetworkError& error);
    void groupLeaveFailed(const QString& requestId, const QString& groupId, const NetworkError& error);

private:
    enum class Action {
        CreateGroup,
        UpdateGroup,
        UpdateMySettings,
        LeaveGroup,
        AddMembers,
        UpdateMember,
        RemoveMember,
        RemoveMembers,
        TransferOwner
    };

    struct PendingOperation {
        Action action = Action::UpdateGroup;
        QString groupId;
        Group group;
        QString batchId;
    };

    struct PendingBatch {
        QString groupId;
        Group group;
        QString representativeRequestId;
        QStringList requestIds;
        int remaining = 0;
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
    QHash<QString, PendingBatch> m_pendingBatches;
};
