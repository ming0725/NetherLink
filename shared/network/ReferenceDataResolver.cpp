#include "ReferenceDataResolver.h"

#include "HttpClient.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"

#include <QJsonArray>
#include <QStringList>

namespace {

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

int firstInt(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.contains(key)) {
            return object.value(key).toInt();
        }
    }
    return 0;
}

QJsonArray arrayValue(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.value(key).isArray()) {
            return object.value(key).toArray();
        }
    }
    return {};
}

QString groupMemberKey(const QString& groupId, const QString& userUuid)
{
    return groupId + QLatin1Char(':') + userUuid;
}

QJsonObject nestedObject(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QJsonObject nested = object.value(key).toObject();
        if (!nested.isEmpty()) {
            return nested;
        }
    }
    return {};
}

QString userUuidFromValue(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    QJsonObject object = value.toObject();
    const QJsonObject nested = nestedObject(object, {QStringLiteral("user"),
                                                     QStringLiteral("profile"),
                                                     QStringLiteral("member")});
    if (!nested.isEmpty()) {
        object = nested;
    }
    return firstString(object, {QStringLiteral("userUuid"),
                                QStringLiteral("id"),
                                QStringLiteral("userId"),
                                QStringLiteral("memberUserUuid")});
}

} // namespace

ReferenceDataResolver& ReferenceDataResolver::instance()
{
    static ReferenceDataResolver resolver;
    return resolver;
}

ReferenceDataResolver::ReferenceDataResolver(QObject* parent)
    : QObject(parent)
{
    m_userRefreshTimer.setSingleShot(true);
    m_userRefreshTimer.setInterval(80);
    connect(&m_userRefreshTimer, &QTimer::timeout, this, &ReferenceDataResolver::flushUserRefreshQueue);

    m_groupMemberRefreshTimer.setSingleShot(true);
    m_groupMemberRefreshTimer.setInterval(80);
    connect(&m_groupMemberRefreshTimer, &QTimer::timeout, this, &ReferenceDataResolver::flushGroupMemberRefreshQueue);

    connect(&HttpClient::instance(), &HttpClient::requestSucceeded, this, &ReferenceDataResolver::handleRequestSucceeded);
    connect(&HttpClient::instance(), &HttpClient::requestFailed, this, &ReferenceDataResolver::handleRequestFailed);
}

void ReferenceDataResolver::consumePayload(const QJsonObject& payload)
{
    consumeIncluded(payload.value(QStringLiteral("included")).toObject());
    consumeRefs(payload.value(QStringLiteral("refs")).toObject());
}

void ReferenceDataResolver::consumeIncluded(const QJsonObject& included)
{
    if (included.isEmpty()) {
        return;
    }

    for (const QJsonValue& value : arrayValue(included, {QStringLiteral("users")})) {
        upsertUserObject(value.toObject());
    }
    for (const QJsonValue& value : arrayValue(included, {QStringLiteral("groups")})) {
        GroupRepository::instance().upsertGroup(value.toObject());
    }
    for (const QJsonValue& value : arrayValue(included, {QStringLiteral("groupMembers"),
                                                         QStringLiteral("members")})) {
        upsertGroupMemberObject(value.toObject());
    }
}

void ReferenceDataResolver::consumeRefs(const QJsonObject& refs)
{
    if (refs.isEmpty()) {
        return;
    }

    for (const QJsonValue& value : arrayValue(refs, {QStringLiteral("users")})) {
        const QJsonObject ref = value.toObject();
        upsertUserObject(ref);
        const QString userUuid = firstString(ref, {QStringLiteral("userUuid"),
                                                   QStringLiteral("id"),
                                                   QStringLiteral("userId")});
        if (userUuid.isEmpty()) {
            continue;
        }
        const int remoteVersion = firstInt(ref, {QStringLiteral("version")});
        if (UserRepository::instance().needsUserRefresh(userUuid, remoteVersion)) {
            scheduleUserRefresh(userUuid, UserRepository::instance().requestUserVersion(userUuid));
        }
    }

    for (const QJsonValue& value : arrayValue(refs, {QStringLiteral("groupMembers"),
                                                     QStringLiteral("members")})) {
        const QJsonObject ref = value.toObject();
        const QString groupId = firstString(ref, {QStringLiteral("groupId"),
                                                  QStringLiteral("groupID")});
        const QString userUuid = firstString(ref, {QStringLiteral("userUuid"),
                                                   QStringLiteral("userId"),
                                                   QStringLiteral("memberUserUuid")});
        if (groupId.isEmpty() || userUuid.isEmpty()) {
            continue;
        }
        if (ref.contains(QStringLiteral("role")) ||
            ref.contains(QStringLiteral("nickname")) ||
            ref.contains(QStringLiteral("user"))) {
            upsertGroupMemberObject(ref);
        }
        const int remoteVersion = firstInt(ref, {QStringLiteral("version")});
        if (GroupRepository::instance().needsGroupMemberRefresh(groupId, userUuid, remoteVersion)) {
            scheduleGroupMemberRefresh(groupId,
                                       userUuid,
                                       GroupRepository::instance().requestGroupMemberVersion(groupId, userUuid));
        }
    }
}

void ReferenceDataResolver::upsertUserObject(const QJsonObject& object)
{
    QJsonObject user = nestedObject(object, {QStringLiteral("user"),
                                             QStringLiteral("profile")});
    if (user.isEmpty()) {
        user = object;
    }
    UserRepository::instance().upsertUserProfile(user);
}

