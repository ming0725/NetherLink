#include "GroupNotificationRepository.h"

#include <algorithm>
#include <QJsonObject>
#include <QSharedPointer>
#include <QSet>
#include <QStringList>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/network/AppEventBus.h"

namespace {

const QString kGroupNotificationUnreadScope = QStringLiteral("group_notifications");

class GroupNotificationListOperation final
    : public RepositoryTemplate<GroupNotificationListRequest, QVector<GroupNotification>>
{
public:
    GroupNotificationListOperation(QVector<GroupNotification> notifications,
                                   GroupNotificationListRequest query)
        : m_notifications(std::move(notifications))
        , m_query(std::move(query))
    {
    }

private:
    QVector<GroupNotification> doRequest(const GroupNotificationListRequest&) const override
    {
        const int offset = qBound(0, m_query.offset, m_notifications.size());
        const int limit = m_query.limit < 0
                ? m_notifications.size() - offset
                : qMax(0, m_query.limit);
        return m_notifications.mid(offset, limit);
    }

    QVector<GroupNotification> m_notifications;
    GroupNotificationListRequest m_query;
};

bool canManageGroup(const Group& group)
{
    return GroupRepository::instance().isCurrentUserGroupOwner(group) ||
           GroupRepository::instance().isCurrentUserGroupAdmin(group);
}

bool isCurrentUserOwner(const Group& group)
{
    return GroupRepository::instance().isCurrentUserGroupOwner(group);
}

GroupMemberRole roleForUser(const Group& group, const QString& userId)
{
    if (group.ownerId == userId) {
        return GroupMemberRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupMemberRole::Admin;
    }
    return GroupMemberRole::Member;
}

QString groupJoinDisplayName(const QString& groupId, const QString& userId)
{
    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    const QString groupNickname = group.memberNicknames.value(userId).trimmed();
    if (!groupNickname.isEmpty()) {
        return groupNickname;
    }
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return CurrentUser::instance().getUserName();
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    if (!user.nick.trimmed().isEmpty()) {
        return user.nick.trimmed();
    }
    return userId;
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

QString groupNotificationTypeToString(GroupNotificationType type)
{
    switch (type) {
    case GroupNotificationType::MemberExited:
        return QStringLiteral("member_exited");
    case GroupNotificationType::AdminAssigned:
        return QStringLiteral("admin_assigned");
    case GroupNotificationType::OwnerTransferred:
        return QStringLiteral("owner_transferred");
    case GroupNotificationType::JoinRequest:
    default:
        return QStringLiteral("join_request");
    }
}

GroupNotificationType groupNotificationTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("member_exited")) {
        return GroupNotificationType::MemberExited;
    }
    if (normalized == QStringLiteral("admin_assigned")) {
        return GroupNotificationType::AdminAssigned;
    }
    if (normalized == QStringLiteral("owner_transferred")) {
        return GroupNotificationType::OwnerTransferred;
    }
    return GroupNotificationType::JoinRequest;
}

QString groupNotificationStatusToString(GroupNotificationStatus status)
{
    switch (status) {
    case GroupNotificationStatus::Pending:
        return QStringLiteral("pending");
    case GroupNotificationStatus::Accepted:
        return QStringLiteral("accepted");
    case GroupNotificationStatus::Rejected:
        return QStringLiteral("rejected");
    case GroupNotificationStatus::None:
    default:
        return QStringLiteral("none");
    }
}

GroupNotificationStatus groupNotificationStatusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("pending")) {
        return GroupNotificationStatus::Pending;
    }
    if (normalized == QStringLiteral("accepted")) {
        return GroupNotificationStatus::Accepted;
    }
    if (normalized == QStringLiteral("rejected")) {
        return GroupNotificationStatus::Rejected;
    }
    return GroupNotificationStatus::None;
}

QJsonObject groupNotificationToJson(const GroupNotification& notification)
{
    return {
            {QStringLiteral("id"), notification.id},
            {QStringLiteral("type"), groupNotificationTypeToString(notification.type)},
            {QStringLiteral("status"), groupNotificationStatusToString(notification.status)},
            {QStringLiteral("groupId"), notification.groupId},
            {QStringLiteral("actorUserId"), notification.actorUserId},
            {QStringLiteral("operatorUserId"), notification.operatorUserId},
            {QStringLiteral("message"), notification.message},
            {QStringLiteral("createdAt"), notification.createdAt.toString(Qt::ISODateWithMs)},
            {QStringLiteral("unread"), notification.unread}
    };
}

