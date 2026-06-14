#include "UserRepository.h"

#include <QCollator>
#include <QDateTime>
#include <QJsonArray>
#include <QImageReader>
#include <QJsonObject>
#include <QMetaObject>
#include <QRunnable>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <utility>

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/network/AppEventBus.h"
#include "shared/network/HttpClient.h"
#include "shared/services/AvatarSource.h"
#include "shared/services/ImageService.h"
#include "app/state/CurrentUser.h"

namespace {

const QString kDefaultFriendGroupId = QStringLiteral("default");
const QString kDefaultFriendGroupName = QStringLiteral("默认分组");

int statusSortRank(UserStatus status)
{
    switch (status) {
    case Online:
        return 0;
    case Mining:
        return 1;
    case Flying:
        return 2;
    case Invisible:
        return 3;
    case Offline:
    default:
        return 4;
    }
}

QString friendVisibleName(const FriendSummary& friendSummary)
{
    if (friendSummary.remark.isEmpty() || friendSummary.nickName.isEmpty()) {
        return friendSummary.displayName;
    }
    return QStringLiteral("%1(%2)").arg(friendSummary.remark, friendSummary.nickName);
}

QString friendVisibleSubtitle(const FriendSummary& friendSummary)
{
    return QStringLiteral("[%1] %2").arg(statusText(friendSummary.status), friendSummary.signature);
}

QString normalizedFriendGroupId(const User& user)
{
    return user.friendGroupId.isEmpty() ? kDefaultFriendGroupId : user.friendGroupId;
}

QString normalizedFriendGroupName(const User& user)
{
    return user.friendGroupName.isEmpty() ? kDefaultFriendGroupName : user.friendGroupName;
}

QString normalizedFriendGroupId(const QString& groupId)
{
    return groupId.isEmpty() ? kDefaultFriendGroupId : groupId;
}

QString normalizedUserKey(const User& user)
{
    if (!user.userUuid.isEmpty()) {
        return user.userUuid;
    }
    return user.id;
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

bool hasAvatarResourcePayload(const QJsonObject& object)
{
    const QJsonObject avatar = object.value(QStringLiteral("avatar")).toObject();
    return object.contains(QStringLiteral("avatarPath")) ||
           object.contains(QStringLiteral("avatarUrl")) ||
           object.contains(QStringLiteral("avatarEtag")) ||
           object.contains(QStringLiteral("avatarContentHash")) ||
           object.contains(QStringLiteral("avatarFileId")) ||
           object.contains(QStringLiteral("avatar_file_id")) ||
           object.contains(QStringLiteral("fileId")) ||
           avatar.contains(QStringLiteral("fileId")) ||
           avatar.contains(QStringLiteral("id")) ||
           avatar.contains(QStringLiteral("avatarUrl")) ||
           avatar.contains(QStringLiteral("url")) ||
           avatar.contains(QStringLiteral("path")) ||
           avatar.contains(QStringLiteral("etag")) ||
           avatar.contains(QStringLiteral("contentHash"));
}

QString avatarFileIdFromSource(const QString& source)
{
    const QUrl url(AvatarSource::cleanForIo(source));
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); ++i) {
        if (parts.at(i) == QStringLiteral("avatar-files")) {
            return QUrl::fromPercentEncoding(parts.at(i + 1).toUtf8());
        }
    }
    return {};
}

bool avatarResourceChanged(const User& next, const User& previous)
{
    if (!next.avatarContentHash.isEmpty()) {
        return next.avatarContentHash != previous.avatarContentHash;
    }
    if (!next.avatarEtag.isEmpty()) {
        return next.avatarEtag != previous.avatarEtag;
    }

    const QString nextFileId = avatarFileIdFromSource(next.avatarPath);
    const QString previousFileId = avatarFileIdFromSource(previous.avatarPath);
    if (!nextFileId.isEmpty() || !previousFileId.isEmpty()) {
        return nextFileId != previousFileId;
    }

    const QString nextSource = AvatarSource::cleanForIo(next.avatarPath);
    const QString previousSource = AvatarSource::cleanForIo(previous.avatarPath);
    return !nextSource.isEmpty() && nextSource != previousSource;
}

void keepAvatarFromPrevious(User& user, const User& previous)
{
    user.avatarPath = previous.avatarPath;
    user.avatarVersion = previous.avatarVersion;
    user.avatarEtag = previous.avatarEtag;
    user.avatarContentHash = previous.avatarContentHash;
}

bool matchesFriendKeyword(const User& user, const QString& keyword)
{
    if (!user.isFriend) {
        return false;
    }

    return keyword.isEmpty() ||
           user.nick.contains(keyword, Qt::CaseInsensitive) ||
           user.remark.contains(keyword, Qt::CaseInsensitive) ||
           user.signature.contains(keyword, Qt::CaseInsensitive) ||
           normalizedFriendGroupName(user).contains(keyword, Qt::CaseInsensitive);
}

bool matchesUserSearchKeyword(const User& user, const QString& keyword)
{
    if (keyword.isEmpty()) {
        return false;
    }

    return user.id.contains(keyword, Qt::CaseInsensitive) ||
           user.userId.contains(keyword, Qt::CaseInsensitive) ||
           user.userUuid.contains(keyword, Qt::CaseInsensitive) ||
           user.nick.contains(keyword, Qt::CaseInsensitive) ||
           user.remark.contains(keyword, Qt::CaseInsensitive);
}

