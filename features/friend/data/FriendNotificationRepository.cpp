#include "FriendNotificationRepository.h"

#include <algorithm>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/network/AppEventBus.h"
#include "shared/network/ReferenceDataResolver.h"

namespace {

const QString kFriendRequestUnreadScope = QStringLiteral("friend_requests");

class NotificationListOperation final
    : public RepositoryTemplate<FriendNotificationListRequest, QVector<FriendNotification>>
{
public:
    NotificationListOperation(QVector<FriendNotification> notifications,
                              FriendNotificationListRequest query)
        : m_notifications(std::move(notifications))
        , m_query(std::move(query))
    {
    }

private:
    QVector<FriendNotification> doRequest(const FriendNotificationListRequest&) const override
    {
        const int offset = qBound(0, m_query.offset, m_notifications.size());
        const int limit = m_query.limit < 0
                ? m_notifications.size() - offset
                : qMax(0, m_query.limit);
        return m_notifications.mid(offset, limit);
    }

    QVector<FriendNotification> m_notifications;
    FriendNotificationListRequest m_query;
};

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

QString notificationSourceTypeToString(NotificationSourceType type)
{
    switch (type) {
    case NotificationSourceType::GroupChat:
        return QStringLiteral("group_chat");
    case NotificationSourceType::FriendShare:
        return QStringLiteral("friend_share");
    case NotificationSourceType::IdSearch:
    default:
        return QStringLiteral("id_search");
    }
}

NotificationSourceType notificationSourceTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("group_chat")) {
        return NotificationSourceType::GroupChat;
    }
    if (normalized == QStringLiteral("friend_share")) {
        return NotificationSourceType::FriendShare;
    }
    return NotificationSourceType::IdSearch;
}

QString notificationStatusToString(NotificationStatus status)
{
    switch (status) {
    case NotificationStatus::Accepted:
        return QStringLiteral("accepted");
    case NotificationStatus::Rejected:
        return QStringLiteral("rejected");
    case NotificationStatus::Pending:
    default:
        return QStringLiteral("pending");
    }
}

NotificationStatus notificationStatusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("accepted")) {
        return NotificationStatus::Accepted;
    }
    if (normalized == QStringLiteral("rejected")) {
        return NotificationStatus::Rejected;
    }
    return NotificationStatus::Pending;
}

bool looksLikeUuid(const QString& value)
{
    return !value.isEmpty() && !QUuid::fromString(value).isNull();
}

QJsonObject notificationToJson(const FriendNotification& notification)
{
    return {
            {QStringLiteral("id"), notification.id},
            {QStringLiteral("fromUserId"), notification.fromUserId},
            {QStringLiteral("fromUserUuid"), notification.fromUserUuid},
            {QStringLiteral("message"), notification.message},
            {QStringLiteral("requestDate"), notification.requestDate.toString(Qt::ISODateWithMs)},
            {QStringLiteral("sourceType"), notificationSourceTypeToString(notification.sourceType)},
            {QStringLiteral("unread"), notification.unread},
            {QStringLiteral("sourceGroupId"), notification.sourceGroupId},
            {QStringLiteral("sourceGroupName"), notification.sourceGroupName},
            {QStringLiteral("sourceGroupMemberName"), notification.sourceGroupMemberName},
            {QStringLiteral("sourceFriendId"), notification.sourceFriendId},
            {QStringLiteral("sourceFriendName"), notification.sourceFriendName},
            {QStringLiteral("status"), notificationStatusToString(notification.status)}
    };
}

