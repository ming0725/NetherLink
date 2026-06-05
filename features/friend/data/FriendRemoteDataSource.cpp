#include "FriendRemoteDataSource.h"

#include "shared/network/HttpClient.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

namespace {

QString newClientOperationId(const QString& prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject bodyWithClientOperationId(const QString& clientOperationId)
{
    return {
            {QStringLiteral("clientOperationId"), clientOperationId}
    };
}

QJsonValue backendFriendGroupId(const QString& groupId)
{
    if (groupId.isEmpty() || groupId == QStringLiteral("default")) {
        return QJsonValue(QJsonValue::Null);
    }
    return QJsonValue(groupId);
}

} // namespace

FriendRemoteDataSource& FriendRemoteDataSource::instance()
{
    static FriendRemoteDataSource dataSource;
    return dataSource;
}

FriendRemoteDataSource::FriendRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &FriendRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &FriendRemoteDataSource::handleRequestFailed);
}

QString FriendRemoteDataSource::acceptFriendRequest(const QString& notificationId,
                                                    const QString& remark,
                                                    const QString& groupId,
                                                    const QString& groupName)
{
    if (notificationId.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_friend_accept"));
    QJsonObject body = bodyWithClientOperationId(clientOperationId);
    body.insert(QStringLiteral("remark"), remark);
    body.insert(QStringLiteral("groupId"),
                groupId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(groupId));

    PendingOperation pending;
    pending.action = Action::AcceptFriendRequest;
    pending.notificationId = notificationId;
    pending.remark = remark;
    pending.groupId = groupId;
    pending.groupName = groupName;
    return sendOperation(Action::AcceptFriendRequest,
                         QStringLiteral("/friend-requests/%1/accept").arg(notificationId),
                         body,
                         pending);
}

QString FriendRemoteDataSource::rejectFriendRequest(const QString& notificationId)
{
    if (notificationId.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::RejectFriendRequest;
    pending.notificationId = notificationId;
    return sendOperation(Action::RejectFriendRequest,
                         QStringLiteral("/friend-requests/%1/reject").arg(notificationId),
                         bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_friend_reject"))),
                         pending);
}

QString FriendRemoteDataSource::acceptGroupJoinRequest(const QString& notificationId,
                                                       const QString& remark,
                                                       const QString& categoryId,
                                                       const QString& categoryName)
{
    if (notificationId.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::AcceptGroupJoinRequest;
    pending.notificationId = notificationId;
    pending.remark = remark;
    pending.categoryId = categoryId;
    pending.categoryName = categoryName;
    return sendOperation(Action::AcceptGroupJoinRequest,
                         QStringLiteral("/group-join-requests/%1/accept").arg(notificationId),
                         bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_join_accept"))),
                         pending);
}

QString FriendRemoteDataSource::rejectGroupJoinRequest(const QString& notificationId)
{
    if (notificationId.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::RejectGroupJoinRequest;
    pending.notificationId = notificationId;
    return sendOperation(Action::RejectGroupJoinRequest,
                         QStringLiteral("/group-join-requests/%1/reject").arg(notificationId),
                         bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_join_reject"))),
                         pending);
}

QString FriendRemoteDataSource::createFriendRequest(const QString& toUserUuid, const QString& message)
{
    if (toUserUuid.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_friend_request")));
    body.insert(QStringLiteral("toUserUuid"), toUserUuid);
    body.insert(QStringLiteral("message"), message);
    body.insert(QStringLiteral("sourceType"), QStringLiteral("search"));
    body.insert(QStringLiteral("sourceGroupId"), QJsonValue(QJsonValue::Null));
    body.insert(QStringLiteral("sourceFriendUuid"), QJsonValue(QJsonValue::Null));

    PendingOperation pending;
    pending.action = Action::CreateFriendRequest;
    pending.userId = toUserUuid;
    return sendOperation(Action::CreateFriendRequest,
                         QStringLiteral("/friend-requests"),
                         body,
                         pending);
}

QString FriendRemoteDataSource::createGroupJoinRequest(const QString& groupId, const QString& message)
{
    if (groupId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_join_request")));
    body.insert(QStringLiteral("groupId"), groupId);
    body.insert(QStringLiteral("message"), message);

    PendingOperation pending;
    pending.action = Action::CreateGroupJoinRequest;
    pending.groupId = groupId;
    return sendOperation(Action::CreateGroupJoinRequest,
                         QStringLiteral("/group-join-requests"),
                         body,
                         pending);
}

QString FriendRemoteDataSource::updateFriend(const User& user)
{
    if (user.id.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_friend_update")));
    body.insert(QStringLiteral("remark"), user.remark);
    body.insert(QStringLiteral("groupId"), backendFriendGroupId(user.friendGroupId));
    body.insert(QStringLiteral("isDnd"), user.isDnd);

    PendingOperation pending;
    pending.action = Action::UpdateFriend;
    pending.userId = user.id;
    pending.user = user;
    return sendOperation(Action::UpdateFriend,
                         QStringLiteral("/friends/%1").arg(user.id),
                         body,
                         pending,
                         HttpMethod::Patch);
}

QString FriendRemoteDataSource::deleteFriend(const QString& userId)
{
    if (userId.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_friend_delete"));
    PendingOperation pending;
    pending.action = Action::DeleteFriend;
    pending.userId = userId;

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/friends/%1").arg(userId),
            {},
            {{QStringLiteral("clientOperationId"), clientOperationId}});
    request.headers.insert("Idempotency-Key", clientOperationId.toUtf8());
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

QString FriendRemoteDataSource::sendOperation(Action action,
                                              const QString& path,
                                              const QJsonObject& body,
                                              PendingOperation pending,
                                              HttpMethod method)
{
    pending.action = action;
    NetworkRequest request = NetworkRequest::json(method, path, body);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

void FriendRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse&)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::AcceptFriendRequest:
        emit friendRequestAccepted(requestId,
                                   pending.notificationId,
                                   pending.remark,
                                   pending.groupId,
                                   pending.groupName);
        break;
    case Action::RejectFriendRequest:
        emit friendRequestRejected(requestId, pending.notificationId);
        break;
    case Action::AcceptGroupJoinRequest:
        emit groupJoinRequestAccepted(requestId,
                                      pending.notificationId,
                                      pending.remark,
                                      pending.categoryId,
                                      pending.categoryName);
        break;
    case Action::RejectGroupJoinRequest:
        emit groupJoinRequestRejected(requestId, pending.notificationId);
        break;
    case Action::CreateFriendRequest:
        emit friendRequestCreated(requestId, pending.userId);
        break;
    case Action::CreateGroupJoinRequest:
        emit groupJoinRequestCreated(requestId, pending.groupId);
        break;
    case Action::UpdateFriend:
        emit friendUpdated(requestId, pending.user);
        break;
    case Action::DeleteFriend:
        emit friendDeleted(requestId, pending.userId);
        break;
    }
}

void FriendRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::AcceptFriendRequest:
    case Action::RejectFriendRequest:
        emit friendRequestActionFailed(requestId, pending.notificationId, error);
        break;
    case Action::AcceptGroupJoinRequest:
    case Action::RejectGroupJoinRequest:
        emit groupJoinRequestActionFailed(requestId, pending.notificationId, error);
        break;
    case Action::CreateFriendRequest:
        emit friendRequestCreateFailed(requestId, pending.userId, error);
        break;
    case Action::CreateGroupJoinRequest:
        emit groupJoinRequestCreateFailed(requestId, pending.groupId, error);
        break;
    case Action::UpdateFriend:
        emit friendUpdateFailed(requestId, pending.userId, error);
        break;
    case Action::DeleteFriend:
        emit friendDeleteFailed(requestId, pending.userId, error);
        break;
    }
}
