#pragma once
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/Group.h"

class GroupRepository : public QObject{
    Q_OBJECT
public:
    static GroupRepository& instance();

    QVector<Group> requestGroupList(const GroupListRequest& query = {}) const;
    QVector<GroupCategorySummary> requestGroupCategorySummaries(const GroupCategoryListRequest& query = {}) const;
    QVector<Group> requestGroupsInCategory(const GroupCategoryItemsRequest& query) const;
    QVector<Group> requestGroupSearch(const QString& keyword, int limit = 80, int offset = 0) const;
    Group requestGroupDetail(const GroupDetailRequest& query) const;
    QString requestGroupAvatarPath(const QString& groupId) const;
    QString requestGroupAvatarImageAsync(const QString& groupId, int delayMs = 120);
    QMap<QString, QString> requestGroupCategories() const;
    bool upsertGroupCategory(const QString& categoryId, const QString& categoryName);
    bool removeGroupCategory(const QString& categoryId);
    QString effectiveGroupCategoryId(const Group& group) const;
    QString effectiveGroupCategoryName(const Group& group) const;
    bool isCurrentUserGroupOwner(const Group& group) const;
    bool isCurrentUserGroupAdmin(const Group& group) const;
    bool contains(const QString& groupId) const;
    GroupMemberProfile requestGroupMember(const QString& groupId, const QString& userId) const;
    QString requestGroupMemberNickname(const QString& groupId, const QString& userId) const;
    int requestGroupMemberVersion(const QString& groupId, const QString& userId) const;

    bool upsertGroup(const QJsonObject& object);
    void saveGroup(const Group& group);
    bool upsertGroupMember(const QJsonObject& object);
    bool needsGroupMemberRefresh(const QString& groupId, const QString& userId, int remoteVersion) const;
    void addMember(const QString& groupId, const QString& userId);
    void removeMember(const QString& groupId, const QString& userId);
    void setAdmin(const QString& groupId, const QString& userId, bool enabled);
    void transferOwner(const QString& groupId, const QString& userId);
    void removeGroup(const QString& groupID);

signals:
    void groupListChanged();
    void groupAvatarImageReady(const QString& requestId, const QString& groupId, const QImage& image);
    void groupAvatarImageFailed(const QString& requestId, const QString& groupId);

private:
    explicit GroupRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRepository)
    void reloadFromStore();

    QMap<QString, Group> groupMap;
    QMap<QString, GroupMemberProfile> groupMemberMap;
    mutable QMutex mutex; // 用于线程安全
};
