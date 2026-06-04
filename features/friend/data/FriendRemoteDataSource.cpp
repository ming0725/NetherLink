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
                                              PendingOperation pending)
{
    pending.action = action;
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post, path, body);
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
    case Action::DeleteFriend:
        emit friendDeleteFailed(requestId, pending.userId, error);
        break;
    }
}