QString userStatusToString(UserStatus status)
{
    switch (status) {
    case Online:
        return QStringLiteral("online");
    case Mining:
        return QStringLiteral("mining");
    case Flying:
        return QStringLiteral("airplane");
    case Invisible:
        return QStringLiteral("invisible");
    case Offline:
    default:
        return QStringLiteral("offline");
    }
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

bool hasPresencePayload(const QJsonObject& object)
{
    return object.contains(QStringLiteral("presence")) ||
           isPresenceStatusValue(object.value(QStringLiteral("status")).toString());
}

QVector<QJsonObject> userObjectsFromResponse(const NetworkResponse& response)
{
    QVector<QJsonObject> users;
    auto appendObject = [&users](const QJsonObject& object) {
        if (!object.isEmpty()) {
            users.push_back(object);
        }
    };
    auto appendArray = [&appendObject](const QJsonArray& array) {
        for (const QJsonValue& value : array) {
            appendObject(value.toObject());
        }
    };

    if (response.body.isArray()) {
        appendArray(response.body.array());
        return users;
    }

    const QJsonObject root = response.object();
    for (const QString& key : {QStringLiteral("users"),
                               QStringLiteral("items"),
                               QStringLiteral("friends")}) {
        const QJsonArray array = root.value(key).toArray();
        if (!array.isEmpty()) {
            appendArray(array);
            return users;
        }
    }

    for (const QString& key : {QStringLiteral("user"),
                               QStringLiteral("profile"),
                               QStringLiteral("friendship")}) {
        appendObject(root.value(key).toObject());
        if (!users.isEmpty()) {
            return users;
        }
    }
    appendObject(root);
    return users;
}

struct FriendGroupRecord {
    QString groupId = kDefaultFriendGroupId;
    QString friendGroupId = kDefaultFriendGroupId;
    QString name = kDefaultFriendGroupName;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

FriendGroupRecord friendGroupFromJson(const QJsonObject& object)
{
    FriendGroupRecord group;
    group.friendGroupId = firstString(object, {QStringLiteral("friendGroupId"),
                                               QStringLiteral("groupId"),
                                               QStringLiteral("id")});
    group.friendGroupId = normalizedFriendGroupId(group.friendGroupId);
    group.groupId = firstString(object, {QStringLiteral("groupId"),
                                         QStringLiteral("friendGroupId"),
                                         QStringLiteral("id")});
    group.groupId = normalizedFriendGroupId(group.groupId);
    group.name = object.value(QStringLiteral("name")).toString(
            object.value(QStringLiteral("groupName")).toString(kDefaultFriendGroupName));
    group.sortOrder = object.value(QStringLiteral("sortOrder")).toInt();
    group.createdAt = dateTimeFromString(object.value(QStringLiteral("createdAt")).toString());
    group.updatedAt = dateTimeFromString(object.value(QStringLiteral("updatedAt")).toString());
    return group;
}

QJsonObject friendGroupToJson(const FriendGroupRecord& group)
{
    return {
            {QStringLiteral("groupId"), group.groupId},
            {QStringLiteral("friendGroupId"), group.friendGroupId},
            {QStringLiteral("name"), group.name},
            {QStringLiteral("sortOrder"), group.sortOrder},
            {QStringLiteral("createdAt"), group.createdAt.isValid()
                                            ? group.createdAt.toUTC().toString(Qt::ISODateWithMs)
                                            : QString()},
            {QStringLiteral("updatedAt"), group.updatedAt.isValid()
                                            ? group.updatedAt.toUTC().toString(Qt::ISODateWithMs)
                                            : QString()}
    };
}

FriendGroupSummary makeFriendGroupSummary(const FriendGroupRecord& group, int friendCount)
{
    return FriendGroupSummary{
            group.friendGroupId,
            group.friendGroupId,
            group.name,
            friendCount,
            group.sortOrder,
            group.createdAt,
            group.updatedAt
    };
}

QJsonObject mergedFriendshipObject(QJsonObject object)
{
    const QJsonObject friendship = object.value(QStringLiteral("friendship")).toObject();
    if (!friendship.isEmpty()) {
        object = friendship;
    }

    const QJsonObject user = object.value(QStringLiteral("user")).toObject();
    if (user.isEmpty()) {
        return object;
    }

    QJsonObject relation = object;
    relation.remove(QStringLiteral("user"));
    object = user;
    for (auto it = relation.constBegin(); it != relation.constEnd(); ++it) {
        if (!object.contains(it.key())) {
            object.insert(it.key(), it.value());
        }
    }
    if (!object.contains(QStringLiteral("isFriend"))) {
        object.insert(QStringLiteral("isFriend"), true);
    }
    return object;
}

User userFromJson(const QJsonObject& object)
{
    const QJsonObject mergedObject = mergedFriendshipObject(object);
    User user;
    user.userUuid = firstString(mergedObject, {QStringLiteral("userUuid"),
                                               QStringLiteral("uuid"),
                                               QStringLiteral("friendUserUuid")});
    user.userId = firstString(mergedObject, {QStringLiteral("userId"),
                                             QStringLiteral("publicId"),
                                             QStringLiteral("public_id")});
    const QString legacyId = mergedObject.value(QStringLiteral("id")).toString();
    if (user.userUuid.isEmpty()) {
        user.userUuid = legacyId;
    }
    if (user.userId.isEmpty()) {
        user.userId = legacyId;
    }
    user.id = user.userUuid.isEmpty() ? user.userId : user.userUuid;
    user.nick = mergedObject.value(QStringLiteral("nick")).toString(
            mergedObject.value(QStringLiteral("nickName")).toString(mergedObject.value(QStringLiteral("displayName")).toString()));
    user.remark = mergedObject.value(QStringLiteral("remark")).toString();
    user.avatarVersion = mergedObject.value(QStringLiteral("avatarVersion")).toInt();
    user.avatarEtag = mergedObject.value(QStringLiteral("avatarEtag")).toString();
    user.avatarContentHash = mergedObject.value(QStringLiteral("avatarContentHash")).toString();
    user.version = mergedObject.value(QStringLiteral("version")).toInt();
    user.etag = mergedObject.value(QStringLiteral("etag")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(mergedObject));
    if (avatarPath.isEmpty()) {
        avatarPath = mergedObject.value(QStringLiteral("avatarPath")).toString(
                mergedObject.value(QStringLiteral("avatarUrl")).toString());
    }
    user.avatarPath = AvatarSource::versioned(
            avatarPath,
            user.avatarVersion,
            user.avatarEtag,
            user.avatarContentHash);
    const QJsonObject presence = presenceObjectFrom(mergedObject);
    user.status = userStatusFromString(presence.value(QStringLiteral("status")).toString());
    user.lastSeenAt = dateTimeFromString(presence.value(QStringLiteral("lastSeenAt")).toString());
    user.signature = mergedObject.value(QStringLiteral("signature")).toString();
    user.isDnd = mergedObject.value(QStringLiteral("isDnd")).toBool(false);
    user.isFriend = mergedObject.value(QStringLiteral("isFriend")).toBool(false);
    user.friendGroupId = mergedObject.value(QStringLiteral("friendGroupId")).toString(kDefaultFriendGroupId);
    user.friendGroupName = mergedObject.value(QStringLiteral("friendGroupName")).toString(kDefaultFriendGroupName);
    user.region = mergedObject.value(QStringLiteral("region")).toString();
    return user;
}

QJsonObject userToJson(const User& user)
{
    return {
            {QStringLiteral("id"), user.id},
            {QStringLiteral("userUuid"), user.userUuid.isEmpty() ? user.id : user.userUuid},
            {QStringLiteral("userId"), user.userId},
            {QStringLiteral("nick"), user.nick},
            {QStringLiteral("remark"), user.remark},
            {QStringLiteral("avatarPath"), user.avatarPath},
            {QStringLiteral("avatarVersion"), user.avatarVersion},
            {QStringLiteral("avatarEtag"), user.avatarEtag},
            {QStringLiteral("avatarContentHash"), user.avatarContentHash},
            {QStringLiteral("version"), user.version},
            {QStringLiteral("etag"), user.etag},
            {QStringLiteral("status"), userStatusToString(user.status)},
            {QStringLiteral("presence"), QJsonObject{
                    {QStringLiteral("status"), userStatusToString(user.status)},
                    {QStringLiteral("lastSeenAt"), user.lastSeenAt.isValid()
                                                   ? user.lastSeenAt.toUTC().toString(Qt::ISODateWithMs)
                                                   : QString()}
            }},
            {QStringLiteral("signature"), user.signature},
            {QStringLiteral("isDnd"), user.isDnd},
            {QStringLiteral("isFriend"), user.isFriend},
            {QStringLiteral("friendGroupId"), user.friendGroupId},
            {QStringLiteral("friendGroupName"), user.friendGroupName},
            {QStringLiteral("region"), user.region}
    };
}

FriendSummary makeFriendSummary(const User& user)
{
    const QString displayName = user.remark.isEmpty() ? user.nick : user.remark;
    return FriendSummary{
            normalizedUserKey(user),
            displayName,
            user.avatarPath,
            user.status,
            user.signature,
            user.isDnd,
            normalizedFriendGroupId(user),
            normalizedFriendGroupName(user),
            user.nick,
            user.remark
    };
}

void sortFriendSummaries(QVector<FriendSummary>& result)
{
    std::sort(result.begin(), result.end(), [](const FriendSummary& lhs, const FriendSummary& rhs) {
        if (lhs.groupName != rhs.groupName) {
            static QCollator groupCollator(QLocale::Chinese);
            groupCollator.setNumericMode(true);
            return groupCollator.compare(lhs.groupName, rhs.groupName) < 0;
        }

        const int lhsStatusRank = statusSortRank(lhs.status);
        const int rhsStatusRank = statusSortRank(rhs.status);
        if (lhsStatusRank != rhsStatusRank) {
            return lhsStatusRank < rhsStatusRank;
        }

        static QCollator localCollator(QLocale::Chinese);
        localCollator.setNumericMode(true);
        const int nameOrder = localCollator.compare(friendVisibleName(lhs), friendVisibleName(rhs));
        if (nameOrder != 0) {
            return nameOrder < 0;
        }

        const int subtitleOrder = localCollator.compare(friendVisibleSubtitle(lhs), friendVisibleSubtitle(rhs));
        if (subtitleOrder != 0) {
            return subtitleOrder < 0;
        }
        return lhs.userId < rhs.userId;
    });
}

class FriendListRequestOperation final
    : public RepositoryTemplate<FriendListRequest, QVector<FriendSummary>> {
public:
    explicit FriendListRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendSummary> doRequest(const FriendListRequest& query) const override
    {
        QVector<FriendSummary> result;
        result.reserve(m_users.size());

        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            result.push_back(makeFriendSummary(user));
        }
        return result;
    }

    void onAfterRequest(const FriendListRequest&, QVector<FriendSummary>& result) const override
    {
        sortFriendSummaries(result);
    }

    QVector<User> m_users;
};

class FriendGroupListRequestOperation final
    : public RepositoryTemplate<FriendGroupListRequest, QVector<FriendGroupSummary>> {
public:
    FriendGroupListRequestOperation(QVector<User> users, QVector<FriendGroupRecord> cachedGroups)
        : m_users(std::move(users))
        , m_cachedGroups(std::move(cachedGroups))
    {
    }

private:
    QVector<FriendGroupSummary> doRequest(const FriendGroupListRequest& query) const override
    {
        QMap<QString, int> friendCounts;
        QMap<QString, QString> groupNamesFromFriends;
        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            const QString groupId = normalizedFriendGroupId(user);
            friendCounts[groupId] = friendCounts.value(groupId) + 1;
            groupNamesFromFriends.insert(groupId, normalizedFriendGroupName(user));
        }

        QMap<QString, FriendGroupSummary> groups;
        if (query.keyword.isEmpty() || kDefaultFriendGroupName.contains(query.keyword, Qt::CaseInsensitive)) {
          groups.insert(kDefaultFriendGroupId,
                          makeFriendGroupSummary(FriendGroupRecord{}, friendCounts.value(kDefaultFriendGroupId)));
        }

        for (const FriendGroupRecord& record : m_cachedGroups) {
            const QString groupId = normalizedFriendGroupId(record.friendGroupId);
            if (groupId == kDefaultFriendGroupId) {
                continue;
            }
            if (!query.keyword.isEmpty() &&
                !record.name.contains(query.keyword, Qt::CaseInsensitive) &&
                friendCounts.value(groupId) <= 0) {
                continue;
            }

            groups.insert(groupId, makeFriendGroupSummary(record, friendCounts.value(groupId)));
        }

        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            const QString groupId = normalizedFriendGroupId(user);
            if (groups.contains(groupId)) {
                continue;
            }

            FriendGroupRecord fallback;
            fallback.groupId = groupId;
            fallback.friendGroupId = groupId;
            fallback.name = groupNamesFromFriends.value(groupId, normalizedFriendGroupName(user));
            FriendGroupSummary summary = makeFriendGroupSummary(fallback, friendCounts.value(groupId));
            groups.insert(groupId, summary);
        }
        return QVector<FriendGroupSummary>::fromList(groups.values());
    }

    void onAfterRequest(const FriendGroupListRequest&, QVector<FriendGroupSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const FriendGroupSummary& lhs, const FriendGroupSummary& rhs) {
            if (lhs.groupId == kDefaultFriendGroupId || rhs.groupId == kDefaultFriendGroupId) {
                return lhs.groupId == kDefaultFriendGroupId && rhs.groupId != kDefaultFriendGroupId;
            }
            if (lhs.sortOrder != rhs.sortOrder) {
                return lhs.sortOrder < rhs.sortOrder;
            }
            static QCollator collator(QLocale::Chinese);
            collator.setNumericMode(true);
            const int order = collator.compare(lhs.groupName, rhs.groupName);
            return order == 0 ? lhs.groupId < rhs.groupId : order < 0;
        });
    }

    QVector<User> m_users;
    QVector<FriendGroupRecord> m_cachedGroups;
};

