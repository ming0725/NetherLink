#pragma once

#include <QObject>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QSet>
#include <QVector>
#include <QMutex>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class UserRepository : public QObject {
    Q_OBJECT
public:
    static UserRepository& instance();

    QVector<FriendSummary> requestFriendList(const FriendListRequest& query = {}) const;
    QVector<FriendGroupSummary> requestFriendGroupSummaries(const FriendGroupListRequest& query = {}) const;
    QVector<FriendSummary> requestFriendsInGroup(const FriendGroupItemsRequest& query) const;
    QVector<User> requestUserSearch(const QString& keyword, int limit = 80, int offset = 0) const;
    QVector<User> requestAllUsers() const;
    User requestUserDetail(const UserDetailRequest& query) const;
    QVector<User> requestUserDetails(const QStringList& userIds) const;
    QMap<QString, QString> requestFriendGroups() const;
    int nextFriendGroupSortOrder() const;
    QString requestUserName(const QString& userId) const;
    QString requestUserAvatarPath(const QString& userId) const;
    QString requestUserAvatarImageAsync(const QString& userId, int delayMs = 120);
    int requestUserVersion(const QString& userId) const;
    bool isFriend(const QString& userId) const;

    void saveUser(const User& user);
    bool upsertUserProfile(const QJsonObject& object, bool preserveFriendFields = true);
    bool upsertFriendGroup(const QJsonObject& object);
    bool removeFriendGroup(const QString& friendGroupId);
    bool upsertPresence(const QString& userUuid, const QString& status, const QString& lastSeenAt = {});
    QString refreshPresenceBatch(const QStringList& userUuids);
    QString refreshFriendPresenceSnapshot();
    bool needsUserRefresh(const QString& userUuid, int remoteVersion) const;
    void addFriend(const QString& userId,
                   const QString& groupId = QStringLiteral("default"),
                   const QString& groupName = QStringLiteral("默认分组"));
    void removeUser(const QString& userID);

signals:
    void friendListChanged();
    void userAvatarImageReady(const QString& requestId, const QString& userId, const QImage& image);
    void userAvatarImageFailed(const QString& requestId, const QString& userId);

private:
    explicit UserRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(UserRepository)
    void reloadFromStore();

    QMap<QString, User> userMap;
    QSet<QString> m_pendingPresenceBatchRequestIds;
    QHash<QString, QStringList> m_pendingPresenceSnapshotRequestIds;
    QSet<QString> m_presenceSnapshotRequestedUserIds;
    mutable QMutex mutex; // 用于线程安全
};