FriendNotification notificationFromJson(const QJsonObject& object)
{
    FriendNotification notification;
    notification.id = firstString(object, {QStringLiteral("id"),
                                           QStringLiteral("requestId"),
                                           QStringLiteral("notificationId"),
                                           QStringLiteral("sourceId")});
    notification.fromUserId = firstString(object, {QStringLiteral("fromUserId"),
                                                   QStringLiteral("actorUserId"),
                                                   QStringLiteral("senderUserId")});
    notification.fromUserUuid = firstString(object, {QStringLiteral("fromUserUuid"),
                                                     QStringLiteral("actorUuid"),
                                                     QStringLiteral("senderUserUuid")});
    if (notification.fromUserId.isEmpty() || notification.fromUserUuid.isEmpty()) {
        for (const QString& key : {QStringLiteral("fromUser"),
                                   QStringLiteral("actor"),
                                   QStringLiteral("sender"),
                                   QStringLiteral("user")}) {
            const QJsonObject user = object.value(key).toObject();
            if (notification.fromUserId.isEmpty()) {
                notification.fromUserId = firstString(user, {QStringLiteral("userId"),
                                                             QStringLiteral("publicId"),
                                                             QStringLiteral("public_id")});
            }
            if (notification.fromUserUuid.isEmpty()) {
                notification.fromUserUuid = firstString(user, {QStringLiteral("userUuid"),
                                                               QStringLiteral("uuid")});
            }
            if (!notification.fromUserId.isEmpty() && !notification.fromUserUuid.isEmpty()) {
                break;
            }
        }
    }
    if (notification.fromUserUuid.isEmpty() && looksLikeUuid(notification.fromUserId)) {
        notification.fromUserUuid = notification.fromUserId;
        notification.fromUserId.clear();
    }
    notification.message = object.value(QStringLiteral("message")).toString();
    notification.requestDate = QDateTime::fromString(firstString(object, {QStringLiteral("requestDate"),
                                                                          QStringLiteral("createdAt"),
                                                                          QStringLiteral("updatedAt")}),
                                                     Qt::ISODateWithMs);
    notification.sourceType = notificationSourceTypeFromString(object.value(QStringLiteral("sourceType")).toString());
    notification.unread = object.value(QStringLiteral("unread")).toBool(object.value(QStringLiteral("readAt")).isNull());
    notification.sourceGroupId = object.value(QStringLiteral("sourceGroupId")).toString();
    notification.sourceGroupName = object.value(QStringLiteral("sourceGroupName")).toString();
    notification.sourceGroupMemberName = object.value(QStringLiteral("sourceGroupMemberName")).toString();
    notification.sourceFriendId = object.value(QStringLiteral("sourceFriendId")).toString();
    notification.sourceFriendName = object.value(QStringLiteral("sourceFriendName")).toString();
    notification.status = notificationStatusFromString(object.value(QStringLiteral("status")).toString());
    return notification;
}

QStringList currentUserIdentifiers()
{
    const CurrentUserProfile profile = CurrentUser::instance().identity();
    QStringList ids;
    for (const QString& id : {CurrentUser::instance().getUserId(), profile.userUuid}) {
        const QString trimmed = id.trimmed();
        if (!trimmed.isEmpty() && !ids.contains(trimmed)) {
            ids.push_back(trimmed);
        }
    }
    return ids;
}

bool matchesCurrentUser(const QString& id)
{
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    return currentUserIdentifiers().contains(trimmed);
}

QString requestRecipientId(const QJsonObject& object)
{
    const QString direct = firstString(object, {QStringLiteral("toUserId"),
                                                QStringLiteral("toUserUuid"),
                                                QStringLiteral("recipientUserId"),
                                                QStringLiteral("recipientUserUuid"),
                                                QStringLiteral("receiverUserId"),
                                                QStringLiteral("receiverUserUuid"),
                                                QStringLiteral("targetUserId"),
                                                QStringLiteral("targetUserUuid")});
    if (!direct.isEmpty()) {
        return direct;
    }

    for (const QString& key : {QStringLiteral("toUser"),
                               QStringLiteral("recipient"),
                               QStringLiteral("receiver"),
                               QStringLiteral("targetUser")}) {
        const QJsonObject user = object.value(key).toObject();
        const QString id = firstString(user, {QStringLiteral("userId"),
                                              QStringLiteral("publicId"),
                                              QStringLiteral("id"),
                                              QStringLiteral("userUuid"),
                                              QStringLiteral("uuid")});
        if (!id.isEmpty()) {
            return id;
        }
    }
    return {};
}

QString requestSenderId(const QJsonObject& object)
{
    const QString direct = firstString(object, {QStringLiteral("fromUserId"),
                                                QStringLiteral("fromUserUuid"),
                                                QStringLiteral("actorUserId"),
                                                QStringLiteral("actorUuid"),
                                                QStringLiteral("senderUserId"),
                                                QStringLiteral("senderUserUuid")});
    if (!direct.isEmpty()) {
        return direct;
    }

    for (const QString& key : {QStringLiteral("fromUser"),
                               QStringLiteral("actor"),
                               QStringLiteral("sender"),
                               QStringLiteral("user")}) {
        const QJsonObject user = object.value(key).toObject();
        const QString id = firstString(user, {QStringLiteral("userId"),
                                              QStringLiteral("publicId"),
                                              QStringLiteral("id"),
                                              QStringLiteral("userUuid"),
                                              QStringLiteral("uuid")});
        if (!id.isEmpty()) {
            return id;
        }
    }
    return {};
}

