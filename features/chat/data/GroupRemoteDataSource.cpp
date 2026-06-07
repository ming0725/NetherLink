#include "GroupRemoteDataSource.h"

#include "shared/data/LocalDataStore.h"
#include "shared/network/HttpClient.h"
#include "shared/services/AvatarSource.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
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

QString normalizedListGroupId(const Group& group)
{
    return group.listGroupId.isEmpty() ? QStringLiteral("gg_joined") : group.listGroupId;
}

QString normalizedListGroupName(const Group& group)
{
    return group.listGroupName.isEmpty() ? QStringLiteral("我加入的群聊") : group.listGroupName;
}

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString avatarFileIdFrom(const QJsonObject& object)
{
    const QJsonObject avatar = object.value(QStringLiteral("avatar")).toObject();
    const QString nestedFileId = firstString(avatar, {QStringLiteral("fileId"), QStringLiteral("id")});
    if (!nestedFileId.isEmpty()) {
        return nestedFileId;
    }

    return firstString(object, {QStringLiteral("avatarFileId"),
                                QStringLiteral("avatar_file_id"),
                                QStringLiteral("fileId")});
}

QVector<QString> stringVectorFromJson(const QJsonArray& array)
{
    QVector<QString> values;
    values.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QString text = value.toString();
        if (!text.isEmpty()) {
            values.push_back(text);
            continue;
        }

        const QJsonObject object = value.toObject();
        const QString id = firstString(object, {QStringLiteral("userId"),
                                                QStringLiteral("userUuid"),
                                                QStringLiteral("id")});
        if (!id.isEmpty()) {
            values.push_back(id);
        }
    }
    return values;
}

QMap<QString, QString> stringMapFromJson(const QJsonObject& object)
{
    QMap<QString, QString> map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        map.insert(it.key(), it.value().toString());
    }
    return map;
}

QJsonArray arrayFromResponse(const QJsonObject& object, const QString& primaryKey)
{
    const QStringList keys = {primaryKey,
                              QStringLiteral("items"),
                              QStringLiteral("data"),
                              QStringLiteral("results")};
    for (const QString& key : keys) {
        const QJsonArray array = object.value(key).toArray();
        if (!array.isEmpty()) {
            return array;
        }
    }
    return {};
}

QString avatarSourceFromObject(const QJsonObject& object, const QString& pathKey)
{
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = firstString(object, {pathKey, QStringLiteral("avatarUrl")});
    }
    return AvatarSource::versioned(avatarPath,
                                   object.value(QStringLiteral("avatarVersion")).toInt(),
                                   object.value(QStringLiteral("avatarEtag")).toString(),
                                   object.value(QStringLiteral("avatarContentHash")).toString());
}

UserStatus userStatusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("online") ||
        normalized == QStringLiteral("active")) {
        return Online;
    }
    if (normalized == QStringLiteral("mining") ||
        normalized == QStringLiteral("busy") ||
        normalized == QStringLiteral("dnd")) {
        return Mining;
    }
    if (normalized == QStringLiteral("airplane") ||
        normalized == QStringLiteral("flying") ||
        normalized == QStringLiteral("away")) {
        return Flying;
    }
    if (normalized == QStringLiteral("invisible")) {
        return Invisible;
    }
    return Offline;
}

QJsonObject presenceObjectFrom(const QJsonObject& object)
{
    const QJsonObject presence = object.value(QStringLiteral("presence")).toObject();
    if (!presence.isEmpty()) {
        return presence;
    }

    QJsonObject legacyPresence;
    if (object.contains(QStringLiteral("status"))) {
        legacyPresence.insert(QStringLiteral("status"), object.value(QStringLiteral("status")));
    }
    if (object.contains(QStringLiteral("lastSeenAt"))) {
        legacyPresence.insert(QStringLiteral("lastSeenAt"), object.value(QStringLiteral("lastSeenAt")));
    }
    return legacyPresence;
}

QDateTime dateTimeFromString(const QString& value)
{
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
}