GroupNotification groupNotificationFromJson(const QJsonObject& object)
{
    GroupNotification notification;
    notification.id = firstString(object, {QStringLiteral("id"),
                                           QStringLiteral("requestId"),
                                           QStringLiteral("notificationId"),
                                           QStringLiteral("sourceId")});
    notification.type = groupNotificationTypeFromString(object.value(QStringLiteral("type")).toString());
    notification.status = groupNotificationStatusFromString(object.value(QStringLiteral("status")).toString());
    notification.groupId = object.value(QStringLiteral("groupId")).toString();
    notification.actorUserId = firstString(object, {QStringLiteral("actorUserId"),
                                                    QStringLiteral("actorUuid"),
                                                    QStringLiteral("fromUserId"),
                                                    QStringLiteral("fromUserUuid")});
    notification.operatorUserId = firstString(object, {QStringLiteral("operatorUserId"),
                                                       QStringLiteral("operatorUuid")});
    notification.message = object.value(QStringLiteral("message")).toString();
    notification.createdAt = QDateTime::fromString(firstString(object, {QStringLiteral("createdAt"),
                                                                        QStringLiteral("updatedAt")}),
                                                   Qt::ISODateWithMs);
    notification.unread = object.value(QStringLiteral("unread")).toBool(object.value(QStringLiteral("readAt")).isNull());
    notification.actorRole = roleForUser(GroupRepository::instance().requestGroupDetail({notification.groupId}),
                                         notification.actorUserId);
    return notification;
}

} // namespace

GroupNotificationRepository& GroupNotificationRepository::instance()
{
    static GroupNotificationRepository repo;
    return repo;
}

GroupNotificationRepository::GroupNotificationRepository(QObject* parent)
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
                if (domain == QStringLiteral("group_notifications")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);
    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (type != QStringLiteral("group.notification.created") &&
                    !type.startsWith(QStringLiteral("group.join_request."))) {
                    return;
                }

                QJsonObject object = payload.value(QStringLiteral("notification")).toObject();
                if (object.isEmpty()) {
                    object = payload.value(QStringLiteral("request")).toObject();
                }
                if (object.isEmpty()) {
                    object = payload;
                }
                GroupNotification notification = groupNotificationFromJson(object);
                if (notification.id.isEmpty()) {
                    return;
                }

                ensureLoaded();
                for (const GroupNotification& previous : m_notifications) {
                    if (previous.id != notification.id) {
                        continue;
                    }
                    if (notification.groupId.isEmpty()) {
                        notification.groupId = previous.groupId;
                    }
                    if (notification.actorUserId.isEmpty()) {
                        notification.actorUserId = previous.actorUserId;
                    }
                    if (notification.operatorUserId.isEmpty()) {
                        notification.operatorUserId = previous.operatorUserId;
                    }
                    if (notification.message.isEmpty()) {
                        notification.message = previous.message;
                    }
                    if (!notification.createdAt.isValid()) {
                        notification.createdAt = previous.createdAt;
                    }
                    notification.actorRole = previous.actorRole;
                    break;
                }
                if (!notification.createdAt.isValid()) {
                    notification.createdAt = QDateTime::currentDateTime();
                }
                if (type.endsWith(QStringLiteral(".accepted"))) {
                    notification.status = GroupNotificationStatus::Accepted;
                    notification.unread = false;
                } else if (type.endsWith(QStringLiteral(".rejected"))) {
                    notification.status = GroupNotificationStatus::Rejected;
                    notification.unread = false;
                } else if (type == QStringLiteral("group.notification.created") ||
                           type.endsWith(QStringLiteral(".created"))) {
                    if (notification.status == GroupNotificationStatus::None) {
                        notification.status = GroupNotificationStatus::Pending;
                    }
                    notification.unread = true;
                }

                LocalDataStore::instance().upsertValue(QStringLiteral("group_notifications"),
                                                       notification.id,
                                                       groupNotificationToJson(notification));
                UnreadStateRepository::instance().setUnread(kGroupNotificationUnreadScope,
                                                            notification.id,
                                                            notification.unread);
                reloadFromStore();
            });
}