bool shouldStoreFriendRequestEvent(const QString& type, const QJsonObject& object)
{
    const QString recipientId = requestRecipientId(object);
    if (!recipientId.isEmpty()) {
        return matchesCurrentUser(recipientId);
    }

    if (type.endsWith(QStringLiteral(".created"))) {
        const QString senderId = requestSenderId(object);
        if (!senderId.isEmpty() && matchesCurrentUser(senderId)) {
            return false;
        }
    }
    return true;
}

QJsonObject senderUserObject(const QJsonObject& object)
{
    for (const QString& key : {QStringLiteral("fromUser"),
                               QStringLiteral("actor"),
                               QStringLiteral("sender"),
                               QStringLiteral("user"),
                               QStringLiteral("profile")}) {
        const QJsonObject user = object.value(key).toObject();
        if (!user.isEmpty()) {
            return user;
        }
    }
    return {};
}

void cacheApplicantUser(const QJsonObject& object, const FriendNotification& notification)
{
    QJsonObject userObject = senderUserObject(object);
    if (userObject.isEmpty() && notification.fromUserId.isEmpty() && notification.fromUserUuid.isEmpty()) {
        return;
    }
    if (!notification.fromUserId.isEmpty() && !userObject.contains(QStringLiteral("userId"))) {
        userObject.insert(QStringLiteral("userId"), notification.fromUserId);
    }
    if (!notification.fromUserUuid.isEmpty() && !userObject.contains(QStringLiteral("userUuid"))) {
        userObject.insert(QStringLiteral("userUuid"), notification.fromUserUuid);
    }
    if (!userObject.isEmpty()) {
        UserRepository::instance().upsertUserProfile(userObject);
        return;
    }

    const QString publicId = notification.fromUserId.isEmpty()
            ? firstString(userObject, {QStringLiteral("userId"),
                                       QStringLiteral("publicId"),
                                       QStringLiteral("public_id")})
            : notification.fromUserId;
    const QString userUuid = notification.fromUserUuid.isEmpty()
            ? firstString(userObject, {QStringLiteral("userUuid"), QStringLiteral("uuid")})
            : notification.fromUserUuid;
    const QString lookupId = publicId.isEmpty() ? userUuid : publicId;
    if (lookupId.isEmpty()) {
        return;
    }

    User user = UserRepository::instance().requestUserDetail({lookupId});
    user.id = userUuid.isEmpty() ? publicId : userUuid;
    user.userUuid = userUuid;
    user.userId = publicId;
    const QString nick = firstString(userObject, {QStringLiteral("nick"),
                                                  QStringLiteral("nickName"),
                                                  QStringLiteral("displayName"),
                                                  QStringLiteral("name")});
    if (!nick.isEmpty()) {
        user.nick = nick;
    } else if (user.nick.isEmpty()) {
        user.nick = publicId.isEmpty() ? QStringLiteral("未知用户") : publicId;
    }
    const QString avatarPath = firstString(userObject, {QStringLiteral("avatarPath"),
                                                        QStringLiteral("avatarUrl")});
    if (!avatarPath.isEmpty()) {
        user.avatarPath = avatarPath;
    }
    if (userObject.contains(QStringLiteral("avatarVersion"))) {
        user.avatarVersion = userObject.value(QStringLiteral("avatarVersion")).toInt();
    }
    if (userObject.contains(QStringLiteral("avatarEtag"))) {
        user.avatarEtag = userObject.value(QStringLiteral("avatarEtag")).toString();
    }
    if (userObject.contains(QStringLiteral("avatarContentHash"))) {
        user.avatarContentHash = userObject.value(QStringLiteral("avatarContentHash")).toString();
    }
    if (user.friendGroupId.isEmpty()) {
        user.friendGroupId = QStringLiteral("default");
    }
    if (user.friendGroupName.isEmpty()) {
        user.friendGroupName = QStringLiteral("默认分组");
    }
    UserRepository::instance().saveUser(user);
}

} // namespace