class FriendGroupItemsRequestOperation final
    : public RepositoryTemplate<FriendGroupItemsRequest, QVector<FriendSummary>> {
public:
    explicit FriendGroupItemsRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendSummary> doRequest(const FriendGroupItemsRequest& query) const override
    {
        QVector<FriendSummary> result;
        for (const User& user : m_users) {
            if (!user.isFriend ||
                normalizedFriendGroupId(user) != query.groupId ||
                !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }
            result.push_back(makeFriendSummary(user));
        }
        return result;
    }

    void onAfterRequest(const FriendGroupItemsRequest& query, QVector<FriendSummary>& result) const override
    {
        sortFriendSummaries(result);
        const int offset = qBound(0, query.offset, result.size());
        const int limit = query.limit < 0 ? result.size() - offset : qMax(0, query.limit);
        result = result.mid(offset, limit);
    }

    QVector<User> m_users;
};

class UserDetailRequestOperation final
    : public RepositoryTemplate<UserDetailRequest, User> {
public:
    explicit UserDetailRequestOperation(QMap<QString, User> users)
        : m_users(std::move(users))
    {
    }

private:
    User doRequest(const UserDetailRequest& query) const override
    {
        return m_users.value(query.userId, User{});
    }

    QMap<QString, User> m_users;
};

} // namespace