QJsonObject memberUserObject(QJsonObject object)
{
    const QJsonObject wrapper = object;
    const QJsonObject nestedUser = object.value(QStringLiteral("user")).toObject();
    if (!nestedUser.isEmpty()) {
        object = nestedUser;
    }
    const QString userUuid = firstString(object, {QStringLiteral("userUuid"),
                                                  QStringLiteral("uuid"),
                                                  QStringLiteral("id")});
    const QString userId = firstString(object, {QStringLiteral("userId"),
                                                QStringLiteral("publicId")});
    const QString modelId = userId.isEmpty() ? userUuid : userId;
    object.insert(QStringLiteral("id"), modelId);
    if (!userUuid.isEmpty()) {
        object.insert(QStringLiteral("userUuid"), userUuid);
    }
    if (!userId.isEmpty()) {
        object.insert(QStringLiteral("userId"), userId);
    }
    if (!object.contains(QStringLiteral("nick"))) {
        object.insert(QStringLiteral("nick"),
                      firstString(object, {QStringLiteral("nickName"), QStringLiteral("displayName")}));
    }
    if (wrapper.contains(QStringLiteral("nickname")) && !object.contains(QStringLiteral("remark"))) {
        object.insert(QStringLiteral("remark"), wrapper.value(QStringLiteral("nickname")));
    }
    for (const QString& key : {QStringLiteral("presence"),
                               QStringLiteral("status"),
                               QStringLiteral("lastSeenAt")}) {
        if (wrapper.contains(key) && !object.contains(key)) {
            object.insert(key, wrapper.value(key));
        }
    }
    return object;
}

User userFromMemberObject(const QJsonObject& source)
{
    const QJsonObject object = memberUserObject(source);
    User user;
    user.id = object.value(QStringLiteral("id")).toString();
    user.nick = object.value(QStringLiteral("nick")).toString();
    user.remark = object.value(QStringLiteral("remark")).toString();
    user.avatarPath = avatarSourceFromObject(object, QStringLiteral("avatarPath"));
    const QJsonObject presence = presenceObjectFrom(object);
    user.status = userStatusFromString(presence.value(QStringLiteral("status")).toString());
    user.lastSeenAt = dateTimeFromString(presence.value(QStringLiteral("lastSeenAt")).toString());
    user.signature = object.value(QStringLiteral("signature")).toString();
    user.region = object.value(QStringLiteral("region")).toString();
    return user;
}

void cacheMemberUserObject(const QJsonObject& source)
{
    const QJsonObject object = memberUserObject(source);
    const QString key = object.value(QStringLiteral("id")).toString();
    if (!key.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("users"), key, object);
    }
}

Group groupFromResponseObject(QJsonObject object)
{
    if (object.value(QStringLiteral("group")).isObject()) {
        object = object.value(QStringLiteral("group")).toObject();
    }

    Group group;
    group.groupId = firstString(object, {QStringLiteral("groupId"), QStringLiteral("id")});
    group.groupName = firstString(object, {QStringLiteral("groupName"), QStringLiteral("name")});
    group.memberNum = object.value(QStringLiteral("memberNum")).toInt(
            object.value(QStringLiteral("memberCount")).toInt());
    group.ownerId = firstString(object, {QStringLiteral("ownerId"), QStringLiteral("ownerUuid")});
    group.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    group.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    group.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = firstString(object, {QStringLiteral("groupAvatarPath"),
                                          QStringLiteral("avatarUrl")});
    }
    group.groupAvatarPath = AvatarSource::versioned(avatarPath,
                                                    group.avatarVersion,
                                                    group.avatarEtag,
                                                    group.avatarContentHash);
    group.isDnd = object.value(QStringLiteral("isDnd")).toBool(false);
    group.adminsID = stringVectorFromJson(object.value(QStringLiteral("adminsID")).toArray());
    group.remark = object.value(QStringLiteral("remark")).toString();
    group.introduction = object.value(QStringLiteral("introduction")).toString();
    group.announcement = object.value(QStringLiteral("announcement")).toString();
    group.currentUserNickname = object.value(QStringLiteral("currentUserNickname")).toString();
    group.memberNicknames = stringMapFromJson(object.value(QStringLiteral("memberNicknames")).toObject());
    group.membersID = stringVectorFromJson(object.value(QStringLiteral("membersID")).toArray());
    if (group.membersID.isEmpty()) {
        group.membersID = stringVectorFromJson(object.value(QStringLiteral("members")).toArray());
    }
    group.listGroupId = object.value(QStringLiteral("listGroupId")).toString(QStringLiteral("gg_joined"));
    group.listGroupName = object.value(QStringLiteral("listGroupName")).toString(QStringLiteral("我加入的群聊"));
    if (group.memberNum <= 0 && !group.membersID.isEmpty()) {
        group.memberNum = group.membersID.size();
    }
    return group;
}

} // namespace

GroupRemoteDataSource& GroupRemoteDataSource::instance()
{
    static GroupRemoteDataSource dataSource;
    return dataSource;
}

GroupRemoteDataSource::GroupRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &GroupRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &GroupRemoteDataSource::handleRequestFailed);
}

