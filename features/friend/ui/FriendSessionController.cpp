#include "features/friend/ui/FriendSessionController.h"

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/GroupRemoteDataSource.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/data/FriendRemoteDataSource.h"
#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"
#include "shared/network/NotificationRemoteDataSource.h"

#include <QUuid>

namespace {

QString currentUserUuid()
{
    const CurrentUserProfile profile = CurrentUser::instance().identity();
    return profile.userUuid.isEmpty() ? CurrentUser::instance().getUserId() : profile.userUuid;
}

} // namespace

FriendSessionController::FriendSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendRequestAccepted,
            this,
            [this](const QString&,
                   const QString& notificationId,
                   const QString& remark,
                   const QString& groupId,
                   const QString& groupName) {
                ensureFriendNotificationRepositoryConnections();
                FriendNotificationRepository::instance().acceptRequest(notificationId,
                                                                       remark,
                                                                       groupId,
                                                                       groupName);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendRequestRejected,
            this,
            [this](const QString&, const QString& notificationId) {
                ensureFriendNotificationRepositoryConnections();
                FriendNotificationRepository::instance().rejectRequest(notificationId);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::groupJoinRequestAccepted,
            this,
            [this](const QString&,
                   const QString& notificationId,
                   const QString& remark,
                   const QString& categoryId,
                   const QString& categoryName) {
                ensureGroupNotificationRepositoryConnections();
                GroupNotificationRepository::instance().acceptJoinRequest(notificationId,
                                                                          remark,
                                                                          categoryId,
                                                                          categoryName);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::groupJoinRequestRejected,
            this,
            [this](const QString&, const QString& notificationId) {
                ensureGroupNotificationRepositoryConnections();
                GroupNotificationRepository::instance().rejectJoinRequest(notificationId);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendUpdated,
            this,
            [this](const QString&, const User& user) {
                if (user.id.isEmpty()) {
                    return;
                }
                ensureUserRepositoryConnections();
                UserRepository::instance().saveUser(user);
            },
            Qt::QueuedConnection);
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendDeleted,
            this,
            [this](const QString&, const QString& userId) {
                if (userId.isEmpty()) {
                    return;
                }
                ensureUserRepositoryConnections();
                UserRepository::instance().removeUser(userId);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendGroupCreated,
            this,
            [this](const QString&, const QJsonObject& group) {
                ensureUserRepositoryConnections();
                UserRepository::instance().upsertFriendGroup(group);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendGroupUpdated,
            this,
            [this](const QString&, const QJsonObject& group) {
                ensureUserRepositoryConnections();
                UserRepository::instance().upsertFriendGroup(group);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendGroupDeleted,
            this,
            [this](const QString&, const QString& friendGroupId) {
                ensureUserRepositoryConnections();
                UserRepository::instance().removeFriendGroup(friendGroupId);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendRequestActionFailed,
            this,
            [this](const QString&, const QString& notificationId, const NetworkError& error) {
                emit friendRequestActionFailed(notificationId, error);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::groupJoinRequestActionFailed,
            this,
            [this](const QString&, const QString& notificationId, const NetworkError& error) {
                emit groupJoinRequestActionFailed(notificationId, error);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendUpdateFailed,
            this,
            [this](const QString&, const QString& userId, const NetworkError& error) {
                emit friendUpdateFailed(userId, error);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendDeleteFailed,
            this,
            [this](const QString&, const QString& userId, const NetworkError& error) {
                emit friendDeleteFailed(userId, error);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendGroupActionFailed,
            this,
            [this](const QString&, const QString& friendGroupId, const NetworkError& error) {
                emit friendGroupActionFailed(friendGroupId, error);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupUpdated,
            this,
            [this](const QString&, const Group& group) {
                if (group.groupId.isEmpty()) {
                    return;
                }
                ensureGroupRepositoryConnections();
                GroupRepository::instance().saveGroup(group);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMySettingsUpdated,
            this,
            [this](const QString&, const Group& group) {
                if (group.groupId.isEmpty()) {
                    return;
                }
                ensureGroupRepositoryConnections();
                GroupRepository::instance().saveGroup(group);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupLeft,
            this,
            [this](const QString&, const QString& groupId) {
                if (groupId.isEmpty()) {
                    return;
                }
                ensureGroupRepositoryConnections();
                MessageRepository::instance().removeConversation(groupId);
                GroupRepository::instance().removeGroup(groupId);
                emit groupLeaveSucceeded(groupId);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupUpdateFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                emit groupUpdateFailed(groupId, error);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMySettingsUpdateFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                emit groupMySettingsUpdateFailed(groupId, error);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupLeaveFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                emit groupLeaveFailed(groupId, error);
            });
    connect(&NotificationRemoteDataSource::instance(),
            &NotificationRemoteDataSource::markAllReadSucceeded,
            this,
            [this](const QString& requestId, const QString&) {
                if (requestId == m_pendingFriendNotificationsReadRequestId) {
                    m_pendingFriendNotificationsReadRequestId.clear();
                    ensureFriendNotificationRepositoryConnections();
                    FriendNotificationRepository::instance().markAllRead();
                    return;
                }

                if (m_pendingGroupNotificationsReadRequestIds.remove(requestId) &&
                    m_pendingGroupNotificationsReadRequestIds.isEmpty()) {
                    ensureGroupNotificationRepositoryConnections();
                    GroupNotificationRepository::instance().markAllRead();
                }
            });
    connect(&NotificationRemoteDataSource::instance(),
            &NotificationRemoteDataSource::markAllReadFailed,
            this,
            [this](const QString& requestId, const QString&, const NetworkError& error) {
                if (requestId == m_pendingFriendNotificationsReadRequestId) {
                    m_pendingFriendNotificationsReadRequestId.clear();
                    emit notificationMarkReadFailed(QStringLiteral("friend_requests"), error);
                    return;
                }

                if (m_pendingGroupNotificationsReadRequestIds.remove(requestId)) {
                    m_pendingGroupNotificationsReadRequestIds.clear();
                    emit notificationMarkReadFailed(QStringLiteral("group_notifications"), error);
                }
            });
}

void FriendSessionController::ensureUserRepositoryConnections() const
{
    if (m_userRepositoryConnectionsReady) {
        return;
    }

    auto* self = const_cast<FriendSessionController*>(this);
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            self, &FriendSessionController::friendListChanged);
    connect(&UserRepository::instance(), &UserRepository::userAvatarImageReady,
            self, &FriendSessionController::userAvatarImageReady);
    connect(&UserRepository::instance(), &UserRepository::userAvatarImageFailed,
            self, &FriendSessionController::userAvatarImageFailed);
    m_userRepositoryConnectionsReady = true;
}

void FriendSessionController::ensureGroupRepositoryConnections() const
{
    if (m_groupRepositoryConnectionsReady) {
        return;
    }

    auto* self = const_cast<FriendSessionController*>(this);
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            self, &FriendSessionController::groupListChanged);
    connect(&GroupRepository::instance(), &GroupRepository::groupAvatarImageReady,
            self, &FriendSessionController::groupAvatarImageReady);
    connect(&GroupRepository::instance(), &GroupRepository::groupAvatarImageFailed,
            self, &FriendSessionController::groupAvatarImageFailed);
    m_groupRepositoryConnectionsReady = true;
}

void FriendSessionController::ensureFriendNotificationRepositoryConnections() const
{
    if (m_friendNotificationRepositoryConnectionsReady) {
        return;
    }

    auto* self = const_cast<FriendSessionController*>(this);
    connect(&FriendNotificationRepository::instance(), &FriendNotificationRepository::notificationListChanged,
            self, &FriendSessionController::friendNotificationListChanged);
    m_friendNotificationRepositoryConnectionsReady = true;
}

void FriendSessionController::ensureGroupNotificationRepositoryConnections() const
{
    if (m_groupNotificationRepositoryConnectionsReady) {
        return;
    }

    auto* self = const_cast<FriendSessionController*>(this);
    connect(&GroupNotificationRepository::instance(), &GroupNotificationRepository::notificationListChanged,
            self, &FriendSessionController::groupNotificationListChanged);
    m_groupNotificationRepositoryConnectionsReady = true;
}

QVector<FriendGroupSummary> FriendSessionController::loadFriendGroupSummaries(const QString& keyword) const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().requestFriendGroupSummaries({keyword});
}

QVector<FriendSummary> FriendSessionController::loadFriendsInGroup(const QString& groupId,
                                                                   const QString& keyword,
                                                                   int offset,
                                                                   int limit) const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().requestFriendsInGroup({groupId, keyword, offset, limit});
}

User FriendSessionController::loadFriend(const QString& userId) const
{
    ensureUserRepositoryConnections();
    if (userId.isEmpty()) {
        return {};
    }
    return UserRepository::instance().requestUserDetail({userId});
}

QString FriendSessionController::userNickname(const QString& userId) const
{
    const User user = loadFriend(userId);
    if (user.id.isEmpty()) {
        return userId;
    }
    if (!user.nick.isEmpty()) {
        return user.nick;
    }
    return user.userId.isEmpty() ? user.id : user.userId;
}

QString FriendSessionController::userAvatarPath(const QString& userId) const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().requestUserAvatarPath(userId);
}

QString FriendSessionController::requestUserAvatarImage(const QString& userId)
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().requestUserAvatarImageAsync(userId);
}

QMap<QString, QString> FriendSessionController::loadFriendGroups() const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().requestFriendGroups();
}

QString FriendSessionController::refreshFriendProfile(const QString& userId) const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().refreshUserProfile(userId);
}

QString FriendSessionController::refreshFriendPresenceSnapshot() const
{
    ensureUserRepositoryConnections();
    return UserRepository::instance().refreshFriendPresenceSnapshot();
}

bool FriendSessionController::saveFriend(const User& user)
{
    if (user.id.isEmpty()) {
        return false;
    }
    ensureUserRepositoryConnections();
    return !FriendRemoteDataSource::instance().updateFriend(user).isEmpty();
}

bool FriendSessionController::changeFriendGroup(const QString& userId,
                                                const QString& groupId,
                                                const QString& groupName)
{
    if (userId.isEmpty() || groupId.isEmpty()) {
        return false;
    }

    ensureUserRepositoryConnections();
    User user = UserRepository::instance().requestUserDetail({userId});
    if (user.id.isEmpty() || user.friendGroupId == groupId) {
        return false;
    }

    user.friendGroupId = groupId;
    user.friendGroupName = groupName;
    return !FriendRemoteDataSource::instance().updateFriend(user).isEmpty();
}

bool FriendSessionController::deleteFriend(const QString& userId)
{
    if (userId.isEmpty()) {
        return false;
    }

    ensureUserRepositoryConnections();
    return !FriendRemoteDataSource::instance().deleteFriend(userId).isEmpty();
}

bool FriendSessionController::createFriendGroup(const QString& name)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return false;
    }

    ensureUserRepositoryConnections();
    return !FriendRemoteDataSource::instance()
                    .createFriendGroup(trimmedName, UserRepository::instance().nextFriendGroupSortOrder())
                    .isEmpty();
}

bool FriendSessionController::renameFriendGroup(const QString& friendGroupId, const QString& name)
{
    const QString trimmedName = name.trimmed();
    if (friendGroupId.isEmpty() || friendGroupId == QStringLiteral("default") || trimmedName.isEmpty()) {
        return false;
    }

    ensureUserRepositoryConnections();
    return !FriendRemoteDataSource::instance()
                    .updateFriendGroup(friendGroupId, trimmedName)
                    .isEmpty();
}

bool FriendSessionController::deleteFriendGroup(const QString& friendGroupId)
{
    if (friendGroupId.isEmpty() || friendGroupId == QStringLiteral("default")) {
        return false;
    }

    ensureUserRepositoryConnections();
    return !FriendRemoteDataSource::instance().deleteFriendGroup(friendGroupId).isEmpty();
}

QVector<GroupCategorySummary> FriendSessionController::loadGroupCategorySummaries(const QString& keyword) const
{
    ensureGroupRepositoryConnections();
    return GroupRepository::instance().requestGroupCategorySummaries({keyword});
}

QVector<Group> FriendSessionController::loadGroupsInCategory(const QString& categoryId,
                                                            const QString& keyword,
                                                            int offset,
                                                            int limit) const
{
    ensureGroupRepositoryConnections();
    return GroupRepository::instance().requestGroupsInCategory({categoryId, keyword, offset, limit});
}

Group FriendSessionController::loadGroup(const QString& groupId) const
{
    ensureGroupRepositoryConnections();
    if (groupId.isEmpty()) {
        return {};
    }
    return GroupRepository::instance().requestGroupDetail({groupId});
}

QString FriendSessionController::groupDisplayName(const QString& groupId) const
{
    const Group group = loadGroup(groupId);
    if (group.groupId.isEmpty()) {
        return QStringLiteral("群聊");
    }
    if (!group.remark.isEmpty()) {
        return group.remark;
    }
    if (!group.groupName.isEmpty()) {
        return group.groupName;
    }
    return group.groupPublicId.isEmpty() ? QStringLiteral("群聊") : group.groupPublicId;
}

QString FriendSessionController::groupNicknameFor(const QString& groupId, const QString& userId) const
{
    const QString cachedNickname = GroupRepository::instance().requestGroupMemberNickname(groupId, userId).trimmed();
    if (!cachedNickname.isEmpty()) {
        return cachedNickname;
    }
    const Group group = loadGroup(groupId);
    const QString groupNickname = group.memberNicknames.value(userId);
    return groupNickname.isEmpty() ? userNickname(userId) : groupNickname;
}

QString FriendSessionController::groupManagerRoleText(const QString& groupId, const QString& userId) const
{
    const Group group = loadGroup(groupId);
    return group.ownerId == userId ? QStringLiteral("群主") : QStringLiteral("管理员");
}

QString FriendSessionController::groupAvatarPath(const QString& groupId) const
{
    ensureGroupRepositoryConnections();
    return GroupRepository::instance().requestGroupAvatarPath(groupId);
}

QString FriendSessionController::requestGroupAvatarImage(const QString& groupId)
{
    ensureGroupRepositoryConnections();
    return GroupRepository::instance().requestGroupAvatarImageAsync(groupId);
}

QMap<QString, QString> FriendSessionController::loadGroupCategories() const
{
    ensureGroupRepositoryConnections();
    return GroupRepository::instance().requestGroupCategories();
}

bool FriendSessionController::canExitGroup(const Group& group) const
{
    ensureGroupRepositoryConnections();
    return !group.groupId.isEmpty() && !GroupRepository::instance().isCurrentUserGroupOwner(group);
}

bool FriendSessionController::saveGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return false;
    }
    ensureGroupRepositoryConnections();
    const Group previous = GroupRepository::instance().requestGroupDetail({group.groupId});
    if (previous.groupId.isEmpty()) {
        return false;
    }

    const bool groupInfoChanged = group.groupName != previous.groupName ||
                                  group.introduction != previous.introduction ||
                                  group.announcement != previous.announcement ||
                                  group.groupAvatarPath != previous.groupAvatarPath;
    const bool mySettingsChanged = group.remark != previous.remark ||
                                   group.listGroupId != previous.listGroupId ||
                                   group.listGroupName != previous.listGroupName ||
                                   group.isDnd != previous.isDnd;
    if (!groupInfoChanged && !mySettingsChanged) {
        return false;
    }

    bool sent = false;
    if (groupInfoChanged) {
        sent = !GroupRemoteDataSource::instance().updateGroup(group).isEmpty() || sent;
    }
    if (mySettingsChanged) {
        sent = !GroupRemoteDataSource::instance().updateMySettings(group).isEmpty() || sent;
    }
    return sent;
}