UserRepository::UserRepository(QObject* parent)
    : QObject(parent)
{
    reloadFromStore();

    connect(&LocalDataStore::instance(),
            &LocalDataStore::activeAccountChanged,
            this,
            [this](const QString&) {
                m_pendingUserProfileRefreshRequestIds.clear();
                m_pendingPresenceBatchRequestIds.clear();
                m_pendingPresenceSnapshotRequestIds.clear();
                {
                    QMutexLocker locker(&mutex);
                    m_presenceSnapshotRequestedUserIds.clear();
                }
                reloadFromStore();
            });
    connect(&LocalDataStore::instance(),
            &LocalDataStore::domainChanged,
            this,
            [this](const QString& domain) {
                if (domain == QStringLiteral("users") || domain == QStringLiteral("friend_groups")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
        if (type == QStringLiteral("presence.updated")) {
            const QString userUuid = firstString(payload, {QStringLiteral("userUuid"),
                                                           QStringLiteral("uuid"),
                                                           QStringLiteral("userId")});
            const QJsonObject presence = presenceObjectFrom(payload);
            upsertPresence(userUuid,
                           presence.value(QStringLiteral("status")).toString(),
                           presence.value(QStringLiteral("lastSeenAt")).toString());
            return;
        }

        if (type == QStringLiteral("friend.deleted")) {
            const QString actorUserUuid = firstString(payload, {QStringLiteral("userUuid"),
                                                                QStringLiteral("fromUserUuid")});
            const QString friendUserUuid = firstString(payload, {QStringLiteral("friendUserUuid"),
                                                                 QStringLiteral("toUserUuid")});
            QString removedFriendUuid;
            const CurrentUser& currentUser = CurrentUser::instance();
            if (currentUser.isCurrentUserId(actorUserUuid)) {
                removedFriendUuid = friendUserUuid;
            } else if (currentUser.isCurrentUserId(friendUserUuid)) {
                removedFriendUuid = actorUserUuid;
            }
            if (!removedFriendUuid.isEmpty()) {
                removeUser(removedFriendUuid);
            }
            return;
        }

        if (type == QStringLiteral("friend.updated")) {
            QJsonObject object = payload.value(QStringLiteral("friendship")).toObject();
            if (object.isEmpty()) {
                object = payload;
            }
            upsertUserProfile(object, false);
            return;
        }

        if (type == QStringLiteral("friend.group.created") ||
            type == QStringLiteral("friend.group.updated")) {
            QJsonObject object = payload.value(QStringLiteral("group")).toObject();
            if (object.isEmpty()) {
                object = payload;
            }
            upsertFriendGroup(object);
            return;
        }

        if (type == QStringLiteral("friend.group.deleted")) {
            const QString friendGroupId = firstString(payload, {QStringLiteral("friendGroupId"),
                                                                QStringLiteral("groupId"),
                                                                QStringLiteral("id")});
            removeFriendGroup(friendGroupId);
            return;
        }

        if (type != QStringLiteral("profile.updated") &&
            type != QStringLiteral("public_id.updated")) {
            return;
        }

        QJsonObject object = payload.value(QStringLiteral("profile")).toObject();
        if (object.isEmpty()) {
            object = payload;
        }

        const QString userId = firstString(payload, {QStringLiteral("userId"),
                                                     QStringLiteral("publicId"),
                                                     QStringLiteral("public_id"),
                                                     QStringLiteral("publicUserId"),
                                                     QStringLiteral("newUserId"),
                                                     QStringLiteral("newPublicId"),
                                                     QStringLiteral("new_public_id")});
        const QString userUuid = payload.value(QStringLiteral("userUuid")).toString();
        if (!userId.isEmpty() && !object.contains(QStringLiteral("userId"))) {
            object.insert(QStringLiteral("userId"), userId);
        }
        if (!userUuid.isEmpty() && !object.contains(QStringLiteral("userUuid"))) {
            object.insert(QStringLiteral("userUuid"), userUuid);
        }

        for (const QString& key : {QStringLiteral("avatarUrl"),
                                   QStringLiteral("avatarFileId"),
                                   QStringLiteral("fileId"),
                                   QStringLiteral("avatarVersion"),
                                   QStringLiteral("avatarEtag"),
                                   QStringLiteral("avatarContentHash")}) {
            if (payload.contains(key) && !object.contains(key)) {
                object.insert(key, payload.value(key));
            }
        }
        if (payload.contains(QStringLiteral("avatar")) && !object.contains(QStringLiteral("avatar"))) {
            object.insert(QStringLiteral("avatar"), payload.value(QStringLiteral("avatar")));
        }

        upsertUserProfile(object);
    });

    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            [this](const QString& requestId, const NetworkResponse& response) {
                if (m_pendingUserProfileRefreshRequestIds.contains(requestId)) {
                    m_pendingUserProfileRefreshRequestIds.take(requestId);
                    for (const QJsonObject& object : userObjectsFromResponse(response)) {
                        upsertUserProfile(object);
                    }
                    return;
                }

                if (!m_pendingPresenceBatchRequestIds.remove(requestId)) {
                    return;
                }

                m_pendingPresenceSnapshotRequestIds.take(requestId);

                QJsonArray presences;
                if (response.body.isArray()) {
                    presences = response.body.array();
                } else {
                    const QJsonObject root = response.object();
                    presences = root.value(QStringLiteral("presences")).toArray();
                    if (presences.isEmpty()) {
                        presences = root.value(QStringLiteral("items")).toArray();
                    }
                    if (presences.isEmpty()) {
                        presences = root.value(QStringLiteral("users")).toArray();
                    }
                }

                for (const QJsonValue& value : presences) {
                    const QJsonObject object = value.toObject();
                    const QString userUuid = firstString(object, {QStringLiteral("userUuid"),
                                                                  QStringLiteral("uuid"),
                                                                  QStringLiteral("userId")});
                    const QJsonObject presence = presenceObjectFrom(object);
                    upsertPresence(userUuid,
                                   presence.value(QStringLiteral("status")).toString(),
                                   presence.value(QStringLiteral("lastSeenAt")).toString());
                }
            });
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            [this](const QString& requestId, const NetworkError&) {
                m_pendingUserProfileRefreshRequestIds.remove(requestId);
                m_pendingPresenceBatchRequestIds.remove(requestId);
                const QStringList snapshotUserUuids =
                        m_pendingPresenceSnapshotRequestIds.take(requestId);
                if (!snapshotUserUuids.isEmpty()) {
                    QMutexLocker locker(&mutex);
                    for (const QString& userUuid : snapshotUserUuids) {
                        m_presenceSnapshotRequestedUserIds.remove(userUuid);
                    }
                }
            });
}

