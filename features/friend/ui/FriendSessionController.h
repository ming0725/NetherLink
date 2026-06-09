#pragma once

#include <QObject>
#include <QImage>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

#include "shared/types/FriendNotification.h"
#include "shared/types/Group.h"
#include "shared/types/GroupNotification.h"
#include "shared/network/NetworkTypes.h"
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class FriendSessionController : public QObject
{
    Q_OBJECT

public:
    explicit FriendSessionController(QObject* parent = nullptr);

    QVector<FriendGroupSummary> loadFriendGroupSummaries(const QString& keyword) const;
    QVector<FriendSummary> loadFriendsInGroup(const QString& groupId,
                                              const QString& keyword,
                                              int offset,
                                              int limit) const;
    User loadFriend(const QString& userId) const;
    QString userNickname(const QString& userId) const;
    QString userAvatarPath(const QString& userId) const;
    QString requestUserAvatarImage(const QString& userId);
    QMap<QString, QString> loadFriendGroups() const;
    QString refreshFriendPresenceSnapshot() const;
    bool saveFriend(const User& user);
    bool changeFriendGroup(const QString& userId, const QString& groupId, const QString& groupName);
    bool deleteFriend(const QString& userId);
    bool createFriendGroup(const QString& name);
    bool renameFriendGroup(const QString& friendGroupId, const QString& name);
    bool deleteFriendGroup(const QString& friendGroupId);

    QVector<GroupCategorySummary> loadGroupCategorySummaries(const QString& keyword) const;
    QVector<Group> loadGroupsInCategory(const QString& categoryId,
                                        const QString& keyword,
                                        int offset,
                                        int limit) const;
    Group loadGroup(const QString& groupId) const;
    QString groupDisplayName(const QString& groupId) const;
    QString groupNicknameFor(const QString& groupId, const QString& userId) const;
    QString groupManagerRoleText(const QString& groupId, const QString& userId) const;
    QString groupAvatarPath(const QString& groupId) const;
    QString requestGroupAvatarImage(const QString& groupId);
    QMap<QString, QString> loadGroupCategories() const;
    bool canExitGroup(const Group& group) const;
    bool saveGroup(const Group& group);
    bool createGroupCategory(const QString& categoryName);
    bool renameGroupCategory(const QString& categoryId, const QString& categoryName);
    bool deleteGroupCategory(const QString& categoryId);
    bool changeGroupCategory(const QString& groupId,
                             const QString& categoryId,
                             const QString& categoryName);
    bool exitGroup(const QString& groupId);

    QVector<FriendNotification> loadFriendNotifications(int offset, int limit) const;
    int friendNotificationCount() const;
    int friendUnreadCount() const;
    void markFriendNotificationsRead();
    bool acceptFriendRequest(const QString& notificationId,
                             const QString& remark = QString(),
                             const QString& groupId = QStringLiteral("default"),
                             const QString& groupName = QStringLiteral("默认分组"));
    bool rejectFriendRequest(const QString& notificationId);

    QVector<GroupNotification> loadGroupNotifications(int offset, int limit) const;
    int groupNotificationCount() const;
    int groupUnreadCount() const;
    void markGroupNotificationsRead();
    bool acceptGroupJoinRequest(const QString& notificationId,
                                const QString& remark = QString(),
                                const QString& categoryId = QStringLiteral("gg_joined"),
                                const QString& categoryName = QStringLiteral("我加入的群聊"));
    bool rejectGroupJoinRequest(const QString& notificationId);

signals:
    void friendListChanged();
    void groupListChanged();
    void friendNotificationListChanged();
    void groupNotificationListChanged();
    void userAvatarImageReady(const QString& requestId, const QString& userId, const QImage& image);
    void userAvatarImageFailed(const QString& requestId, const QString& userId);
    void groupAvatarImageReady(const QString& requestId, const QString& groupId, const QImage& image);
    void groupAvatarImageFailed(const QString& requestId, const QString& groupId);
    void friendRequestActionFailed(const QString& notificationId, const NetworkError& error);
    void groupJoinRequestActionFailed(const QString& notificationId, const NetworkError& error);
    void friendUpdateFailed(const QString& userId, const NetworkError& error);
    void friendDeleteFailed(const QString& userId, const NetworkError& error);
    void friendGroupActionFailed(const QString& friendGroupId, const NetworkError& error);
    void groupUpdateFailed(const QString& groupId, const NetworkError& error);
    void groupMySettingsUpdateFailed(const QString& groupId, const NetworkError& error);
    void groupLeaveSucceeded(const QString& groupId);
    void groupLeaveFailed(const QString& groupId, const NetworkError& error);
    void notificationMarkReadFailed(const QString& scope, const NetworkError& error);

private:
    void ensureUserRepositoryConnections() const;
    void ensureGroupRepositoryConnections() const;
    void ensureFriendNotificationRepositoryConnections() const;
    void ensureGroupNotificationRepositoryConnections() const;

    mutable bool m_userRepositoryConnectionsReady = false;
    mutable bool m_groupRepositoryConnectionsReady = false;
    mutable bool m_friendNotificationRepositoryConnectionsReady = false;
    mutable bool m_groupNotificationRepositoryConnectionsReady = false;
    QString m_pendingFriendNotificationsReadRequestId;
    QSet<QString> m_pendingGroupNotificationsReadRequestIds;
};