bool FriendSessionController::createGroupCategory(const QString& categoryName)
{
    const QString normalizedName = categoryName.trimmed();
    if (normalizedName.isEmpty()) {
        return false;
    }

    ensureGroupRepositoryConnections();
    const QString categoryId = QStringLiteral("gc_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return GroupRepository::instance().upsertGroupCategory(categoryId, normalizedName);
}

bool FriendSessionController::renameGroupCategory(const QString& categoryId, const QString& categoryName)
{
    const QString normalizedName = categoryName.trimmed();
    if (categoryId.isEmpty() ||
        categoryId == QStringLiteral("gg_joined") ||
        categoryId == QStringLiteral("gg_created") ||
        categoryId == QStringLiteral("gg_managed") ||
        normalizedName.isEmpty()) {
        return false;
    }

    ensureGroupRepositoryConnections();
    bool sent = GroupRepository::instance().upsertGroupCategory(categoryId, normalizedName);
    const QVector<Group> groups = GroupRepository::instance().requestGroupsInCategory({categoryId, {}, 0, -1});
    for (Group group : groups) {
        group.listGroupId = categoryId;
        group.listGroupName = normalizedName;
        sent = !GroupRemoteDataSource::instance().updateMySettings(group).isEmpty() || sent;
    }
    return sent;
}

bool FriendSessionController::deleteGroupCategory(const QString& categoryId)
{
    if (categoryId.isEmpty() ||
        categoryId == QStringLiteral("gg_joined") ||
        categoryId == QStringLiteral("gg_created") ||
        categoryId == QStringLiteral("gg_managed")) {
        return false;
    }

    ensureGroupRepositoryConnections();
    bool sent = GroupRepository::instance().removeGroupCategory(categoryId);
    const QVector<Group> groups = GroupRepository::instance().requestGroupsInCategory({categoryId, {}, 0, -1});
    for (Group group : groups) {
        group.listGroupId.clear();
        group.listGroupName.clear();
        sent = !GroupRemoteDataSource::instance().updateMySettings(group).isEmpty() || sent;
    }
    return sent;
}

bool FriendSessionController::changeGroupCategory(const QString& groupId,
                                                  const QString& categoryId,
                                                  const QString& categoryName)
{
    if (groupId.isEmpty() || categoryId.isEmpty()) {
        return false;
    }

    ensureGroupRepositoryConnections();
    Group group = GroupRepository::instance().requestGroupDetail({groupId});
    const QString currentCategoryId = group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : group.listGroupId;
    if (group.groupId.isEmpty() || categoryId == currentCategoryId) {
        return false;
    }

    if (categoryId == QStringLiteral("gg_joined")) {
        group.listGroupId.clear();
        group.listGroupName.clear();
    } else {
        group.listGroupId = categoryId;
        group.listGroupName = categoryName;
    }
    return !GroupRemoteDataSource::instance().updateMySettings(group).isEmpty();
}

bool FriendSessionController::exitGroup(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return false;
    }

    ensureGroupRepositoryConnections();
    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    if (!canExitGroup(group)) {
        return false;
    }

    return !GroupRemoteDataSource::instance().leaveGroup(groupId, currentUserUuid()).isEmpty();
}

