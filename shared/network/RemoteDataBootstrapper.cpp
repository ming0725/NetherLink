#include "RemoteDataBootstrapper.h"

#include "AppEventBus.h"
#include "HttpClient.h"
#include "ReferenceDataResolver.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/services/AvatarSource.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace {

constexpr int kPageLimit = 100;
const QString kFriendRequestUnreadScope = QStringLiteral("friend_requests");
const QString kGroupNotificationUnreadScope = QStringLiteral("group_notifications");

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

int firstInt(const QJsonObject& object, const QStringList& keys, int fallback = 0)
{
    for (const QString& key : keys) {
        if (object.contains(key)) {
            return object.value(key).toInt(fallback);
        }
    }
    return fallback;
}

QString avatarSourceFrom(const QJsonObject& object, const QStringList& sourceKeys)
{
    QString source = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (source.isEmpty()) {
        source = firstString(object, sourceKeys);
    }
    return AvatarSource::versioned(source,
                                   firstInt(object, {QStringLiteral("avatarVersion")}),
                                   firstString(object, {QStringLiteral("avatarEtag")}),
                                   firstString(object, {QStringLiteral("avatarContentHash")}));
}

QString cacheKeyFor(const QJsonObject& object, const QStringList& keyFields, int index)
{
    const QString key = firstString(object, keyFields);
    return key.isEmpty() ? QString::number(index) : key;
}

QJsonArray arrayFromResponse(const QJsonObject& root, const QString& arrayKey)
{
    if (root.value(arrayKey).isArray()) {
        return root.value(arrayKey).toArray();
    }
    if (root.value(QStringLiteral("items")).isArray()) {
        return root.value(QStringLiteral("items")).toArray();
    }
    if (root.value(QStringLiteral("data")).isArray()) {
        return root.value(QStringLiteral("data")).toArray();
    }
    if (root.value(QStringLiteral("results")).isArray()) {
        return root.value(QStringLiteral("results")).toArray();
    }
    return {};
}

QJsonObject mergedUserObject(QJsonObject object)
{
    const QJsonObject friendObject = object.value(QStringLiteral("friend")).toObject();
    if (!friendObject.isEmpty()) {
        object = friendObject;
    }

    const QJsonObject user = object.value(QStringLiteral("user")).toObject();
    if (!user.isEmpty()) {
        QJsonObject relation = object;
        relation.remove(QStringLiteral("user"));
        const QJsonObject nestedRelation = object.value(QStringLiteral("relation")).toObject();
        relation.remove(QStringLiteral("relation"));
        for (auto it = nestedRelation.constBegin(); it != nestedRelation.constEnd(); ++it) {
            relation.insert(it.key(), it.value());
        }

        object = user;
        for (auto it = relation.constBegin(); it != relation.constEnd(); ++it) {
            const bool userHasPrivateId = object.contains(QStringLiteral("userUuid")) ||
                                          object.contains(QStringLiteral("uuid")) ||
                                          object.contains(QStringLiteral("id"));
            const bool userHasPublicId = object.contains(QStringLiteral("userId")) ||
                                         object.contains(QStringLiteral("publicId")) ||
                                         object.contains(QStringLiteral("public_id"));
            if ((it.key() == QStringLiteral("id") ||
                 it.key() == QStringLiteral("userUuid") ||
                 it.key() == QStringLiteral("uuid")) && userHasPrivateId) {
                continue;
            }
            if ((it.key() == QStringLiteral("userId") ||
                 it.key() == QStringLiteral("publicId") ||
                 it.key() == QStringLiteral("public_id")) && userHasPublicId) {
                continue;
            }
            object.insert(it.key(), it.value());
        }
    }

    const QString id = firstString(object, {QStringLiteral("userUuid"),
                                            QStringLiteral("friendUserUuid"),
                                            QStringLiteral("uuid"),
                                            QStringLiteral("id"),
                                            QStringLiteral("userId")});
    const QString publicId = firstString(object, {QStringLiteral("userId"),
                                                  QStringLiteral("publicId"),
                                                  QStringLiteral("public_id")});
    const QString nick = firstString(object, {QStringLiteral("nick"),
                                              QStringLiteral("nickName"),
                                              QStringLiteral("displayName"),
                                              QStringLiteral("name")});
    if (!id.isEmpty()) {
        object.insert(QStringLiteral("id"), id);
    }
    if (!publicId.isEmpty()) {
        object.insert(QStringLiteral("userId"), publicId);
    }
    if (!nick.isEmpty()) {
        object.insert(QStringLiteral("nick"), nick);
    }
    if (!object.contains(QStringLiteral("avatarPath"))) {
        object.insert(QStringLiteral("avatarPath"),
                      avatarSourceFrom(object, {QStringLiteral("avatarUrl"), QStringLiteral("avatarPath")}));
    } else {
        object.insert(QStringLiteral("avatarPath"),
                      avatarSourceFrom(object, {QStringLiteral("avatarPath"), QStringLiteral("avatarUrl")}));
    }
    if (!object.contains(QStringLiteral("isFriend"))) {
        object.insert(QStringLiteral("isFriend"), true);
    }
    if (!object.contains(QStringLiteral("friendGroupId"))) {
        object.insert(QStringLiteral("friendGroupId"), QStringLiteral("default"));
    }
    if (!object.contains(QStringLiteral("friendGroupName"))) {
        object.insert(QStringLiteral("friendGroupName"), QStringLiteral("默认分组"));
    }
    return object;
}