UserRepository& UserRepository::instance()
{
    static UserRepository repo;
    return repo;
}

void UserRepository::reloadFromStore()
{
    QMap<QString, User> nextUsers;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("users"))) {
        const User user = userFromJson(object);
        if (!user.id.isEmpty()) {
            nextUsers.insert(user.id, user);
        }
    }

    {
        QMutexLocker locker(&mutex);
        userMap = nextUsers;
    }
    emit friendListChanged();
}

QVector<FriendSummary> UserRepository::requestFriendList(const FriendListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return FriendListRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
}

QVector<FriendGroupSummary> UserRepository::requestFriendGroupSummaries(const FriendGroupListRequest& query) const
{
    QMutexLocker locker(&mutex);
    const QVector<User> users = QVector<User>::fromList(userMap.values());
    locker.unlock();

    QVector<FriendGroupRecord> groups;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("friend_groups"))) {
        FriendGroupRecord group = friendGroupFromJson(object);
        if (!group.friendGroupId.isEmpty() && group.friendGroupId != kDefaultFriendGroupId) {
            groups.push_back(group);
        }
    }
    return FriendGroupListRequestOperation(users, groups).request(query);
}

QVector<FriendSummary> UserRepository::requestFriendsInGroup(const FriendGroupItemsRequest& query) const
{
    QMutexLocker locker(&mutex);
    return FriendGroupItemsRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
}

