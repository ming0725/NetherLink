#include "GroupRemoteDataSource.h"

#include "features/chat/data/GroupRepository.h"
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
    return group.listGroupId;
}

QString normalizedListGroupName(const Group& group)
{
    return group.listGroupName;
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

QString userUuidFromObject(QJsonObject object)
{
    const QJsonObject nestedUser = object.value(QStringLiteral("user")).toObject();
    if (!nestedUser.isEmpty()) {
        object = nestedUser;
    }
    return firstString(object, {QStringLiteral("userUuid"),
                                QStringLiteral("memberUserUuid"),
                                QStringLiteral("uuid"),
                                QStringLiteral("id"),
                                QStringLiteral("userId")});
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

        const QString id = userUuidFromObject(value.toObject());
        if (!id.isEmpty()) {
            values.push_back(id);
        }
    }
    return values;
}

QJsonArray stringListToJsonArray(const QStringList& values)
{
    QJsonArray array;
    QSet<QString> seen;
    for (const QString& value : values) {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty() || seen.contains(normalized)) {
            continue;
        }
        array.append(normalized);
        seen.insert(normalized);
    }
    return array;
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
    if (normalized == QStringLiteral("online")) {
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

bool isPresenceStatusValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("online") ||
           normalized == QStringLiteral("offline") ||
           normalized == QStringLiteral("mining") ||
           normalized == QStringLiteral("busy") ||
           normalized == QStringLiteral("dnd") ||
           normalized == QStringLiteral("airplane") ||
           normalized == QStringLiteral("flying") ||
           normalized == QStringLiteral("away") ||
           normalized == QStringLiteral("invisible");
}

bool isAiKindValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("ai") ||
           normalized == QStringLiteral("bot") ||
           normalized == QStringLiteral("agent") ||
           normalized == QStringLiteral("assistant") ||
           normalized == QStringLiteral("group_bot");
}

