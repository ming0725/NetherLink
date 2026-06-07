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
#include <QUuid>

#include <algorithm>

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

bool hasAvatarPayload(const QJsonObject& object)
{
    return object.contains(QStringLiteral("avatar")) ||
           object.contains(QStringLiteral("avatarPath")) ||
           object.contains(QStringLiteral("avatarUrl")) ||
           object.contains(QStringLiteral("avatarVersion")) ||
           object.contains(QStringLiteral("avatarEtag")) ||
           object.contains(QStringLiteral("avatarContentHash")) ||
           object.contains(QStringLiteral("avatarFileId")) ||
           object.contains(QStringLiteral("avatar_file_id")) ||
           object.contains(QStringLiteral("fileId"));
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

bool hasPresencePayload(const QJsonObject& object)
{
    return object.contains(QStringLiteral("presence")) ||
           object.contains(QStringLiteral("status")) ||
           object.contains(QStringLiteral("lastSeenAt"));
}

User userFromJson(const QJsonObject& object)
{
    User user;
    user.userUuid = firstString(object, {QStringLiteral("userUuid"),
                                         QStringLiteral("uuid"),
                                         QStringLiteral("friendUserUuid")});
    user.userId = firstString(object, {QStringLiteral("userId"),
                                       QStringLiteral("publicId"),
                                       QStringLiteral("public_id")});
    const QString legacyId = object.value(QStringLiteral("id")).toString();
    if (user.userUuid.isEmpty()) {
        user.userUuid = legacyId;
    }
    if (user.userId.isEmpty()) {
        user.userId = legacyId;
    }
    user.id = user.userUuid.isEmpty() ? user.userId : user.userUuid;
    user.nick = object.value(QStringLiteral("nick")).toString(
            object.value(QStringLiteral("nickName")).toString(object.value(QStringLiteral("displayName")).toString()));
    user.remark = object.value(QStringLiteral("remark")).toString();
    user.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    user.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    user.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    user.version = object.value(QStringLiteral("version")).toInt();
    user.etag = object.value(QStringLiteral("etag")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = object.value(QStringLiteral("avatarPath")).toString(
                object.value(QStringLiteral("avatarUrl")).toString());
    }
    user.avatarPath = AvatarSource::versioned(
            avatarPath,
            user.avatarVersion,
            user.avatarEtag,
            user.avatarContentHash);
    const QJsonObject presence = presenceObjectFrom(object);
    user.status = userStatusFromString(presence.value(QStringLiteral("status")).toString());
    user.lastSeenAt = dateTimeFromString(presence.value(QStringLiteral("lastSeenAt")).toString());
    user.signature = object.value(QStringLiteral("signature")).toString();
    user.isDnd = object.value(QStringLiteral("isDnd")).toBool(false);
    user.isFriend = object.value(QStringLiteral("isFriend")).toBool(false);
    user.friendGroupId = object.value(QStringLiteral("friendGroupId")).toString(kDefaultFriendGroupId);
    user.friendGroupName = object.value(QStringLiteral("friendGroupName")).toString(kDefaultFriendGroupName);
    user.region = object.value(QStringLiteral("region")).toString();
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
    explicit FriendGroupListRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendGroupSummary> doRequest(const FriendGroupListRequest& query) const override
    {
        QMap<QString, FriendGroupSummary> groups;
        if (query.keyword.isEmpty() || kDefaultFriendGroupName.contains(query.keyword, Qt::CaseInsensitive)) {
            groups.insert(kDefaultFriendGroupId,
                          FriendGroupSummary{kDefaultFriendGroupId, kDefaultFriendGroupName, 0});
        }
        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            const QString groupId = normalizedFriendGroupId(user);
            FriendGroupSummary summary = groups.value(groupId);
            summary.groupId = groupId;
            summary.groupName = normalizedFriendGroupName(user);
            ++summary.friendCount;
            groups.insert(groupId, summary);
        }
        return QVector<FriendGroupSummary>::fromList(groups.values());
    }

    void onAfterRequest(const FriendGroupListRequest&, QVector<FriendGroupSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const FriendGroupSummary& lhs, const FriendGroupSummary& rhs) {
            static QCollator collator(QLocale::Chinese);
            collator.setNumericMode(true);
            const int order = collator.compare(lhs.groupName, rhs.groupName);
            return order == 0 ? lhs.groupId < rhs.groupId : order < 0;
        });
    }

    QVector<User> m_users;
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
                reloadFromStore();
            });
    connect(&LocalDataStore::instance(),
            &LocalDataStore::domainChanged,
            this,
            [this](const QString& domain) {
                if (domain == QStringLiteral("users")) {
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
            upsertPresence(userUuid,
                           payload.value(QStringLiteral("status")).toString(),
                           payload.value(QStringLiteral("lastSeenAt")).toString());
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

        if (type != QStringLiteral("profile.updated")) {
            return;
        }

        QJsonObject object = payload.value(QStringLiteral("profile")).toObject();
        if (object.isEmpty()) {
            object = payload;
        }

        const QString userId = payload.value(QStringLiteral("userId")).toString();
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
                if (!m_pendingPresenceBatchRequestIds.remove(requestId)) {
                    return;
                }

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
                m_pendingPresenceBatchRequestIds.remove(requestId);
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
    return FriendGroupListRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
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
    QMutexLocker locker(&mutex);
    QMap<QString, QString> groups;
    groups.insert(kDefaultFriendGroupId, kDefaultFriendGroupName);
    for (const User& user : userMap) {
        if (!user.isFriend) {
            continue;
        }
        groups.insert(normalizedFriendGroupId(user), normalizedFriendGroupName(user));
    }
    return groups;
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
        const bool avatarPayload = hasAvatarPayload(object);
        if (!avatarPayload ||
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