QVector<User> UserRepository::requestUserSearch(const QString& keyword, int limit, int offset) const
{
    const QString trimmedKeyword = keyword.trimmed();
    QMutexLocker locker(&mutex);

    QVector<User> result;
    result.reserve(qMin(qMax(0, limit), userMap.size()));
    for (const User& user : userMap) {
        if (!matchesUserSearchKeyword(user, trimmedKeyword)) {
            continue;
        }
        result.push_back(user);
    }

    locker.unlock();

    std::sort(result.begin(), result.end(), [](const User& lhs, const User& rhs) {
        if (lhs.isFriend != rhs.isFriend) {
            return lhs.isFriend && !rhs.isFriend;
        }

        static QCollator collator(QLocale::Chinese);
        collator.setNumericMode(true);
        const int nameOrder = collator.compare(lhs.nick, rhs.nick);
        if (nameOrder != 0) {
            return nameOrder < 0;
        }
        return lhs.id < rhs.id;
    });

    const int boundedOffset = qBound(0, offset, result.size());
    const int boundedLimit = limit < 0 ? result.size() - boundedOffset : qMax(0, limit);
    return result.mid(boundedOffset, boundedLimit);
}

QVector<User> UserRepository::requestAllUsers() const
{
    QMutexLocker locker(&mutex);
    return QVector<User>::fromList(userMap.values());
}

User UserRepository::requestUserDetail(const UserDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    const User direct = userMap.value(query.userId, User{});
    if (!direct.id.isEmpty()) {
        return direct;
    }
    for (const User& user : userMap) {
        if (user.id == query.userId ||
            user.userUuid == query.userId ||
            user.userId == query.userId) {
            return user;
        }
    }
    return {};
}

QVector<User> UserRepository::requestUserDetails(const QStringList& userIds) const
{
    QMutexLocker locker(&mutex);

    QVector<User> result;
    result.reserve(userIds.size());
    QSet<QString> seen;
    for (const QString& userId : userIds) {
        if (userId.isEmpty() || seen.contains(userId)) {
            continue;
        }
        seen.insert(userId);
        User user = userMap.value(userId, User{});
        if (user.id.isEmpty()) {
            for (const User& candidate : userMap) {
                if (candidate.id == userId ||
                    candidate.userUuid == userId ||
                    candidate.userId == userId) {
                    user = candidate;
                    break;
                }
            }
        }
        if (!user.id.isEmpty()) {
            result.push_back(user);
        }
    }
    return result;
}

QString UserRepository::requestUserName(const QString& userId) const
{
    const User user = requestUserDetail({userId});
    if (!user.nick.isEmpty()) {
        return user.nick;
    }
    if (!user.userId.isEmpty()) {
        return user.userId;
    }
    return user.id;
}

QString UserRepository::requestUserAvatarPath(const QString& userId) const
{
    return requestUserDetail({userId}).avatarPath;
}

int UserRepository::requestUserVersion(const QString& userId) const
{
    return requestUserDetail({userId}).version;
}

QString UserRepository::requestUserAvatarImageAsync(const QString& userId, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString source = requestUserAvatarPath(userId);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, userId, source, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        QImageReader reader(AvatarSource::cleanForIo(source));
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        QMetaObject::invokeMethod(this, [this, requestId, userId, image]() {
            if (image.isNull()) {
                emit userAvatarImageFailed(requestId, userId);
                return;
            }
            emit userAvatarImageReady(requestId, userId, image);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

QMap<QString, QString> UserRepository::requestFriendGroups() const
{
    QMap<QString, QString> groups;
    groups.insert(kDefaultFriendGroupId, kDefaultFriendGroupName);
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("friend_groups"))) {
        const FriendGroupRecord group = friendGroupFromJson(object);
        if (!group.friendGroupId.isEmpty() && group.friendGroupId != kDefaultFriendGroupId) {
            groups.insert(group.friendGroupId, group.name);
        }
    }

    QMutexLocker locker(&mutex);
    for (const User& user : userMap) {
        if (!user.isFriend) {
            continue;
        }
        groups.insert(normalizedFriendGroupId(user), normalizedFriendGroupName(user));
    }
    return groups;
}

int UserRepository::nextFriendGroupSortOrder() const
{
    int maxSortOrder = 0;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("friend_groups"))) {
        maxSortOrder = qMax(maxSortOrder, object.value(QStringLiteral("sortOrder")).toInt());
    }
    return maxSortOrder + 10;
}