QJsonObject presenceObjectFrom(const QJsonObject& object)
{
    const QJsonObject presence = object.value(QStringLiteral("presence")).toObject();
    if (!presence.isEmpty()) {
        return presence;
    }

    QJsonObject legacyPresence;
    if (isPresenceStatusValue(object.value(QStringLiteral("status")).toString())) {
        legacyPresence.insert(QStringLiteral("status"), object.value(QStringLiteral("status")));
    }
    if (!legacyPresence.isEmpty() && object.contains(QStringLiteral("lastSeenAt"))) {
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

GroupMemberRoleValue groupMemberRoleFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("owner")) {
        return GroupMemberRoleValue::Owner;
    }
    if (normalized == QStringLiteral("admin")) {
        return GroupMemberRoleValue::Admin;
    }
    if (normalized == QStringLiteral("ai")) {
        return GroupMemberRoleValue::Ai;
    }
    return GroupMemberRoleValue::Member;
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
    const QString modelId = userUuid.isEmpty() ? userId : userUuid;
    object.insert(QStringLiteral("id"), modelId);
    if (!userUuid.isEmpty()) {
        object.insert(QStringLiteral("userUuid"), userUuid);
    }
    if (!userId.isEmpty()) {
        object.insert(QStringLiteral("userId"), userId);
    }
    if (!object.contains(QStringLiteral("nick"))) {
        object.insert(QStringLiteral("nick"),
                      firstString(object, {QStringLiteral("nickName"),
                                           QStringLiteral("displayName"),
                                           QStringLiteral("name")}));
    }
    for (const QString& key : {QStringLiteral("presence"),
                               QStringLiteral("status"),
                               QStringLiteral("lastSeenAt"),
                               QStringLiteral("isAi"),
                               QStringLiteral("kind"),
                               QStringLiteral("agentId"),
                               QStringLiteral("agent_id"),
                               QStringLiteral("botUserId"),
                               QStringLiteral("bot_user_id"),
                               QStringLiteral("botStatus"),
                               QStringLiteral("agentStatus"),
                               QStringLiteral("aiStatus")}) {
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
    user.userUuid = firstString(object, {QStringLiteral("userUuid"),
                                         QStringLiteral("uuid"),
                                         QStringLiteral("id")});
    user.userId = firstString(object, {QStringLiteral("userId"),
                                       QStringLiteral("publicId"),
                                       QStringLiteral("public_id")});
    user.nick = object.value(QStringLiteral("nick")).toString();
    user.remark = object.value(QStringLiteral("remark")).toString();
    user.avatarPath = avatarSourceFromObject(object, QStringLiteral("avatarPath"));
    const QJsonObject presence = presenceObjectFrom(object);
    user.status = userStatusFromString(presence.value(QStringLiteral("status")).toString());
    user.lastSeenAt = dateTimeFromString(presence.value(QStringLiteral("lastSeenAt")).toString());
    user.signature = object.value(QStringLiteral("signature")).toString();
    user.region = object.value(QStringLiteral("region")).toString();
    user.aiAgentId = firstString(object, {QStringLiteral("agentId"),
                                          QStringLiteral("agent_id")});
    user.aiKind = firstString(object, {QStringLiteral("kind"),
                                       QStringLiteral("agentKind"),
                                       QStringLiteral("agent_kind")});
    user.aiStatus = firstString(object, {QStringLiteral("botStatus"),
                                         QStringLiteral("agentStatus"),
                                         QStringLiteral("aiStatus")});
    const QString rawStatus = object.value(QStringLiteral("status")).toString();
    if (user.aiStatus.isEmpty() && !isPresenceStatusValue(rawStatus)) {
        user.aiStatus = rawStatus;
    }
    user.isAi = object.value(QStringLiteral("isAi")).toBool(false) ||
                isAiKindValue(user.aiKind) ||
                !user.aiAgentId.isEmpty() ||
                !firstString(object, {QStringLiteral("botUserId"),
                                      QStringLiteral("bot_user_id")}).isEmpty() ||
                user.userId.startsWith(QStringLiteral("agent_"));
    if (user.aiAgentId.isEmpty() &&
        user.isAi &&
        user.userId.startsWith(QStringLiteral("agent_"))) {
        user.aiAgentId = user.userId;
    }
    return user;
}

GroupBotAgent groupBotAgentFromObject(QJsonObject object)
{
    const QJsonObject nestedAgent = object.value(QStringLiteral("agent")).toObject();
    if (!nestedAgent.isEmpty()) {
        object = nestedAgent;
    }

    GroupBotAgent agent;
    agent.agentId = firstString(object, {QStringLiteral("agentId"),
                                         QStringLiteral("agent_id"),
                                         QStringLiteral("id")});
    agent.botUserId = firstString(object, {QStringLiteral("botUserId"),
                                           QStringLiteral("bot_user_id"),
                                           QStringLiteral("userUuid"),
                                           QStringLiteral("user_uuid")});
    agent.ownerUserId = firstString(object, {QStringLiteral("ownerUserId"),
                                             QStringLiteral("owner_user_id"),
                                             QStringLiteral("ownerUuid")});
    agent.kind = firstString(object, {QStringLiteral("kind"),
                                      QStringLiteral("agentKind")});
    agent.visibility = object.value(QStringLiteral("visibility")).toString();
    agent.name = firstString(object, {QStringLiteral("name"),
                                      QStringLiteral("nick"),
                                      QStringLiteral("displayName")});
    agent.avatarFileId = avatarFileIdFrom(object);
    agent.basePrompt = object.value(QStringLiteral("basePrompt")).toString(
            object.value(QStringLiteral("base_prompt")).toString());
    agent.model = object.value(QStringLiteral("model")).toString();
    agent.status = object.value(QStringLiteral("status")).toString();
    agent.version = object.value(QStringLiteral("version")).toInt();
    agent.createdAt = dateTimeFromString(object.value(QStringLiteral("createdAt")).toString());
    agent.updatedAt = dateTimeFromString(object.value(QStringLiteral("updatedAt")).toString());
    return agent;
}

GroupMemberProfile groupMemberFromObject(const QString& groupId, const QJsonObject& source)
{
    GroupMemberProfile member;
    member.groupId = firstString(source, {QStringLiteral("groupId"),
                                          QStringLiteral("groupID")});
    if (member.groupId.isEmpty()) {
        member.groupId = groupId;
    }
    member.userUuid = userUuidFromObject(source);
    member.user = userFromMemberObject(source);
    if (member.userUuid.isEmpty()) {
        member.userUuid = member.user.id;
    }
    if (member.user.id.isEmpty()) {
        member.user.id = member.userUuid;
        member.user.userUuid = member.userUuid;
    }
    member.nickname = source.value(QStringLiteral("nickname")).toString();
    member.role = groupMemberRoleFromString(source.value(QStringLiteral("role")).toString());
    if (member.role == GroupMemberRoleValue::Ai) {
        member.user.isAi = true;
        if (member.user.aiKind.isEmpty()) {
            member.user.aiKind = QStringLiteral("group_bot");
        }
    }
    member.isDnd = source.value(QStringLiteral("isDnd")).toBool(false);
    member.joinedAt = dateTimeFromString(source.value(QStringLiteral("joinedAt")).toString());
    member.version = source.value(QStringLiteral("version")).toInt();
    return member;
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
    group.groupPublicId = firstString(object, {QStringLiteral("groupPublicId"),
                                               QStringLiteral("publicId"),
                                               QStringLiteral("public_id")});
    group.version = object.value(QStringLiteral("version")).toInt();
    group.etag = object.value(QStringLiteral("etag")).toString();
    group.groupName = firstString(object, {QStringLiteral("groupName"), QStringLiteral("name")});
    group.memberNum = object.value(QStringLiteral("memberNum")).toInt(
            object.value(QStringLiteral("memberCount")).toInt());
    group.ownerId = firstString(object, {QStringLiteral("ownerUuid"), QStringLiteral("ownerId")});
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
    group.currentUserNickname = object.value(QStringLiteral("currentUserNickname")).toString(
            object.value(QStringLiteral("nickname")).toString());
    group.memberNicknames = stringMapFromJson(object.value(QStringLiteral("memberNicknames")).toObject());
    group.membersID = stringVectorFromJson(object.value(QStringLiteral("membersID")).toArray());
    if (group.membersID.isEmpty()) {
        group.membersID = stringVectorFromJson(object.value(QStringLiteral("members")).toArray());
    }
    group.listGroupId = object.value(QStringLiteral("listGroupId")).toString();
    group.listGroupName = object.value(QStringLiteral("listGroupName")).toString();
    group.role = object.value(QStringLiteral("role")).toString();
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
    if (group.version > 0) {
        body.insert(QStringLiteral("expectedVersion"), group.version);
    }

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
    if (group.version > 0) {
        body.insert(QStringLiteral("expectedVersion"), group.version);
    }

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

QString GroupRemoteDataSource::createBot(const GroupBotCreateRequest& request)
{
    const QString groupId = request.groupId.trimmed();
    const QString name = request.name.trimmed();
    const QString basePrompt = request.basePrompt.trimmed();
    if (groupId.isEmpty() || name.isEmpty() || basePrompt.isEmpty()) {
        return {};
    }

    const QString clientOperationId = request.clientOperationId.trimmed().isEmpty()
            ? newClientOperationId(QStringLiteral("op_group_bot_create"))
            : request.clientOperationId.trimmed();

    QJsonObject body;
    body.insert(QStringLiteral("name"), name);
    if (!request.avatarFileId.trimmed().isEmpty()) {
        body.insert(QStringLiteral("avatarFileId"), request.avatarFileId.trimmed());
    }
    body.insert(QStringLiteral("basePrompt"), basePrompt);
    if (!request.model.trimmed().isEmpty()) {
        body.insert(QStringLiteral("model"), request.model.trimmed());
    }
    body.insert(QStringLiteral("toolPolicy"), QJsonObject{
            {QStringLiteral("allowedTools"), stringListToJsonArray(request.allowedTools)},
            {QStringLiteral("allowedDomains"), stringListToJsonArray(request.allowedDomains)}
    });
    body.insert(QStringLiteral("safetyPolicy"), QJsonObject{
            {QStringLiteral("requireConfirmationForSideEffects"),
             request.requireConfirmationForSideEffects}
    });
    body.insert(QStringLiteral("cooldownSeconds"), qMax(0, request.cooldownSeconds));
    body.insert(QStringLiteral("clientOperationId"), clientOperationId);

    PendingOperation pending;
    pending.action = Action::CreateBot;
    pending.groupId = groupId;
    return sendOperation(Action::CreateBot,
                         QStringLiteral("/groups/%1/bots").arg(groupId),
                         body,
                         pending,
                         HttpMethod::Post);
}

QString GroupRemoteDataSource::fetchGroup(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::FetchGroup;
    pending.groupId = groupId;
    return sendOperation(Action::FetchGroup,
                         QStringLiteral("/groups/%1").arg(groupId),
                         {},
                         pending,
                         HttpMethod::Get);
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
    case Action::FetchGroup: {
        Group group = groupFromResponseObject(response.object());
        if (group.groupId.isEmpty()) {
            group.groupId = pending.groupId;
        }
        emit groupFetched(requestId, group);
        break;
    }
    case Action::FetchMembers: {
        const QJsonObject root = response.object();
        const QJsonArray items = arrayFromResponse(root, QStringLiteral("members"));
        QVector<GroupMemberProfile> members;
        members.reserve(items.size());
        for (const QJsonValue& value : items) {
            const QJsonObject object = value.toObject();
            cacheMemberUserObject(object);
            const GroupMemberProfile member = groupMemberFromObject(pending.groupId, object);
            if (!member.userUuid.isEmpty()) {
                members.push_back(member);
            }
        }
        const int total = root.value(QStringLiteral("total")).toInt(
                root.value(QStringLiteral("totalCount")).toInt(pending.offset + members.size()));
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
    {
        Group group = groupFromResponseObject(response.object());
        if (group.groupId.isEmpty()) {
            group = pending.group;
        }
        emit groupUpdated(requestId, group);
        break;
    }
    case Action::AddMembers:
    case Action::UpdateMember:
    case Action::RemoveMember:
    case Action::TransferOwner:
    {
        if (pending.action == Action::UpdateMember) {
            QJsonObject member = response.object().value(QStringLiteral("member")).toObject();
            if (member.isEmpty()) {
                member = response.object();
            }
            if (!member.isEmpty() && !pending.groupId.isEmpty()) {
                if (!member.contains(QStringLiteral("groupId"))) {
                    member.insert(QStringLiteral("groupId"), pending.groupId);
                }
                GroupRepository::instance().upsertGroupMember(member);
            }
        }

        Group group = GroupRepository::instance().requestGroupDetail({pending.groupId});
        if (group.groupId.isEmpty()) {
            group = pending.group;
        }
        emit groupUpdated(requestId, group);
        break;
    }
    case Action::CreateBot:
    {
        GroupBotAgent agent = groupBotAgentFromObject(response.object());
        if (agent.agentId.isEmpty() || agent.botUserId.isEmpty()) {
            NetworkError error;
            error.code = QStringLiteral("GROUP_BOT_MISSING");
            error.message = QStringLiteral("Create group bot response did not include agentId or botUserId.");
            emit groupBotCreateFailed(requestId, pending.groupId, error);
            return;
        }
        emit groupBotCreated(requestId, pending.groupId, agent);
        break;
    }
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
    {
        Group group = groupFromResponseObject(response.object());
        if (group.groupId.isEmpty()) {
            group = pending.group;
        }
        emit groupMySettingsUpdated(requestId, group);
        break;
    }
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
    case Action::FetchGroup:
        emit groupFetchFailed(requestId, pending.groupId, error);
        break;
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
    case Action::CreateBot:
        emit groupBotCreateFailed(requestId, pending.groupId, error);
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