void ReferenceDataResolver::upsertGroupMemberObject(const QJsonObject& object)
{
    GroupRepository::instance().upsertGroupMember(object);
}

void ReferenceDataResolver::scheduleUserRefresh(const QString& userUuid, int knownVersion)
{
    if (userUuid.isEmpty() || m_inFlightUserIds.contains(userUuid)) {
        return;
    }
    m_pendingUsers.insert(userUuid, UserRefreshItem{userUuid, knownVersion});
    m_userRefreshTimer.start();
}

void ReferenceDataResolver::scheduleGroupMemberRefresh(const QString& groupId,
                                                       const QString& userUuid,
                                                       int knownVersion)
{
    const QString key = groupMemberKey(groupId, userUuid);
    if (key.isEmpty() || m_inFlightGroupMemberKeys.contains(key)) {
        return;
    }
    m_pendingGroupMembers.insert(key, GroupMemberRefreshItem{groupId, userUuid, knownVersion});
    m_groupMemberRefreshTimer.start();
}

void ReferenceDataResolver::flushUserRefreshQueue()
{
    if (m_pendingUsers.isEmpty()) {
        return;
    }

    QJsonArray items;
    const auto pending = m_pendingUsers;
    m_pendingUsers.clear();
    for (const UserRefreshItem& item : pending) {
        if (m_inFlightUserIds.contains(item.userUuid)) {
            continue;
        }
        m_inFlightUserIds.insert(item.userUuid);
        items.append(QJsonObject{
                {QStringLiteral("userUuid"), item.userUuid},
                {QStringLiteral("knownVersion"), item.knownVersion}
        });
    }
    if (items.isEmpty()) {
        return;
    }

    const QString requestId = HttpClient::instance().send(NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/users/batch"),
            {{QStringLiteral("items"), items}}));
    if (!requestId.isEmpty()) {
        m_userBatchRequests.insert(requestId);
    }
}

void ReferenceDataResolver::flushGroupMemberRefreshQueue()
{
    if (m_pendingGroupMembers.isEmpty()) {
        return;
    }

    QHash<QString, QJsonArray> groupedItems;
    const auto pending = m_pendingGroupMembers;
    m_pendingGroupMembers.clear();
    for (const GroupMemberRefreshItem& item : pending) {
        const QString key = groupMemberKey(item.groupId, item.userUuid);
        if (m_inFlightGroupMemberKeys.contains(key)) {
            continue;
        }
        m_inFlightGroupMemberKeys.insert(key);
        groupedItems[item.groupId].append(QJsonObject{
                {QStringLiteral("userUuid"), item.userUuid},
                {QStringLiteral("knownVersion"), item.knownVersion}
        });
    }

    for (auto it = groupedItems.cbegin(); it != groupedItems.cend(); ++it) {
        const QString requestId = HttpClient::instance().send(NetworkRequest::json(
                HttpMethod::Post,
                QStringLiteral("/groups/%1/members/batch").arg(it.key()),
                {{QStringLiteral("items"), it.value()}}));
        if (!requestId.isEmpty()) {
            m_groupMemberBatchRequests.insert(requestId, it.key());
        }
    }
}

void ReferenceDataResolver::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (m_userBatchRequests.remove(requestId)) {
        const QJsonObject root = response.object();
        for (const QJsonValue& value : arrayValue(root, {QStringLiteral("users")})) {
            upsertUserObject(value.toObject());
        }
        for (const QJsonValue& value : arrayValue(root, {QStringLiteral("notModified"),
                                                         QStringLiteral("notFound")})) {
            const QString userUuid = userUuidFromValue(value);
            m_inFlightUserIds.remove(userUuid);
        }
        for (const QJsonValue& value : arrayValue(root, {QStringLiteral("users")})) {
            const QString userUuid = userUuidFromValue(value);
            m_inFlightUserIds.remove(userUuid);
        }
        return;
    }

    if (m_groupMemberBatchRequests.contains(requestId)) {
        const QString groupId = m_groupMemberBatchRequests.take(requestId);
        const QJsonObject root = response.object();
        for (const QJsonValue& value : arrayValue(root, {QStringLiteral("members"),
                                                         QStringLiteral("groupMembers")})) {
            QJsonObject member = value.toObject();
            if (!member.contains(QStringLiteral("groupId"))) {
                member.insert(QStringLiteral("groupId"), groupId);
            }
            upsertGroupMemberObject(member);
            const QString userUuid = userUuidFromValue(member);
            m_inFlightGroupMemberKeys.remove(groupMemberKey(groupId, userUuid));
        }
        for (const QString& key : {QStringLiteral("notModified"), QStringLiteral("notFound")}) {
            for (const QJsonValue& value : arrayValue(root, {key})) {
                const QString userUuid = userUuidFromValue(value);
                m_inFlightGroupMemberKeys.remove(groupMemberKey(groupId, userUuid));
            }
        }
    }
}

void ReferenceDataResolver::handleRequestFailed(const QString& requestId, const NetworkError&)
{
    if (m_userBatchRequests.remove(requestId)) {
        m_inFlightUserIds.clear();
        return;
    }

    if (m_groupMemberBatchRequests.contains(requestId)) {
        const QString groupId = m_groupMemberBatchRequests.take(requestId);
        const QString prefix = groupId + QLatin1Char(':');
        const QSet<QString> inFlightKeys = m_inFlightGroupMemberKeys;
        for (const QString& key : inFlightKeys) {
            if (key.startsWith(prefix)) {
                m_inFlightGroupMemberKeys.remove(key);
            }
        }
    }
}