QJsonObject profileObject(QJsonObject object)
{
    if (object.value(QStringLiteral("user")).isObject()) {
        object = object.value(QStringLiteral("user")).toObject();
    }
    const QString userUuid = firstString(object, {QStringLiteral("userUuid"),
                                                  QStringLiteral("uuid"),
                                                  QStringLiteral("id")});
    QString userId = firstString(object, {QStringLiteral("userId"),
                                          QStringLiteral("publicId"),
                                          QStringLiteral("public_id")});
    if (userId.isEmpty()) {
        userId = userUuid;
    }
    const QString nickName = firstString(object, {QStringLiteral("nickName"),
                                                  QStringLiteral("nick"),
                                                  QStringLiteral("displayName"),
                                                  QStringLiteral("name")});
    const QJsonObject presence = object.value(QStringLiteral("presence")).toObject();
    QString presenceStatus = presence.value(QStringLiteral("status")).toString();
    if (presenceStatus.isEmpty()) {
        const QString legacyStatus = object.value(QStringLiteral("status")).toString();
        const QString normalizedStatus = legacyStatus.trimmed().toLower();
        if (normalizedStatus == QStringLiteral("online") ||
            normalizedStatus == QStringLiteral("offline") ||
            normalizedStatus == QStringLiteral("mining") ||
            normalizedStatus == QStringLiteral("busy") ||
            normalizedStatus == QStringLiteral("dnd") ||
            normalizedStatus == QStringLiteral("airplane") ||
            normalizedStatus == QStringLiteral("flying") ||
            normalizedStatus == QStringLiteral("away") ||
            normalizedStatus == QStringLiteral("invisible")) {
            presenceStatus = legacyStatus;
        }
    }
    return {
            {QStringLiteral("userUuid"), userUuid},
            {QStringLiteral("userId"), userId},
            {QStringLiteral("nickName"), nickName.isEmpty() ? userId : nickName},
            {QStringLiteral("avatarPath"), avatarSourceFrom(object, {QStringLiteral("avatarPath"),
                                                                     QStringLiteral("avatarUrl")})},
            {QStringLiteral("avatarVersion"), firstInt(object, {QStringLiteral("avatarVersion")})},
            {QStringLiteral("avatarEtag"), firstString(object, {QStringLiteral("avatarEtag")})},
            {QStringLiteral("avatarContentHash"), firstString(object, {QStringLiteral("avatarContentHash")})},
            {QStringLiteral("status"), presenceStatus.isEmpty() ? QStringLiteral("online") : presenceStatus},
            {QStringLiteral("signature"), object.value(QStringLiteral("signature")).toString()},
            {QStringLiteral("region"), object.value(QStringLiteral("region")).toString()},
            {QStringLiteral("version"), object.value(QStringLiteral("version")).toInt()},
            {QStringLiteral("etag"), object.value(QStringLiteral("etag")).toString()}
    };
}