void UserRepository::saveUser(const User& user)
{
    if (normalizedUserKey(user).isEmpty()) {
        return;
    }

    User normalized = user;
    normalized.id = normalizedUserKey(user);
    if (normalized.userUuid.isEmpty()) {
        normalized.userUuid = normalized.id;
    }

    bool changed = false;
    QString oldAvatarPath;
    QMutexLocker locker(&mutex);
    const User previous = userMap.value(normalized.id);
    oldAvatarPath = previous.avatarPath;
    changed = !userMap.contains(normalized.id) || previous.userUuid != normalized.userUuid
            || previous.userId != normalized.userId
            || previous.nick != normalized.nick
            || previous.remark != normalized.remark
            || previous.avatarPath != normalized.avatarPath
            || previous.avatarVersion != normalized.avatarVersion
            || previous.avatarEtag != normalized.avatarEtag
            || previous.avatarContentHash != normalized.avatarContentHash
            || previous.version != normalized.version
            || previous.etag != normalized.etag
            || previous.status != normalized.status
            || previous.lastSeenAt != normalized.lastSeenAt
            || previous.signature != normalized.signature
            || previous.isDnd != normalized.isDnd
            || previous.isFriend != normalized.isFriend
            || previous.friendGroupId != normalized.friendGroupId
            || previous.friendGroupName != normalized.friendGroupName
            || previous.region != normalized.region;
    userMap[normalized.id] = normalized;
    locker.unlock();
    LocalDataStore::instance().upsertValue(QStringLiteral("users"), normalized.id, userToJson(normalized));

    if (!oldAvatarPath.isEmpty() && oldAvatarPath != user.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    if (changed) {
        emit friendListChanged();
    }
}

bool UserRepository::upsertUserProfile(const QJsonObject& object, bool preserveFriendFields)
{
    User user = userFromJson(object);
    if (user.id.isEmpty()) {
        return false;
    }

    const User previous = requestUserDetail({user.id});
    if (!previous.id.isEmpty()) {
        if (previous.version > 0 && user.version > 0 && user.version < previous.version) {
            return false;
        }
        const bool avatarResourcePayload = hasAvatarResourcePayload(object);
        if (!avatarResourcePayload ||
            !avatarResourceChanged(user, previous) ||
            (previous.avatarVersion > 0 &&
             user.avatarVersion > 0 &&
             user.avatarVersion < previous.avatarVersion)) {
            keepAvatarFromPrevious(user, previous);
        }
        if (user.userId.isEmpty()) {
            user.userId = previous.userId;
        }
        if (user.nick.isEmpty()) {
            user.nick = previous.nick;
        }
        if (preserveFriendFields) {
            user.remark = previous.remark;
            user.isDnd = previous.isDnd;
            user.isFriend = previous.isFriend;
            user.friendGroupId = previous.friendGroupId;
            user.friendGroupName = previous.friendGroupName;
        }
        user.status = hasPresencePayload(object) ? user.status : previous.status;
        user.lastSeenAt = hasPresencePayload(object) ? user.lastSeenAt : previous.lastSeenAt;
        user.signature = object.contains(QStringLiteral("signature")) ? user.signature : previous.signature;
        user.region = object.contains(QStringLiteral("region")) ? user.region : previous.region;
    }

    saveUser(user);
    return true;
}

bool UserRepository::upsertFriendGroup(const QJsonObject& object)
{
    const FriendGroupRecord group = friendGroupFromJson(object);
    if (group.friendGroupId.isEmpty() || group.friendGroupId == kDefaultFriendGroupId) {
        return false;
    }

    return LocalDataStore::instance().upsertValue(QStringLiteral("friend_groups"),
                                                  group.friendGroupId,
                                                  friendGroupToJson(group));
}

bool UserRepository::removeFriendGroup(const QString& friendGroupId)
{
    const QString normalizedGroupId = normalizedFriendGroupId(friendGroupId);
    if (normalizedGroupId.isEmpty() || normalizedGroupId == kDefaultFriendGroupId) {
        return false;
    }

    bool changed = false;
    QVector<User> changedUsers;
    {
        QMutexLocker locker(&mutex);
        for (auto it = userMap.begin(); it != userMap.end(); ++it) {
            if (!it->isFriend || normalizedFriendGroupId(*it) != normalizedGroupId) {
                continue;
            }
            it->friendGroupId = kDefaultFriendGroupId;
            it->friendGroupName = kDefaultFriendGroupName;
            changedUsers.push_back(*it);
            changed = true;
        }
    }

    for (const User& user : changedUsers) {
        LocalDataStore::instance().upsertValue(QStringLiteral("users"), user.id, userToJson(user));
    }
    const bool removed = LocalDataStore::instance().removeValue(QStringLiteral("friend_groups"), normalizedGroupId);
    if (changed && !removed) {
        emit friendListChanged();
    }
    return removed || changed;
}

bool UserRepository::upsertPresence(const QString& userUuid, const QString& status, const QString& lastSeenAt)
{
    if (userUuid.isEmpty()) {
        return false;
    }

    const UserStatus nextStatus = userStatusFromString(status);
    const QDateTime nextLastSeenAt = dateTimeFromString(lastSeenAt);

    bool changed = false;
    QString storageKey;
    User updatedUser;
    {
        QMutexLocker locker(&mutex);
        auto it = userMap.find(userUuid);
        if (it == userMap.end()) {
            for (auto candidate = userMap.begin(); candidate != userMap.end(); ++candidate) {
                if (candidate->id == userUuid ||
                    candidate->userUuid == userUuid ||
                    candidate->userId == userUuid) {
                    it = candidate;
                    break;
                }
            }
        }
        if (it == userMap.end()) {
            return false;
        }

        changed = it->status != nextStatus ||
                  (nextLastSeenAt.isValid() && it->lastSeenAt != nextLastSeenAt);
        it->status = nextStatus;
        if (nextLastSeenAt.isValid()) {
            it->lastSeenAt = nextLastSeenAt;
        }
        storageKey = it->id;
        updatedUser = *it;
    }

    if (!storageKey.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("users"), storageKey, userToJson(updatedUser));
    }
    if (changed) {
        emit friendListChanged();
    }
    return true;
}