void GroupNotificationRepository::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    auto* self = const_cast<GroupNotificationRepository*>(this);
    LocalDataStore& store = LocalDataStore::instance();
    for (const QJsonObject& object : store.values(QStringLiteral("group_notifications"))) {
        const GroupNotification notification = groupNotificationFromJson(object);
        if (!notification.id.isEmpty()) {
            self->m_notifications.push_back(notification);
        }
    }
    for (const GroupNotification& notification : self->m_notifications) {
        if (notification.unread) {
            UnreadStateRepository::instance().setUnread(kGroupNotificationUnreadScope,
                                                        notification.id,
                                                        true);
        }
    }
    self->m_loaded = true;
}

void GroupNotificationRepository::reloadFromStore()
{
    m_notifications.clear();
    m_loaded = false;
    ensureLoaded();
    emit notificationListChanged();
}

QVector<GroupNotification> GroupNotificationRepository::requestNotificationList(
    const GroupNotificationListRequest& query) const
{
    ensureLoaded();
    QVector<GroupNotification> notifications = m_notifications;
    for (GroupNotification& notification : notifications) {
        notification.unread = UnreadStateRepository::instance().isUnread(kGroupNotificationUnreadScope,
                                                                         notification.id);
    }
    std::sort(notifications.begin(), notifications.end(),
              [](const GroupNotification& lhs, const GroupNotification& rhs) {
                  return lhs.createdAt > rhs.createdAt;
              });
    return GroupNotificationListOperation(std::move(notifications), query).request(query);
}

int GroupNotificationRepository::unreadCount() const
{
    return m_loaded
            ? UnreadStateRepository::instance().unreadCount(kGroupNotificationUnreadScope)
            : 0;
}

int GroupNotificationRepository::notificationCount() const
{
    ensureLoaded();
    return m_notifications.size();
}

void GroupNotificationRepository::markAllRead()
{
    ensureLoaded();
    UnreadStateRepository::instance().markAllRead(kGroupNotificationUnreadScope);
    emit notificationListChanged();
}

bool GroupNotificationRepository::acceptJoinRequest(const QString& notificationId,
                                                    const QString& remark,
                                                    const QString& categoryId,
                                                    const QString& categoryName)
{
    ensureLoaded();
    for (GroupNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.type != GroupNotificationType::JoinRequest ||
            notification.status != GroupNotificationStatus::Pending) {
            continue;
        }

        const QDateTime acceptedAt = QDateTime::currentDateTime();
        notification.status = GroupNotificationStatus::Accepted;
        notification.operatorUserId = CurrentUser::instance().getUserId();
        LocalDataStore::instance().upsertValue(QStringLiteral("group_notifications"),
                                               notification.id,
                                               groupNotificationToJson(notification));
        GroupRepository::instance().addMember(notification.groupId, notification.actorUserId);
        Group group = GroupRepository::instance().requestGroupDetail({notification.groupId});
        if (!group.groupId.isEmpty()) {
            group.remark = remark;
            group.listGroupId = categoryId.isEmpty() ? QStringLiteral("gg_joined") : categoryId;
            group.listGroupName = categoryName.isEmpty() ? QStringLiteral("我加入的群聊") : categoryName;
            GroupRepository::instance().saveGroup(group);
        }

        auto message = QSharedPointer<ChatMessage>(new GroupMemberJoinedMessage(
            notification.actorUserId,
            groupJoinDisplayName(notification.groupId, notification.actorUserId)));
        message->setTimestamp(acceptedAt);
        MessageRepository::instance().addMessage(notification.groupId, message);

        emit notificationListChanged();
        return true;
    }
    return false;
}

bool GroupNotificationRepository::rejectJoinRequest(const QString& notificationId)
{
    ensureLoaded();
    for (GroupNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.type != GroupNotificationType::JoinRequest ||
            notification.status != GroupNotificationStatus::Pending) {
            continue;
        }

        notification.status = GroupNotificationStatus::Rejected;
        notification.operatorUserId = CurrentUser::instance().getUserId();
        LocalDataStore::instance().upsertValue(QStringLiteral("group_notifications"),
                                               notification.id,
                                               groupNotificationToJson(notification));
        emit notificationListChanged();
        return true;
    }
    return false;
}
