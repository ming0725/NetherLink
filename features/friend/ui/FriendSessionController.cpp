#include "features/friend/ui/FriendSessionController.h"

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/data/UserRepository.h"

FriendSessionController::FriendSessionController(QObject* parent)
    : QObject(parent)
{
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
    return user.nick.isEmpty() ? user.id : user.nick;
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

bool FriendSessionController::saveFriend(const User& user)
{
    if (user.id.isEmpty()) {
        return false;
    }
    ensureUserRepositoryConnections();
    UserRepository::instance().saveUser(user);
    return true;
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
    UserRepository::instance().saveUser(user);
    return true;
}

bool FriendSessionController::deleteFriend(const QString& userId)
{
    if (userId.isEmpty()) {
        return false;
    }

    ensureUserRepositoryConnections();
    MessageRepository::instance().removeConversation(userId);
    UserRepository::instance().removeUser(userId);
    return true;
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
        return groupId;
    }
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

QString FriendSessionController::groupNicknameFor(const QString& groupId, const QString& userId) const
{
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
    GroupRepository::instance().saveGroup(group);
    return true;
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

    group.listGroupId = categoryId;
    group.listGroupName = categoryName;
    GroupRepository::instance().saveGroup(group);
    return true;
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

    MessageRepository::instance().removeConversation(groupId);
    GroupRepository::instance().removeGroup(groupId);
    return true;
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
    FriendNotificationRepository::instance().markAllRead();
}

bool FriendSessionController::acceptFriendRequest(const QString& notificationId)
{
    ensureFriendNotificationRepositoryConnections();
    return FriendNotificationRepository::instance().acceptRequest(notificationId);
}

bool FriendSessionController::rejectFriendRequest(const QString& notificationId)
{
    ensureFriendNotificationRepositoryConnections();
    return FriendNotificationRepository::instance().rejectRequest(notificationId);
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
    GroupNotificationRepository::instance().markAllRead();
}

bool FriendSessionController::acceptGroupJoinRequest(const QString& notificationId)
{
    ensureGroupNotificationRepositoryConnections();
    return GroupNotificationRepository::instance().acceptJoinRequest(notificationId);
}

bool FriendSessionController::rejectGroupJoinRequest(const QString& notificationId)
{
    ensureGroupNotificationRepositoryConnections();
    return GroupNotificationRepository::instance().rejectJoinRequest(notificationId);
}
