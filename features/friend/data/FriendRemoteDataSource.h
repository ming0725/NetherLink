#pragma once

#include "shared/network/NetworkTypes.h"
#include "shared/types/User.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

class FriendRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static FriendRemoteDataSource& instance();

    QString acceptFriendRequest(const QString& notificationId,
                                const QString& remark = {},
                                const QString& groupId = {},
                                const QString& groupName = {});
    QString rejectFriendRequest(const QString& notificationId);
    QString acceptGroupJoinRequest(const QString& notificationId,
                                   const QString& remark = {},
                                   const QString& categoryId = {},
                                   const QString& categoryName = {});
    QString rejectGroupJoinRequest(const QString& notificationId);
    QString createFriendRequest(const QString& toUserId, const QString& message);
    QString createGroupJoinRequest(const QString& groupId, const QString& message);
    QString updateFriend(const User& user);
    QString deleteFriend(const QString& userId);
    QString createFriendGroup(const QString& name, int sortOrder);
    QString updateFriendGroup(const QString& friendGroupId,
                              const QString& name = {},
                              int sortOrder = -1);
    QString deleteFriendGroup(const QString& friendGroupId);

signals:
    void friendRequestAccepted(const QString& requestId,
                               const QString& notificationId,
                               const QString& remark,
                               const QString& groupId,
                               const QString& groupName);
    void friendRequestRejected(const QString& requestId, const QString& notificationId);
    void groupJoinRequestAccepted(const QString& requestId,
                                  const QString& notificationId,
                                  const QString& remark,
                                  const QString& categoryId,
                                  const QString& categoryName);
    void groupJoinRequestRejected(const QString& requestId, const QString& notificationId);
    void friendRequestCreated(const QString& requestId, const QString& userId);
    void groupJoinRequestCreated(const QString& requestId, const QString& groupId);
    void friendUpdated(const QString& requestId, const User& user);
    void friendDeleted(const QString& requestId, const QString& userId);
    void friendGroupCreated(const QString& requestId, const QJsonObject& group);
    void friendGroupUpdated(const QString& requestId, const QJsonObject& group);
    void friendGroupDeleted(const QString& requestId, const QString& friendGroupId);
    void friendRequestCreateFailed(const QString& requestId,
                                   const QString& userId,
                                   const NetworkError& error);
    void groupJoinRequestCreateFailed(const QString& requestId,
                                      const QString& groupId,
                                      const NetworkError& error);
    void friendRequestActionFailed(const QString& requestId,
                                   const QString& notificationId,
                                   const NetworkError& error);
    void groupJoinRequestActionFailed(const QString& requestId,
                                      const QString& notificationId,
                                      const NetworkError& error);
    void friendUpdateFailed(const QString& requestId, const QString& userId, const NetworkError& error);
    void friendDeleteFailed(const QString& requestId, const QString& userId, const NetworkError& error);
    void friendGroupActionFailed(const QString& requestId,
                                 const QString& friendGroupId,
                                 const NetworkError& error);

private:
    enum class Action {
        AcceptFriendRequest,
        RejectFriendRequest,
        AcceptGroupJoinRequest,
        RejectGroupJoinRequest,
        CreateFriendRequest,
        CreateGroupJoinRequest,
        UpdateFriend,
        DeleteFriend,
        CreateFriendGroup,
        UpdateFriendGroup,
        DeleteFriendGroup
    };

    struct PendingOperation {
        Action action = Action::AcceptFriendRequest;
        QString notificationId;
        QString userId;
        QString remark;
        QString groupId;
        QString groupName;
        QString categoryId;
        QString categoryName;
        User user;
    };

    explicit FriendRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(FriendRemoteDataSource)

    QString sendOperation(Action action,
                          const QString& path,
                          const QJsonObject& body,
                          PendingOperation pending,
                          HttpMethod method = HttpMethod::Post,
                          const QString& idempotencyKey = {});
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, PendingOperation> m_pendingOperations;
};