QJsonObject preferencesObject(QJsonObject object)
{
    object.insert(QStringLiteral("id"), QStringLiteral("current"));
    return object;
}

QJsonObject groupObject(QJsonObject object)
{
    if (object.value(QStringLiteral("group")).isObject()) {
        object = object.value(QStringLiteral("group")).toObject();
    }
    const QString groupId = firstString(object, {QStringLiteral("groupId"), QStringLiteral("id")});
    const QString groupName = firstString(object, {QStringLiteral("groupName"),
                                                   QStringLiteral("name"),
                                                   QStringLiteral("title")});
    const QString groupPublicId = firstString(object, {QStringLiteral("groupPublicId"),
                                                       QStringLiteral("publicId"),
                                                       QStringLiteral("public_id")});
    if (!groupId.isEmpty()) {
        object.insert(QStringLiteral("groupId"), groupId);
    }
    if (!groupPublicId.isEmpty()) {
        object.insert(QStringLiteral("groupPublicId"), groupPublicId);
    }
    if (!groupName.isEmpty()) {
        object.insert(QStringLiteral("groupName"), groupName);
    }
    if (!object.contains(QStringLiteral("memberNum"))) {
        object.insert(QStringLiteral("memberNum"),
                      firstInt(object, {QStringLiteral("memberCount"), QStringLiteral("membersCount")}));
    }
    if (!object.contains(QStringLiteral("groupAvatarPath"))) {
        object.insert(QStringLiteral("groupAvatarPath"),
                      avatarSourceFrom(object, {QStringLiteral("groupAvatarPath"),
                                                QStringLiteral("avatarUrl")}));
    } else {
        object.insert(QStringLiteral("groupAvatarPath"),
                      avatarSourceFrom(object, {QStringLiteral("groupAvatarPath"),
                                                QStringLiteral("avatarUrl")}));
    }
    return object;
}

QJsonObject friendGroupObject(QJsonObject object)
{
    if (object.value(QStringLiteral("group")).isObject()) {
        object = object.value(QStringLiteral("group")).toObject();
    }
    const QString friendGroupId = firstString(object, {QStringLiteral("friendGroupId"),
                                                       QStringLiteral("groupId"),
                                                       QStringLiteral("id")});
    const QString name = firstString(object, {QStringLiteral("name"),
                                              QStringLiteral("groupName")});
    if (!friendGroupId.isEmpty()) {
        object.insert(QStringLiteral("friendGroupId"), friendGroupId);
        object.insert(QStringLiteral("groupId"), friendGroupId);
    }
    if (!name.isEmpty()) {
        object.insert(QStringLiteral("name"), name);
    }
    return object;
}

QJsonObject notificationObject(QJsonObject object)
{
    const QJsonObject payload = object.value(QStringLiteral("payload")).toObject();
    if (!payload.isEmpty()) {
        for (auto it = payload.constBegin(); it != payload.constEnd(); ++it) {
            if (!object.contains(it.key())) {
                object.insert(it.key(), it.value());
            }
        }
    }
    if (!object.contains(QStringLiteral("id"))) {
        object.insert(QStringLiteral("id"),
                      firstString(object, {QStringLiteral("notificationId"),
                                           QStringLiteral("requestId"),
                                           QStringLiteral("id"),
                                           QStringLiteral("sourceId")}));
    }
    if (!object.contains(QStringLiteral("createdAt"))) {
        object.insert(QStringLiteral("createdAt"), object.value(QStringLiteral("updatedAt")).toString());
    }
    if (!object.contains(QStringLiteral("requestDate"))) {
        object.insert(QStringLiteral("requestDate"), object.value(QStringLiteral("createdAt")).toString());
    }
    if (!object.contains(QStringLiteral("unread"))) {
        object.insert(QStringLiteral("unread"), object.value(QStringLiteral("readAt")).isNull());
    }
    return object;
}

