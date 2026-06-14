#include "FriendRemoteDataSource.h"

#include "features/friend/data/FriendNotificationRepository.h"
#include "shared/network/HttpClient.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
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
        return QString();
    }
    return QJsonValue(groupId);
}

bool isDefaultFriendGroup(const QString& groupId)
{
    return groupId.isEmpty() || groupId == QStringLiteral("default");
}

bool isGroupPublicId(const QString& value)
{
    static const QRegularExpression tenDigitId(QStringLiteral("^\\d{10}$"));
    return tenDigitId.match(value.trimmed()).hasMatch();
}

QString handledStatusFromDetails(const NetworkError& error)
{
    const QJsonObject request = error.details.value(QStringLiteral("request")).toObject();
    QString status = request.value(QStringLiteral("status")).toString();
    if (status.isEmpty()) {
        status = error.details.value(QStringLiteral("currentStatus")).toString();
    }
    return status.trimmed().toLower();
}

bool statusFromBackend(const QString& status, NotificationStatus* out)
{
    if (!out) {
        return false;
    }
    if (status == QStringLiteral("accepted")) {
        *out = NotificationStatus::Accepted;
        return true;
    }
    if (status == QStringLiteral("rejected")) {
        *out = NotificationStatus::Rejected;
        return true;
    }
    if (status == QStringLiteral("pending")) {
        *out = NotificationStatus::Pending;
        return true;
    }
    return false;
}

QString requestIdFromDetails(const NetworkError& error, const QString& fallback)
{
    const QJsonObject request = error.details.value(QStringLiteral("request")).toObject();
    for (const QString& key : {QStringLiteral("requestId"),
                               QStringLiteral("id"),
                               QStringLiteral("notificationId")}) {
        const QString requestId = request.value(key).toString();
        if (!requestId.isEmpty()) {
            return requestId;
        }
    }
    return fallback;
}

QJsonObject responseObjectForKey(const NetworkResponse& response, const QString& key)
{
    const QJsonObject root = response.object();
    const QJsonObject nested = root.value(key).toObject();
    return nested.isEmpty() ? root : nested;
}