FriendNotificationRepository& FriendNotificationRepository::instance()
{
    static FriendNotificationRepository repo;
    return repo;
}

FriendNotificationRepository::FriendNotificationRepository(QObject* parent)
    : QObject(parent)
{
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
                if (domain == QStringLiteral("friend_notifications")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);
    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (!type.startsWith(QStringLiteral("friend.request."))) {
                    return;
                }
                ReferenceDataResolver::instance().consumePayload(payload);

                QJsonObject object = payload.value(QStringLiteral("request")).toObject();
                if (object.isEmpty()) {
                    object = payload.value(QStringLiteral("notification")).toObject();
                }
                if (object.isEmpty()) {
                    object = payload;
                }
                if (!shouldStoreFriendRequestEvent(type, object)) {
                    return;
                }
                ReferenceDataResolver::instance().upsertUserObject(object.value(QStringLiteral("fromUser")).toObject());
                ReferenceDataResolver::instance().upsertUserObject(object.value(QStringLiteral("toUser")).toObject());
                FriendNotification notification = notificationFromJson(object);
                if (notification.id.isEmpty()) {
                    return;
                }
                cacheApplicantUser(object, notification);

                ensureLoaded();
                for (const FriendNotification& previous : m_notifications) {
                    if (previous.id != notification.id) {
                        continue;
                    }
                    if (notification.fromUserId.isEmpty()) {
                        notification.fromUserId = previous.fromUserId;
                    }
                    if (notification.fromUserUuid.isEmpty()) {
                        notification.fromUserUuid = previous.fromUserUuid;
                    }
                    if (notification.message.isEmpty()) {
                        notification.message = previous.message;
                    }
                    if (!notification.requestDate.isValid()) {
                        notification.requestDate = previous.requestDate;
                    }
                    if (notification.sourceGroupId.isEmpty()) {
                        notification.sourceGroupId = previous.sourceGroupId;
                        notification.sourceGroupName = previous.sourceGroupName;
                        notification.sourceGroupMemberName = previous.sourceGroupMemberName;
                    }
                    if (notification.sourceFriendId.isEmpty()) {
                        notification.sourceFriendId = previous.sourceFriendId;
                        notification.sourceFriendName = previous.sourceFriendName;
                    }
                    break;
                }
                if (!notification.requestDate.isValid()) {
                    notification.requestDate = QDateTime::currentDateTime();
                }
                if (type.endsWith(QStringLiteral(".accepted"))) {
                    notification.status = NotificationStatus::Accepted;
                    notification.unread = false;
                } else if (type.endsWith(QStringLiteral(".rejected"))) {
                    notification.status = NotificationStatus::Rejected;
                    notification.unread = false;
                } else if (type.endsWith(QStringLiteral(".created"))) {
                    notification.status = NotificationStatus::Pending;
                    notification.unread = true;
                }

                LocalDataStore::instance().upsertValue(QStringLiteral("friend_notifications"),
                                                       notification.id,
                                                       notificationToJson(notification));
                UnreadStateRepository::instance().setUnread(kFriendRequestUnreadScope,
                                                            notification.id,
                                                            notification.unread);
                reloadFromStore();
            });
}

void FriendNotificationRepository::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    auto* self = const_cast<FriendNotificationRepository*>(this);
    LocalDataStore& store = LocalDataStore::instance();
    for (const QJsonObject& object : store.values(QStringLiteral("friend_notifications"))) {
        const FriendNotification notification = notificationFromJson(object);
        if (!notification.id.isEmpty()) {
            cacheApplicantUser(object, notification);
            self->m_notifications.push_back(notification);
        }
    }
    for (const FriendNotification& notification : self->m_notifications) {
        if (notification.unread) {
            UnreadStateRepository::instance().setUnread(kFriendRequestUnreadScope,
                                                        notification.id,
                                                        true);
        }
    }
    self->m_loaded = true;
}

void FriendNotificationRepository::reloadFromStore()
{
    m_notifications.clear();
    m_loaded = false;
    ensureLoaded();
    emit notificationListChanged();
}

