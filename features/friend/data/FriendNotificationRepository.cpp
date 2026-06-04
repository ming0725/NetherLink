#include "FriendNotificationRepository.h"

#include <algorithm>
#include <QJsonObject>
#include <QStringList>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/network/AppEventBus.h"
#include "shared/types/ChatMessage.h"

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
    notification.id = firstString(object, {QStringLiteral("id"),
                                           QStringLiteral("requestId"),
                                           QStringLiteral("notificationId"),
                                           QStringLiteral("sourceId")});
    notification.fromUserId = firstString(object, {QStringLiteral("fromUserId"),
                                                   QStringLiteral("fromUserUuid"),
                                                   QStringLiteral("actorUserId"),
                                                   QStringLiteral("actorUuid")});
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

                QJsonObject object = payload.value(QStringLiteral("request")).toObject();
                if (object.isEmpty()) {
                    object = payload.value(QStringLiteral("notification")).toObject();
                }
                if (object.isEmpty()) {
                    object = payload;
                }
                FriendNotification notification = notificationFromJson(object);
                if (notification.id.isEmpty()) {
                    return;
                }

                ensureLoaded();
                for (const FriendNotification& previous : m_notifications) {
                    if (previous.id != notification.id) {
                        continue;
                    }
                    if (notification.fromUserId.isEmpty()) {
                        notification.fromUserId = previous.fromUserId;
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
    return m_loaded
            ? UnreadStateRepository::instance().unreadCount(kFriendRequestUnreadScope)
            : 0;
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