bool syncStaleFriendRequestCache(const QString& notificationId, const NetworkError& error)
{
    const QString requestId = requestIdFromDetails(error, notificationId);
    if (requestId.isEmpty()) {
        return false;
    }

    if (error.httpStatus == 404 || error.code == QStringLiteral("NOT_FOUND")) {
        return FriendNotificationRepository::instance().removeRequest(requestId);
    }

    const QString actorRole = error.details.value(QStringLiteral("actorRole")).toString().trimmed().toLower();
    if (error.code == QStringLiteral("FRIEND_REQUEST_NOT_RECIPIENT") &&
        actorRole == QStringLiteral("sender")) {
        return FriendNotificationRepository::instance().removeRequest(requestId);
    }

    if (error.code != QStringLiteral("FRIEND_REQUEST_ALREADY_HANDLED")) {
        return false;
    }

    NotificationStatus status = NotificationStatus::Pending;
    if (!statusFromBackend(handledStatusFromDetails(error), &status)) {
        return false;
    }
    return FriendNotificationRepository::instance().syncRequestStatus(requestId, status);
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
    if (!remark.isEmpty()) {
        body.insert(QStringLiteral("remark"), remark);
    }
    if (!isDefaultFriendGroup(groupId)) {
        body.insert(QStringLiteral("friendGroupId"), groupId);
    }

    PendingOperation pending;
    pending.action = Action::AcceptFriendRequest;
    pending.notificationId = notificationId;
    pending.remark = remark;
    pending.groupId = isDefaultFriendGroup(groupId) ? QStringLiteral("default") : groupId;
    pending.groupName = groupName.isEmpty() ? QStringLiteral("默认分组") : groupName;
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

QString FriendRemoteDataSource::createFriendRequest(const QString& toUserId, const QString& message)
{
    if (toUserId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_friend_request")));
    body.insert(QStringLiteral("toUserId"), toUserId);
    body.insert(QStringLiteral("message"), message);
    body.insert(QStringLiteral("sourceType"), QStringLiteral("search"));
    body.insert(QStringLiteral("sourceGroupId"), QJsonValue(QJsonValue::Null));
    body.insert(QStringLiteral("sourceFriendUuid"), QJsonValue(QJsonValue::Null));

    PendingOperation pending;
    pending.action = Action::CreateFriendRequest;
    pending.userId = toUserId;
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
    if (isGroupPublicId(groupId)) {
        body.insert(QStringLiteral("groupPublicId"), groupId.trimmed());
    } else {
        body.insert(QStringLiteral("groupId"), groupId);
    }
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
    body.insert(QStringLiteral("friendGroupId"), backendFriendGroupId(user.friendGroupId));
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

QString FriendRemoteDataSource::createFriendGroup(const QString& name, int sortOrder)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_friend_group_create"));
    QJsonObject body = bodyWithClientOperationId(clientOperationId);
    body.insert(QStringLiteral("name"), trimmedName);
    body.insert(QStringLiteral("sortOrder"), sortOrder);

    PendingOperation pending;
    pending.action = Action::CreateFriendGroup;
    pending.groupName = trimmedName;
    return sendOperation(Action::CreateFriendGroup,
                         QStringLiteral("/friend-groups"),
                         body,
                         pending,
                         HttpMethod::Post,
                         clientOperationId);
}

QString FriendRemoteDataSource::updateFriendGroup(const QString& friendGroupId,
                                                  const QString& name,
                                                  int sortOrder)
{
    if (friendGroupId.isEmpty() || friendGroupId == QStringLiteral("default")) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_friend_group_update"));
    QJsonObject body = bodyWithClientOperationId(clientOperationId);
    const QString trimmedName = name.trimmed();
    if (!trimmedName.isEmpty()) {
        body.insert(QStringLiteral("name"), trimmedName);
    }
    if (sortOrder >= 0) {
        body.insert(QStringLiteral("sortOrder"), sortOrder);
    }
    if (!body.contains(QStringLiteral("name")) && !body.contains(QStringLiteral("sortOrder"))) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::UpdateFriendGroup;
    pending.groupId = friendGroupId;
    pending.groupName = trimmedName;
    return sendOperation(Action::UpdateFriendGroup,
                         QStringLiteral("/friend-groups/%1").arg(friendGroupId),
                         body,
                         pending,
                         HttpMethod::Patch,
                         clientOperationId);
}

QString FriendRemoteDataSource::deleteFriendGroup(const QString& friendGroupId)
{
    if (friendGroupId.isEmpty() || friendGroupId == QStringLiteral("default")) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_friend_group_delete"));
    PendingOperation pending;
    pending.action = Action::DeleteFriendGroup;
    pending.groupId = friendGroupId;

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/friend-groups/%1").arg(friendGroupId),
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
                                              HttpMethod method,
                                              const QString& idempotencyKey)
{
    pending.action = action;
    NetworkRequest request = NetworkRequest::json(method, path, body);
    if (!idempotencyKey.isEmpty()) {
        request.headers.insert("Idempotency-Key", idempotencyKey.toUtf8());
    }
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

void FriendRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
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
        {
            User user = pending.user;
            const QJsonObject friendship = responseObjectForKey(response, QStringLiteral("friendship"));
            if (!friendship.isEmpty()) {
                const QJsonObject nestedUser = friendship.value(QStringLiteral("user")).toObject();
                const QString responseUserUuid = nestedUser.value(QStringLiteral("userUuid")).toString(
                        friendship.value(QStringLiteral("friendUserUuid")).toString());
                if (!responseUserUuid.isEmpty()) {
                    user.id = responseUserUuid;
                    user.userUuid = responseUserUuid;
                }
                const QString responseUserId = nestedUser.value(QStringLiteral("userId")).toString();
                if (!responseUserId.isEmpty()) {
                    user.userId = responseUserId;
                }
                const QString responseNick = nestedUser.value(QStringLiteral("nickName")).toString(
                        nestedUser.value(QStringLiteral("nick")).toString());
                if (!responseNick.isEmpty()) {
                    user.nick = responseNick;
                }
                user.remark = friendship.value(QStringLiteral("remark")).toString(pending.user.remark);
                user.friendGroupId = friendship.value(QStringLiteral("friendGroupId")).toString(pending.user.friendGroupId);
                user.friendGroupName = friendship.value(QStringLiteral("friendGroupName")).toString(pending.user.friendGroupName);
                user.isDnd = friendship.value(QStringLiteral("isDnd")).toBool(pending.user.isDnd);
                user.isFriend = true;
                user.version = friendship.value(QStringLiteral("version")).toInt(pending.user.version);
            }
            emit friendUpdated(requestId, user);
        }
        break;
    case Action::DeleteFriend:
        emit friendDeleted(requestId, pending.userId);
        break;
    case Action::CreateFriendGroup:
        emit friendGroupCreated(requestId, responseObjectForKey(response, QStringLiteral("group")));
        break;
    case Action::UpdateFriendGroup:
        emit friendGroupUpdated(requestId, responseObjectForKey(response, QStringLiteral("group")));
        break;
    case Action::DeleteFriendGroup:
        emit friendGroupDeleted(requestId, pending.groupId);
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
        if (syncStaleFriendRequestCache(pending.notificationId, error)) {
            return;
        }
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
    case Action::CreateFriendGroup:
    case Action::UpdateFriendGroup:
    case Action::DeleteFriendGroup:
        emit friendGroupActionFailed(requestId, pending.groupId, error);
        break;
    }
}
