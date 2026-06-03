#include "FriendNotificationRepository.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonObject>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/types/ChatMessage.h"

namespace {

const QString kFriendRequestUnreadScope = QStringLiteral("friend_requests");
constexpr int kInitialUnreadCount = 36;

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

QString userDisplayName(const QString& userId)
{
    const User user = UserRepository::instance().requestUserDetail({userId});
    return user.nick.isEmpty() ? userId : user.nick;
}

QString groupDisplayName(const Group& group)
{
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

FriendNotification makeNotification(const QString& id,
                                    const QString& fromUserId,
                                    const QString& message,
                                    int daysAgo,
                                    NotificationSourceType sourceType,
                                    const QString& groupId = {},
                                    const QString& sourceFriendName = {})
{
    FriendNotification notification;
    notification.id = id;
    notification.fromUserId = fromUserId;
    notification.message = message;
    notification.requestDate = QDateTime::currentDateTime().addDays(-daysAgo);
    notification.sourceType = sourceType;

    if (sourceType == NotificationSourceType::GroupChat) {
        const Group group = GroupRepository::instance().requestGroupDetail({groupId});
        notification.sourceGroupId = group.groupId;
        notification.sourceGroupName = groupDisplayName(group);
        notification.sourceGroupMemberName = group.memberNicknames.value(fromUserId, userDisplayName(fromUserId));
    } else if (sourceType == NotificationSourceType::FriendShare) {
        notification.sourceFriendName = sourceFriendName;
    }

    return notification;
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

QJsonObject notificationToJson(const FriendNotification& notification)
{
    return {
            {QStringLiteral("id"), notification.id},
            {QStringLiteral("fromUserId"), notification.fromUserId},
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
    notification.id = object.value(QStringLiteral("id")).toString();
    notification.fromUserId = object.value(QStringLiteral("fromUserId")).toString();
    notification.message = object.value(QStringLiteral("message")).toString();
    notification.requestDate = QDateTime::fromString(object.value(QStringLiteral("requestDate")).toString(),
                                                     Qt::ISODateWithMs);
    notification.sourceType = notificationSourceTypeFromString(object.value(QStringLiteral("sourceType")).toString());
    notification.unread = object.value(QStringLiteral("unread")).toBool(true);
    notification.sourceGroupId = object.value(QStringLiteral("sourceGroupId")).toString();
    notification.sourceGroupName = object.value(QStringLiteral("sourceGroupName")).toString();
    notification.sourceGroupMemberName = object.value(QStringLiteral("sourceGroupMemberName")).toString();
    notification.sourceFriendId = object.value(QStringLiteral("sourceFriendId")).toString();
    notification.sourceFriendName = object.value(QStringLiteral("sourceFriendName")).toString();
    notification.status = notificationStatusFromString(object.value(QStringLiteral("status")).toString());
    return notification;
}

QStringList stringsFromJsonArray(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array) {
        values.append(value.toString());
    }
    return values;
}

QVector<FriendNotification> buildInitialNotifications()
{
    const QJsonObject seed = LocalDataStore::instance()
            .seedObject(QStringLiteral(":/resources/data/friend_notifications.json"));
    const QStringList userIds = stringsFromJsonArray(seed.value(QStringLiteral("userIds")).toArray());
    const QStringList groupIds = stringsFromJsonArray(seed.value(QStringLiteral("groupIds")).toArray());
    const QStringList shareNames = stringsFromJsonArray(seed.value(QStringLiteral("shareNames")).toArray());
    const QStringList messages = stringsFromJsonArray(seed.value(QStringLiteral("messages")).toArray());

    QVector<FriendNotification> notifications;
    notifications.reserve(userIds.size());
    for (int i = 0; i < userIds.size() && !groupIds.isEmpty() && !shareNames.isEmpty() && !messages.isEmpty(); ++i) {
        const NotificationSourceType sourceType =
                i % 3 == 0 ? NotificationSourceType::GroupChat
              : i % 3 == 1 ? NotificationSourceType::FriendShare
                            : NotificationSourceType::IdSearch;
        notifications.push_back(makeNotification(QStringLiteral("fn_%1").arg(i + 1, 3, 10, QChar('0')),
                                                 userIds.at(i),
                                                 messages.at(i % messages.size()),
                                                 i % 18,
                                                 sourceType,
                                                 groupIds.at(i % groupIds.size()),
                                                 shareNames.at(i % shareNames.size())));
    }
    return notifications;
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
}

void FriendNotificationRepository::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    auto* self = const_cast<FriendNotificationRepository*>(this);
    LocalDataStore& store = LocalDataStore::instance();
    if (!store.hasDomain(QStringLiteral("friend_notifications"))) {
        for (const FriendNotification& notification : buildInitialNotifications()) {
            store.upsertValue(QStringLiteral("friend_notifications"),
                              notification.id,
                              notificationToJson(notification));
        }
    }
    for (const QJsonObject& object : store.values(QStringLiteral("friend_notifications"))) {
        const FriendNotification notification = notificationFromJson(object);
        if (!notification.id.isEmpty()) {
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
    return m_loaded
            ? UnreadStateRepository::instance().unreadCount(kFriendRequestUnreadScope)
            : kInitialUnreadCount;
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

        const QDateTime acceptedAt = QDateTime::currentDateTime();
        notification.status = NotificationStatus::Accepted;
        LocalDataStore::instance().upsertValue(QStringLiteral("friend_notifications"),
                                               notification.id,
                                               notificationToJson(notification));
        User user = UserRepository::instance().requestUserDetail({notification.fromUserId});
        if (!user.id.isEmpty()) {
            user.isFriend = true;
            user.remark = remark;
            user.friendGroupId = groupId.isEmpty() ? QStringLiteral("default") : groupId;
            user.friendGroupName = groupName.isEmpty() ? QStringLiteral("默认分组") : groupName;
            UserRepository::instance().saveUser(user);
        }

        auto message = QSharedPointer<ChatMessage>(new TextMessage(
            QStringLiteral("我们已经是好友了，现在可以开始聊天了"),
            true,
            CurrentUser::instance().getUserId(),
            false,
            CurrentUser::instance().getUserName()));
        message->setTimestamp(acceptedAt);
        MessageRepository::instance().addMessage(notification.fromUserId, message);
        MessageRepository::instance().markConversationRead(notification.fromUserId);

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