QVector<FriendNotification> FriendSessionController::loadFriendNotifications(int offset, int limit) const
{
    ensureFriendNotificationRepositoryConnections();
    return FriendNotificationRepository::instance().requestNotificationList({offset, limit});
}

int FriendSessionController::friendNotificationCount() const
{
    ensureFriendNotificationRepositoryConnections();
    return FriendNotificationRepository::instance().notificationCount();
}

int FriendSessionController::friendUnreadCount() const
{
    ensureFriendNotificationRepositoryConnections();
    return FriendNotificationRepository::instance().unreadCount();
}

void FriendSessionController::markFriendNotificationsRead()
{
    ensureFriendNotificationRepositoryConnections();
    if (FriendNotificationRepository::instance().unreadCount() <= 0 ||
        !m_pendingFriendNotificationsReadRequestId.isEmpty()) {
        return;
    }
    m_pendingFriendNotificationsReadRequestId =
            NotificationRemoteDataSource::instance().markAllRead(QStringLiteral("friend.request.created"));
}

bool FriendSessionController::acceptFriendRequest(const QString& notificationId,
                                                  const QString& remark,
                                                  const QString& groupId,
                                                  const QString& groupName)
{
    ensureFriendNotificationRepositoryConnections();
    return !FriendRemoteDataSource::instance().acceptFriendRequest(notificationId,
                                                                  remark,
                                                                  groupId,
                                                                  groupName).isEmpty();
}

