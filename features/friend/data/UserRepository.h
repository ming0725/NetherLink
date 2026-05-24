#pragma once

#include <QObject>
#include <QImage>
#include <QMap>
#include <QStringList>
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
    User requestUserDetail(const UserDetailRequest& query) const;
    QVector<User> requestUserDetails(const QStringList& userIds) const;
    QMap<QString, QString> requestFriendGroups() const;
    QString requestUserName(const QString& userId) const;
    QString requestUserAvatarPath(const QString& userId) const;
    QString requestUserAvatarImageAsync(const QString& userId, int delayMs = 120);
    bool isFriend(const QString& userId) const;

    void saveUser(const User& user);
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
    QMap<QString, User> userMap;
    mutable QMutex mutex; // 用于线程安全
};