QString UserRepository::refreshUserProfile(const QString& userId)
{
    const User user = requestUserDetail({userId});
    if (user.id.isEmpty()) {
        return {};
    }

    const QString userUuid = user.userUuid.isEmpty() ? user.id : user.userUuid;
    if (userUuid.trimmed().isEmpty()) {
        return {};
    }

    QJsonObject item{
            {QStringLiteral("userUuid"), userUuid}
    };
    if (user.version > 0) {
        item.insert(QStringLiteral("knownVersion"), user.version);
    }

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/users/batch"),
            {{QStringLiteral("items"), QJsonArray{item}}});
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    if (!requestId.isEmpty()) {
        m_pendingUserProfileRefreshRequestIds.insert(requestId, user.id);
    }
    return requestId;
}

QString UserRepository::refreshPresenceBatch(const QStringList& userUuids)
{
    QJsonArray array;
    QSet<QString> seen;
    for (const QString& userUuid : userUuids) {
        const QString normalized = userUuid.trimmed();
        if (normalized.isEmpty() || seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);
        array.append(normalized);
    }
    if (array.isEmpty()) {
        return {};
    }

    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/presence/batch"),
                                                  {{QStringLiteral("userUuids"), array}});
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingPresenceBatchRequestIds.insert(requestId);
    return requestId;
}

QString UserRepository::refreshFriendPresenceSnapshot()
{
    QStringList userUuids;
    {
        QMutexLocker locker(&mutex);
        for (const User& user : userMap) {
            if (!user.isFriend) {
                continue;
            }

            const QString userUuid = (user.userUuid.isEmpty() ? user.id : user.userUuid).trimmed();
            if (userUuid.isEmpty() || m_presenceSnapshotRequestedUserIds.contains(userUuid)) {
                continue;
            }
            userUuids.push_back(userUuid);
        }
    }

    const QString requestId = refreshPresenceBatch(userUuids);
    if (requestId.isEmpty()) {
        return {};
    }

    {
        QMutexLocker locker(&mutex);
        for (const QString& userUuid : std::as_const(userUuids)) {
            m_presenceSnapshotRequestedUserIds.insert(userUuid);
        }
    }
    m_pendingPresenceSnapshotRequestIds.insert(requestId, userUuids);
    return requestId;
}

bool UserRepository::needsUserRefresh(const QString& userUuid, int remoteVersion) const
{
    const User user = requestUserDetail({userUuid});
    if (user.id.isEmpty()) {
        return true;
    }
    return remoteVersion > 0 && (user.version <= 0 || user.version < remoteVersion);
}

bool UserRepository::isFriend(const QString& userId) const
{
    return requestUserDetail({userId}).isFriend;
}

void UserRepository::addFriend(const QString& userId,
                               const QString& groupId,
                               const QString& groupName)
{
    if (userId.isEmpty()) {
        return;
    }

    bool changed = false;
    QString storageKey;
    User updatedUser;
    {
        QMutexLocker locker(&mutex);
        auto it = userMap.find(userId);
        if (it == userMap.end()) {
            for (auto candidate = userMap.begin(); candidate != userMap.end(); ++candidate) {
                if (candidate->id == userId ||
                    candidate->userUuid == userId ||
                    candidate->userId == userId) {
                    it = candidate;
                    break;
                }
            }
        }
        if (it == userMap.end()) {
            return;
        }

        changed = !it->isFriend ||
                  it->friendGroupId != groupId ||
                  it->friendGroupName != groupName;
        it->isFriend = true;
        it->friendGroupId = groupId.isEmpty() ? kDefaultFriendGroupId : groupId;
        it->friendGroupName = groupName.isEmpty() ? kDefaultFriendGroupName : groupName;
        storageKey = it->id;
        updatedUser = *it;
    }
    if (!storageKey.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("users"), storageKey, userToJson(updatedUser));
    }

    if (changed) {
        emit friendListChanged();
    }
}

void UserRepository::removeUser(const QString& userID)
{
    bool changed = false;
    QString storageKey;
    User updatedUser;
    {
        QMutexLocker locker(&mutex);
        auto it = userMap.find(userID);
        if (it == userMap.end()) {
            for (auto candidate = userMap.begin(); candidate != userMap.end(); ++candidate) {
                if (candidate->id == userID ||
                    candidate->userUuid == userID ||
                    candidate->userId == userID) {
                    it = candidate;
                    break;
                }
            }
        }
        if (it != userMap.end() && it->isFriend) {
            it->isFriend = false;
            it->remark.clear();
            it->friendGroupId = kDefaultFriendGroupId;
            it->friendGroupName = kDefaultFriendGroupName;
            it->isDnd = false;
            storageKey = it->id;
            updatedUser = *it;
            changed = true;
        }
    }
    if (!storageKey.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("users"), storageKey, userToJson(updatedUser));
    }
    if (changed) {
        emit friendListChanged();
    }
}

QString statusText(UserStatus userStatus)
{
    if (userStatus == Online) return "在线";
    if (userStatus == Offline) return "离线";
    if (userStatus == Flying) return "飞行模式";
    if (userStatus == Invisible) return "隐身";
    return "挖矿中";
}

QString statusIconPath(UserStatus userStatus)
{
    const QString prefix = ":/resources/icon/";
    if (userStatus == Online) return prefix + "online.png";
    if (userStatus == Offline) return prefix + "offline.png";
    if (userStatus == Flying) return prefix + "flying.png";
    if (userStatus == Invisible) return prefix + "invisible.png";
    return prefix + "mining.png";
}