QVector<FriendNotification> FriendNotificationRepository::requestNotificationList(
    const FriendNotificationListRequest& query) const
{
    ensureLoaded();
    QVector<FriendNotification> notifications = m_notifications;
    for (FriendNotification& notification : notifications) {
        notification.unread = UnreadStateRepository::instance().isUnread(kFriendRequestUnreadScope,
                                                                         notification.id);
    }
    std::sort(notifications.begin(), notifications.end(),
              [](const FriendNotification& lhs, const FriendNotification& rhs) {
                  return lhs.requestDate > rhs.requestDate;
              });
    return NotificationListOperation(std::move(notifications), query).request(query);
}

int FriendNotificationRepository::unreadCount() const
{
    ensureLoaded();
    return UnreadStateRepository::instance().unreadCount(kFriendRequestUnreadScope);
}

int FriendNotificationRepository::notificationCount() const
{
    ensureLoaded();
    return m_notifications.size();
}

void FriendNotificationRepository::markAllRead()
{
    ensureLoaded();
    UnreadStateRepository::instance().markAllRead(kFriendRequestUnreadScope);
    emit notificationListChanged();
}

bool FriendNotificationRepository::acceptRequest(const QString& notificationId,
                                                 const QString& remark,
                                                 const QString& groupId,
                                                 const QString& groupName)
{
    ensureLoaded();
    for (FriendNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.status != NotificationStatus::Pending) {
            continue;
        }

        notification.status = NotificationStatus::Accepted;
        LocalDataStore::instance().upsertValue(QStringLiteral("friend_notifications"),
                                               notification.id,
                                               notificationToJson(notification));
        const QString applicantLookupId = notification.fromUserId.isEmpty()
                ? notification.fromUserUuid
                : notification.fromUserId;
        if (!applicantLookupId.isEmpty()) {
            User user = UserRepository::instance().requestUserDetail({applicantLookupId});
            if (user.id.isEmpty()) {
                user.id = notification.fromUserUuid.isEmpty()
                        ? notification.fromUserId
                        : notification.fromUserUuid;
                user.userUuid = notification.fromUserUuid;
                user.userId = notification.fromUserId;
                user.nick = notification.fromUserId.isEmpty()
                        ? QStringLiteral("未知用户")
                        : notification.fromUserId;
            }
            user.isFriend = true;
            user.remark = remark;
            user.friendGroupId = groupId.isEmpty() ? QStringLiteral("default") : groupId;
            user.friendGroupName = groupName.isEmpty() ? QStringLiteral("默认分组") : groupName;
            UserRepository::instance().saveUser(user);
        }

        emit notificationListChanged();
        return true;
    }
    return false;
}

bool FriendNotificationRepository::rejectRequest(const QString& notificationId)
{
    ensureLoaded();
    for (FriendNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.status != NotificationStatus::Pending) {
            continue;
        }

        notification.status = NotificationStatus::Rejected;
        LocalDataStore::instance().upsertValue(QStringLiteral("friend_notifications"),
                                               notification.id,
                                               notificationToJson(notification));
        emit notificationListChanged();
        return true;
    }
    return false;
}

bool FriendNotificationRepository::syncRequestStatus(const QString& notificationId,
                                                     NotificationStatus status)
{
    ensureLoaded();
    for (FriendNotification& notification : m_notifications) {
        if (notification.id != notificationId) {
            continue;
        }

        notification.status = status;
        notification.unread = false;
        LocalDataStore::instance().upsertValue(QStringLiteral("friend_notifications"),
                                               notification.id,
                                               notificationToJson(notification));
        UnreadStateRepository::instance().setUnread(kFriendRequestUnreadScope,
                                                    notification.id,
                                                    false);
        emit notificationListChanged();
        return true;
    }
    return false;
}

bool FriendNotificationRepository::removeRequest(const QString& notificationId)
{
    if (notificationId.isEmpty()) {
        return false;
    }

    ensureLoaded();
    const auto previousSize = m_notifications.size();
    m_notifications.erase(std::remove_if(m_notifications.begin(),
                                         m_notifications.end(),
                                         [&notificationId](const FriendNotification& notification) {
                                             return notification.id == notificationId;
                                         }),
                          m_notifications.end());
    UnreadStateRepository::instance().setUnread(kFriendRequestUnreadScope,
                                                notificationId,
                                                false);
    const bool removed = LocalDataStore::instance().removeValue(QStringLiteral("friend_notifications"),
                                                               notificationId) ||
            m_notifications.size() != previousSize;
    if (removed) {
        emit notificationListChanged();
    }
    return removed;
}