QString GroupRemoteDataSource::createGroup(const QString& name, const QStringList& memberIds)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || memberIds.isEmpty()) {
        return {};
    }

    QJsonArray memberArray;
    QSet<QString> seen;
    for (const QString& memberId : memberIds) {
        const QString normalizedId = memberId.trimmed();
        if (normalizedId.isEmpty() || seen.contains(normalizedId)) {
            continue;
        }
        seen.insert(normalizedId);
        memberArray.append(normalizedId);
    }
    if (memberArray.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_create")));
    body.insert(QStringLiteral("name"), trimmedName);
    body.insert(QStringLiteral("memberIds"), memberArray);

    PendingOperation pending;
    pending.action = Action::CreateGroup;
    pending.group.groupName = trimmedName;
    pending.group.memberNum = memberArray.size();
    for (const QJsonValue& value : memberArray) {
        pending.group.membersID.push_back(value.toString());
    }

    return sendOperation(Action::CreateGroup,
                         QStringLiteral("/groups"),
                         body,
                         pending,
                         HttpMethod::Post);
}

QString GroupRemoteDataSource::updateGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_update")));
    body.insert(QStringLiteral("name"), group.groupName);
    body.insert(QStringLiteral("introduction"), group.introduction);
    body.insert(QStringLiteral("announcement"), group.announcement);

    PendingOperation pending;
    pending.action = Action::UpdateGroup;
    pending.groupId = group.groupId;
    pending.group = group;
    return sendOperation(Action::UpdateGroup,
                         QStringLiteral("/groups/%1").arg(group.groupId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::updateMySettings(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_settings")));
    body.insert(QStringLiteral("remark"), group.remark);
    body.insert(QStringLiteral("listGroupId"), normalizedListGroupId(group));
    body.insert(QStringLiteral("listGroupName"), normalizedListGroupName(group));
    body.insert(QStringLiteral("isDnd"), group.isDnd);

    PendingOperation pending;
    pending.action = Action::UpdateMySettings;
    pending.groupId = group.groupId;
    pending.group = group;
    return sendOperation(Action::UpdateMySettings,
                         QStringLiteral("/groups/%1/my-settings").arg(group.groupId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::leaveGroup(const QString& groupId, const QString& currentUserUuid)
{
    if (groupId.isEmpty() || currentUserUuid.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_group_leave"));
    PendingOperation pending;
    pending.action = Action::LeaveGroup;
    pending.groupId = groupId;

    QVariantMap query;
    query.insert(QStringLiteral("clientOperationId"), clientOperationId);
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/groups/%1/members/%2").arg(groupId, currentUserUuid),
            {},
            query);
    request.headers.insert("Idempotency-Key", clientOperationId.toUtf8());
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

QString GroupRemoteDataSource::addMembers(const Group& group, const QStringList& userIds)
{
    if (group.groupId.isEmpty() || userIds.isEmpty()) {
        return {};
    }

    QJsonArray userArray;
    QSet<QString> seen;
    for (const QString& userId : userIds) {
        const QString normalizedId = userId.trimmed();
        if (normalizedId.isEmpty() || seen.contains(normalizedId)) {
            continue;
        }
        seen.insert(normalizedId);
        userArray.append(normalizedId);
    }
    if (userArray.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_members_add")));
    body.insert(QStringLiteral("userUuids"), userArray);

    PendingOperation pending;
    pending.action = Action::AddMembers;
    pending.groupId = group.groupId;
    pending.group = group;
    for (const QJsonValue& value : userArray) {
        const QString userId = value.toString();
        if (!pending.group.membersID.contains(userId)) {
            pending.group.membersID.push_back(userId);
        }
    }
    pending.group.memberNum = pending.group.membersID.size();
    return sendOperation(Action::AddMembers,
                         QStringLiteral("/groups/%1/members").arg(group.groupId),
                         body,
                         pending,
                         HttpMethod::Post);
}

QString GroupRemoteDataSource::updateMemberNickname(const Group& group,
                                                    const QString& userId,
                                                    const QString& nickname)
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return {};
    }

    Group next = group;
    const QString nextNickname = nickname.trimmed();
    if (nextNickname.isEmpty()) {
        next.memberNicknames.remove(userId);
    } else {
        next.memberNicknames.insert(userId, nextNickname);
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_member_update")));
    body.insert(QStringLiteral("nickname"), nextNickname);

    PendingOperation pending;
    pending.action = Action::UpdateMember;
    pending.groupId = group.groupId;
    pending.group = next;
    return sendOperation(Action::UpdateMember,
                         QStringLiteral("/groups/%1/members/%2").arg(group.groupId, userId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::setMemberAdmin(const Group& group, const QString& userId, bool admin)
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return {};
    }

    Group next = group;
    if (admin && !next.adminsID.contains(userId)) {
        next.adminsID.push_back(userId);
    } else if (!admin) {
        next.adminsID.removeAll(userId);
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_member_role")));
    body.insert(QStringLiteral("role"), admin ? QStringLiteral("admin") : QStringLiteral("member"));

    PendingOperation pending;
    pending.action = Action::UpdateMember;
    pending.groupId = group.groupId;
    pending.group = next;
    return sendOperation(Action::UpdateMember,
                         QStringLiteral("/groups/%1/members/%2").arg(group.groupId, userId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::removeMember(const Group& group, const QString& userId)
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_group_member_remove"));
    Group next = group;
    next.membersID.removeAll(userId);
    next.adminsID.removeAll(userId);
    next.memberNicknames.remove(userId);
    next.memberNum = next.membersID.size();

    PendingOperation pending;
    pending.action = Action::RemoveMember;
    pending.groupId = group.groupId;
    pending.group = next;

    QVariantMap query;
    query.insert(QStringLiteral("clientOperationId"), clientOperationId);
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/groups/%1/members/%2").arg(group.groupId, userId),
            {},
            query);
    request.headers.insert("Idempotency-Key", clientOperationId.toUtf8());
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

QStringList GroupRemoteDataSource::removeMembers(const Group& group, const QStringList& userIds)
{
    if (group.groupId.isEmpty() || userIds.isEmpty()) {
        return {};
    }

    QStringList normalizedIds;
    QSet<QString> seen;
    for (const QString& userId : userIds) {
        const QString normalizedId = userId.trimmed();
        if (normalizedId.isEmpty() || seen.contains(normalizedId)) {
            continue;
        }
        seen.insert(normalizedId);
        normalizedIds.push_back(normalizedId);
    }
    if (normalizedIds.isEmpty()) {
        return {};
    }

    Group next = group;
    for (const QString& userId : std::as_const(normalizedIds)) {
        next.membersID.removeAll(userId);
        next.adminsID.removeAll(userId);
        next.memberNicknames.remove(userId);
    }
    next.memberNum = next.membersID.size();

    const QString batchId = newClientOperationId(QStringLiteral("op_group_members_remove_batch"));
    PendingBatch batch;
    batch.groupId = group.groupId;
    batch.group = next;
    batch.remaining = normalizedIds.size();

    for (const QString& userId : std::as_const(normalizedIds)) {
        const QString clientOperationId = newClientOperationId(QStringLiteral("op_group_member_remove"));
        PendingOperation pending;
        pending.action = Action::RemoveMembers;
        pending.groupId = group.groupId;
        pending.group = next;
        pending.batchId = batchId;

        QVariantMap query;
        query.insert(QStringLiteral("clientOperationId"), clientOperationId);
        NetworkRequest request = NetworkRequest::json(
                HttpMethod::Delete,
                QStringLiteral("/groups/%1/members/%2").arg(group.groupId, userId),
                {},
                query);
        request.headers.insert("Idempotency-Key", clientOperationId.toUtf8());
        request.maxRetries = 3;
        const QString requestId = HttpClient::instance().send(request);
        if (batch.representativeRequestId.isEmpty()) {
            batch.representativeRequestId = requestId;
        }
        batch.requestIds.push_back(requestId);
        m_pendingOperations.insert(requestId, pending);
    }

    m_pendingBatches.insert(batchId, batch);
    return batch.requestIds;
}

QString GroupRemoteDataSource::transferOwner(const Group& group, const QString& userId)
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return {};
    }

    Group next = group;
    next.adminsID.removeAll(userId);
    if (!next.ownerId.isEmpty() && !next.adminsID.contains(next.ownerId)) {
        next.adminsID.push_back(next.ownerId);
    }
    next.ownerId = userId;

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_owner_transfer")));
    body.insert(QStringLiteral("userUuid"), userId);

    PendingOperation pending;
    pending.action = Action::TransferOwner;
    pending.groupId = group.groupId;
    pending.group = next;
    return sendOperation(Action::TransferOwner,
                         QStringLiteral("/groups/%1/transfer-owner").arg(group.groupId),
                         body,
                         pending,
                         HttpMethod::Post);
}

QString GroupRemoteDataSource::fetchMembers(const QString& groupId, const QString& keyword, int offset, int limit)
{
    if (groupId.isEmpty() || offset < 0 || limit <= 0) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::FetchMembers;
    pending.groupId = groupId;
    pending.keyword = keyword.trimmed();
    pending.offset = offset;
    pending.limit = limit;

    QVariantMap query;
    query.insert(QStringLiteral("keyword"), pending.keyword);
    query.insert(QStringLiteral("offset"), offset);
    query.insert(QStringLiteral("limit"), limit);
    return sendOperation(Action::FetchMembers,
                         QStringLiteral("/groups/%1/members").arg(groupId),
                         {},
                         pending,
                         HttpMethod::Get,
                         query);
}

QString GroupRemoteDataSource::sendOperation(Action action,
                                             const QString& path,
                                             const QJsonObject& body,
                                             PendingOperation pending,
                                             HttpMethod method,
                                             const QVariantMap& query)
{
    pending.action = action;
    NetworkRequest request = NetworkRequest::json(method, path, body, query);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

void GroupRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::FetchMembers: {
        const QJsonObject root = response.object();
        const QJsonArray items = arrayFromResponse(root, QStringLiteral("members"));
        QVector<User> members;
        members.reserve(items.size());
        for (const QJsonValue& value : items) {
            const QJsonObject object = value.toObject();
            cacheMemberUserObject(object);
            const User user = userFromMemberObject(object);
            if (!user.id.isEmpty()) {
                members.push_back(user);
            }
        }
        const int total = root.value(QStringLiteral("total")).toInt(pending.offset + members.size());
        const bool hasMore = root.value(QStringLiteral("hasMore")).toBool(pending.offset + members.size() < total);
        emit groupMembersFetched(requestId,
                                 pending.groupId,
                                 pending.keyword,
                                 pending.offset,
                                 pending.limit,
                                 members,
                                 total,
                                 hasMore);
        break;
    }
    case Action::CreateGroup: {
        Group group = groupFromResponseObject(response.object());
        if (group.groupId.isEmpty()) {
            NetworkError error;
            error.code = QStringLiteral("GROUP_ID_MISSING");
            error.message = QStringLiteral("Create group response did not include groupId.");
            emit groupCreateFailed(requestId, error);
            return;
        }
        if (group.groupName.isEmpty()) {
            group.groupName = pending.group.groupName;
        }
        if (group.membersID.isEmpty()) {
            group.membersID = pending.group.membersID;
        }
        if (group.memberNum <= 0) {
            group.memberNum = qMax(1, group.membersID.size());
        }
        emit groupCreated(requestId, group);
        break;
    }
    case Action::UpdateGroup:
        emit groupUpdated(requestId, pending.group);
        break;
    case Action::AddMembers:
    case Action::UpdateMember:
    case Action::RemoveMember:
    case Action::TransferOwner:
        emit groupUpdated(requestId, pending.group);
        break;
    case Action::RemoveMembers: {
        auto it = m_pendingBatches.find(pending.batchId);
        if (it == m_pendingBatches.end()) {
            break;
        }
        --it->remaining;
        if (it->remaining <= 0) {
            const QString representativeRequestId = it->representativeRequestId.isEmpty()
                    ? requestId
                    : it->representativeRequestId;
            const Group group = it->group;
            m_pendingBatches.erase(it);
            emit groupUpdated(representativeRequestId, group);
        }
        break;
    }
    case Action::UpdateMySettings:
        emit groupMySettingsUpdated(requestId, pending.group);
        break;
    case Action::LeaveGroup:
        emit groupLeft(requestId, pending.groupId);
        break;
    }
}

void GroupRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::FetchMembers:
        emit groupMembersFetchFailed(requestId,
                                     pending.groupId,
                                     pending.keyword,
                                     pending.offset,
                                     pending.limit,
                                     error);
        break;
    case Action::CreateGroup:
        emit groupCreateFailed(requestId, error);
        break;
    case Action::UpdateGroup:
    case Action::AddMembers:
    case Action::UpdateMember:
    case Action::RemoveMember:
    case Action::TransferOwner:
        emit groupUpdateFailed(requestId, pending.groupId, error);
        break;
    case Action::RemoveMembers: {
        const PendingBatch batch = m_pendingBatches.take(pending.batchId);
        for (const QString& pendingRequestId : batch.requestIds) {
            if (pendingRequestId != requestId) {
                m_pendingOperations.remove(pendingRequestId);
            }
        }
        emit groupUpdateFailed(requestId, pending.groupId, error);
        break;
    }
    case Action::UpdateMySettings:
        emit groupMySettingsUpdateFailed(requestId, pending.groupId, error);
        break;
    case Action::LeaveGroup:
        emit groupLeaveFailed(requestId, pending.groupId, error);
        break;
    }
}