bool FriendSessionController::rejectFriendRequest(const QString& notificationId)
{
    ensureFriendNotificationRepositoryConnections();
    return !FriendRemoteDataSource::instance().rejectFriendRequest(notificationId).isEmpty();
}

QVector<GroupNotification> FriendSessionController::loadGroupNotifications(int offset, int limit) const
{
    ensureGroupNotificationRepositoryConnections();
    return GroupNotificationRepository::instance().requestNotificationList({offset, limit});
}

int FriendSessionController::groupNotificationCount() const
{
    ensureGroupNotificationRepositoryConnections();
    return GroupNotificationRepository::instance().notificationCount();
}

int FriendSessionController::groupUnreadCount() const
{
    ensureGroupNotificationRepositoryConnections();
    return GroupNotificationRepository::instance().unreadCount();
}

void FriendSessionController::markGroupNotificationsRead()
{
    ensureGroupNotificationRepositoryConnections();
    if (GroupNotificationRepository::instance().unreadCount() <= 0 ||
        !m_pendingGroupNotificationsReadRequestIds.isEmpty()) {
        return;
    }

    const QString notificationRequestId =
            NotificationRemoteDataSource::instance().markAllRead(QStringLiteral("group.notification.created"));
    const QString joinRequestId =
            NotificationRemoteDataSource::instance().markAllRead(QStringLiteral("group.join_request.created"));
    if (!notificationRequestId.isEmpty()) {
        m_pendingGroupNotificationsReadRequestIds.insert(notificationRequestId);
    }
    if (!joinRequestId.isEmpty()) {
        m_pendingGroupNotificationsReadRequestIds.insert(joinRequestId);
    }
}

bool FriendSessionController::acceptGroupJoinRequest(const QString& notificationId,
                                                     const QString& remark,
                                                     const QString& categoryId,
                                                     const QString& categoryName)
{
    ensureGroupNotificationRepositoryConnections();
    return !FriendRemoteDataSource::instance().acceptGroupJoinRequest(notificationId,
                                                                     remark,
                                                                     categoryId,
                                                                     categoryName).isEmpty();
}

bool FriendSessionController::rejectGroupJoinRequest(const QString& notificationId)
{
    ensureGroupNotificationRepositoryConnections();
    return !FriendRemoteDataSource::instance().rejectGroupJoinRequest(notificationId).isEmpty();
}