QString notificationUnreadScope(const QString& type)
{
    if (type == QStringLiteral("friend.request.created")) {
        return kFriendRequestUnreadScope;
    }
    if (type == QStringLiteral("group.notification.created") ||
        type == QStringLiteral("group.join_request.created")) {
        return kGroupNotificationUnreadScope;
    }
    return {};
}

QString notificationUnreadItemId(const QJsonObject& object)
{
    const QString direct = firstString(object, {QStringLiteral("sourceId"),
                                                QStringLiteral("requestId"),
                                                QStringLiteral("notificationId"),
                                                QStringLiteral("id")});
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonObject payload = object.value(QStringLiteral("payload")).toObject();
    const QString payloadId = firstString(payload, {QStringLiteral("sourceId"),
                                                    QStringLiteral("requestId"),
                                                    QStringLiteral("notificationId"),
                                                    QStringLiteral("id")});
    if (!payloadId.isEmpty()) {
        return payloadId;
    }

    for (const QString& key : {QStringLiteral("request"), QStringLiteral("notification")}) {
        const QJsonObject nested = payload.value(key).toObject();
        const QString nestedId = firstString(nested, {QStringLiteral("sourceId"),
                                                      QStringLiteral("requestId"),
                                                      QStringLiteral("notificationId"),
                                                      QStringLiteral("id")});
        if (!nestedId.isEmpty()) {
            return nestedId;
        }
    }
    return {};
}

bool notificationUnreadValue(const QJsonObject& object)
{
    if (object.contains(QStringLiteral("unread"))) {
        return object.value(QStringLiteral("unread")).toBool();
    }
    if (object.contains(QStringLiteral("readAt"))) {
        const QJsonValue readAt = object.value(QStringLiteral("readAt"));
        return readAt.isNull() || readAt.toString().isEmpty();
    }
    return false;
}

void updateUnreadStateFromNotification(const QJsonObject& object)
{
    const QString scope = notificationUnreadScope(object.value(QStringLiteral("type")).toString());
    const QString itemId = notificationUnreadItemId(object);
    if (scope.isEmpty() || itemId.isEmpty()) {
        return;
    }

    UnreadStateRepository::instance().setUnread(scope, itemId, notificationUnreadValue(object));
}

QJsonObject aiConversationObject(QJsonObject object)
{
    const QString conversationId = firstString(object, {QStringLiteral("conversationId"), QStringLiteral("id")});
    if (!conversationId.isEmpty()) {
        object.insert(QStringLiteral("conversationId"), conversationId);
    }
    if (!object.contains(QStringLiteral("title"))) {
        object.insert(QStringLiteral("title"), object.value(QStringLiteral("name")).toString());
    }
    if (!object.contains(QStringLiteral("time"))) {
        object.insert(QStringLiteral("time"),
                      firstString(object, {QStringLiteral("updatedAt"),
                                           QStringLiteral("lastMessageAt"),
                                           QStringLiteral("createdAt")}));
    }
    return object;
}

QJsonObject normalizeForDomain(const QString& domain, const QJsonObject& object)
{
    if (domain == QStringLiteral("users")) {
        return mergedUserObject(object);
    }
    if (domain == QStringLiteral("current_profiles")) {
        return profileObject(object);
    }
    if (domain == QStringLiteral("current_preferences")) {
        return preferencesObject(object);
    }
    if (domain == QStringLiteral("groups")) {
        return groupObject(object);
    }
    if (domain == QStringLiteral("friend_groups")) {
        return friendGroupObject(object);
    }
    if (domain == QStringLiteral("friend_notifications") ||
        domain == QStringLiteral("group_notifications") ||
        domain == QStringLiteral("notifications")) {
        return notificationObject(object);
    }
    if (domain == QStringLiteral("ai_chat_entries")) {
        return aiConversationObject(object);
    }
    return object;
}

QVector<RemoteDataBootstrapper::FetchSpec> defaultFetchSpecs()
{
    return {
            {QStringLiteral("current_profiles"), QStringLiteral("/me"), {}, {QStringLiteral("userUuid"), QStringLiteral("id"), QStringLiteral("userId")}, {}, true, false},
            {QStringLiteral("current_preferences"), QStringLiteral("/me/preferences"), {}, {QStringLiteral("id")}, {}, true, false},
            {QStringLiteral("friend_groups"), QStringLiteral("/friend-groups"), QStringLiteral("groups"), {QStringLiteral("friendGroupId"), QStringLiteral("groupId"), QStringLiteral("id")}, {}, true, false},
            {QStringLiteral("users"), QStringLiteral("/friends"), QStringLiteral("friends"), {QStringLiteral("userUuid"), QStringLiteral("friendUserUuid"), QStringLiteral("uuid"), QStringLiteral("id"), QStringLiteral("userId")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("groups"), QStringLiteral("/groups"), QStringLiteral("groups"), {QStringLiteral("groupId"), QStringLiteral("id")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("friend_notifications"), QStringLiteral("/friend-requests"), QStringLiteral("requests"), {QStringLiteral("id"), QStringLiteral("requestId"), QStringLiteral("notificationId")}, {{QStringLiteral("status"), QStringLiteral("pending")}, {QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("group_notifications"), QStringLiteral("/group-notifications"), QStringLiteral("notifications"), {QStringLiteral("id"), QStringLiteral("requestId"), QStringLiteral("notificationId")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("notifications"), QStringLiteral("/notifications"), QStringLiteral("notifications"), {QStringLiteral("notificationId"), QStringLiteral("id")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("posts"), QStringLiteral("/posts"), QStringLiteral("posts"), {QStringLiteral("postId"), QStringLiteral("postID"), QStringLiteral("id")}, {{QStringLiteral("followOnly"), false}, {QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("ai_chat_entries"), QStringLiteral("/ai/conversations"), QStringLiteral("conversations"), {QStringLiteral("conversationId"), QStringLiteral("id")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true},
            {QStringLiteral("conversations"), QStringLiteral("/conversations"), QStringLiteral("conversations"), {QStringLiteral("conversationId"), QStringLiteral("id")}, {{QStringLiteral("limit"), kPageLimit}, {QStringLiteral("offset"), 0}}, true, true}
    };
}

} // namespace

RemoteDataBootstrapper& RemoteDataBootstrapper::instance()
{
    static RemoteDataBootstrapper bootstrapper;
    return bootstrapper;
}

RemoteDataBootstrapper::RemoteDataBootstrapper(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(), &HttpClient::requestSucceeded, this, &RemoteDataBootstrapper::handleSuccess);
    connect(&HttpClient::instance(), &HttpClient::requestFailed, this, &RemoteDataBootstrapper::handleFailure);
    connect(&AppEventBus::instance(), &AppEventBus::fullSyncRequired, this, [this](const RealtimeEvent&) {
        syncAll();
    });
}

bool RemoteDataBootstrapper::isRunning() const
{
    return m_running;
}

void RemoteDataBootstrapper::syncAll()
{
    if (m_running) {
        return;
    }

    m_running = true;
    m_failed = false;
    emit syncStarted();

    const QVector<FetchSpec> specs = defaultFetchSpecs();
    for (const FetchSpec& spec : specs) {
        enqueue(spec);
    }

    if (m_requests.isEmpty()) {
        m_running = false;
        emit syncFinished(!m_failed);
    }
}

void RemoteDataBootstrapper::enqueue(const FetchSpec& spec)
{
    NetworkRequest request;
    request.method = HttpMethod::Get;
    request.path = spec.path;
    request.query = spec.query;
    request.requiresAuth = true;
    request.maxRetries = 3;

    const QString requestId = HttpClient::instance().send(request);
    m_requests.insert(requestId, spec);
    m_activeDomains.insert(spec.domain);
}

void RemoteDataBootstrapper::handleSuccess(const QString& requestId, const NetworkResponse& response)
{
    if (!m_requests.contains(requestId)) {
        return;
    }

    const FetchSpec spec = m_requests.value(requestId);
    int itemCount = 0;
    cacheResponse(spec, response, &itemCount);
    emit domainSynced(spec.domain, itemCount);
    finishRequest(requestId);
}

void RemoteDataBootstrapper::handleFailure(const QString& requestId, const NetworkError& error)
{
    if (!m_requests.contains(requestId)) {
        return;
    }

    const FetchSpec spec = m_requests.value(requestId);
    m_failed = true;
    emit domainSyncFailed(spec.domain, error);
    finishRequest(requestId);
}

void RemoteDataBootstrapper::finishRequest(const QString& requestId)
{
    m_requests.remove(requestId);
    if (m_requests.isEmpty()) {
        m_activeDomains.clear();
        m_running = false;
        emit syncFinished(!m_failed);
    }
}

void RemoteDataBootstrapper::cacheResponse(const FetchSpec& spec,
                                           const NetworkResponse& response,
                                           int* itemCount)
{
    const QJsonObject root = response.object();
    ReferenceDataResolver::instance().consumePayload(root);
    QJsonArray items = arrayFromResponse(root, spec.arrayKey);
    if (items.isEmpty() && spec.arrayKey.isEmpty() && !root.isEmpty()) {
        items.append(root);
    }

    LocalDataStore& store = LocalDataStore::instance();
    QVector<QJsonObject> preservedLocalConversations;
    QSet<QString> incomingKeys;
    const bool preservesLocalConversationShells = spec.domain == QStringLiteral("conversations");
    if (preservesLocalConversationShells && spec.clearBeforeStore) {
        for (const QJsonObject& object : store.values(spec.domain)) {
            if (object.contains(QStringLiteral("localMessageListTime"))) {
                preservedLocalConversations.push_back(object);
            }
        }
    }

    if (spec.clearBeforeStore) {
        store.clearDomain(spec.domain);
    }

    int index = spec.query.value(QStringLiteral("offset"), 0).toInt();
    for (const QJsonValue& value : items) {
        QJsonObject object = normalizeForDomain(spec.domain, value.toObject());
        const QString key = cacheKeyFor(object, spec.keyFields, index);
        if (!key.isEmpty()) {
            incomingKeys.insert(key);
            store.upsertValue(spec.domain, key, object);
        }
        if (spec.domain == QStringLiteral("notifications")) {
            updateUnreadStateFromNotification(object);
        }
        ++index;
    }

    for (const QJsonObject& object : preservedLocalConversations) {
        const QString key = cacheKeyFor(object, spec.keyFields, 0);
        if (!key.isEmpty() && !incomingKeys.contains(key)) {
            store.upsertValue(spec.domain, key, object);
        }
    }

    if (itemCount) {
        *itemCount = items.size();
    }
    enqueueNextPageIfNeeded(spec, root, items.size());
}

void RemoteDataBootstrapper::enqueueNextPageIfNeeded(const FetchSpec& spec,
                                                     const QJsonObject& root,
                                                     int itemCount)
{
    if (!spec.paged || itemCount <= 0) {
        return;
    }

    const int offset = spec.query.value(QStringLiteral("offset"), 0).toInt();
    const int limit = spec.query.value(QStringLiteral("limit"), kPageLimit).toInt();
    const int nextOffset = offset + itemCount;
    const int total = root.value(QStringLiteral("total")).toInt(-1);
    const bool hasMore = root.value(QStringLiteral("hasMore")).toBool(total < 0 && itemCount >= limit);
    if ((total >= 0 && nextOffset >= total) || (!hasMore && total < 0)) {
        return;
    }

    FetchSpec nextSpec = spec;
    nextSpec.clearBeforeStore = false;
    nextSpec.query.insert(QStringLiteral("offset"), nextOffset);
    enqueue(nextSpec);
}
