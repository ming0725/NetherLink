#include "GroupRepository.h"

#include <QCollator>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QRunnable>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include <algorithm>

#include "app/state/CurrentUser.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"

namespace {

const QString kCreatedCategoryId = QStringLiteral("gg_created");
const QString kCreatedCategoryName = QStringLiteral("我创建的群聊");
const QString kManagedCategoryId = QStringLiteral("gg_managed");
const QString kManagedCategoryName = QStringLiteral("我管理的群聊");
const QString kJoinedCategoryId = QStringLiteral("gg_joined");
const QString kJoinedCategoryName = QStringLiteral("我加入的群聊");
const QString kCollegeCategoryId = QStringLiteral("gg_college");
const QString kCollegeCategoryName = QStringLiteral("创意学院");
const QString kWorkCategoryId = QStringLiteral("gg_work");
const QString kWorkCategoryName = QStringLiteral("工坊协作");
const QString kPerformanceCategoryId = QStringLiteral("gg_performance");
const QString kPerformanceCategoryName = QStringLiteral("大型存档压测");

QString currentUserId()
{
    return CurrentUser::instance().getUserId();
}

bool isCurrentUserOwnerOf(const Group& group)
{
    return !group.ownerId.isEmpty() && group.ownerId == currentUserId();
}

bool isCurrentUserAdminOf(const Group& group)
{
    const QString userId = currentUserId();
    return !userId.isEmpty() && group.adminsID.contains(userId);
}

QString normalizedBaseCategoryId(const Group& group)
{
    return group.listGroupId.isEmpty() ? kJoinedCategoryId : group.listGroupId;
}

QString normalizedBaseCategoryName(const Group& group)
{
    return group.listGroupName.isEmpty() ? kJoinedCategoryName : group.listGroupName;
}

void appendListEntry(QVector<Group>& groups,
                     const Group& group,
                     const QString& categoryId,
                     const QString& categoryName)
{
    Group normalized = group;
    normalized.listGroupId = categoryId;
    normalized.listGroupName = categoryName;
    groups.push_back(normalized);
}

void appendUniqueMember(QVector<QString>& members, QSet<QString>& seen, const QString& userId)
{
    if (userId.isEmpty() || seen.contains(userId)) {
        return;
    }

    members.push_back(userId);
    seen.insert(userId);
}

QVector<QString> sampleNonFriendMemberIds()
{
    const QStringList candidateIds = {
            QStringLiteral("u101"),
            QStringLiteral("u102"),
            QStringLiteral("u103"),
            QStringLiteral("u104"),
            QStringLiteral("u105"),
            QStringLiteral("u106"),
            QStringLiteral("u107"),
            QStringLiteral("u108"),
            QStringLiteral("u109"),
            QStringLiteral("u110"),
            QStringLiteral("u111"),
            QStringLiteral("u112"),
            QStringLiteral("u113"),
            QStringLiteral("u114"),
            QStringLiteral("u115"),
            QStringLiteral("u116"),
            QStringLiteral("u117"),
            QStringLiteral("u118"),
            QStringLiteral("u119"),
            QStringLiteral("u120"),
            QStringLiteral("u121"),
            QStringLiteral("u122"),
            QStringLiteral("u123"),
            QStringLiteral("u124"),
            QStringLiteral("u125"),
            QStringLiteral("u126"),
            QStringLiteral("u127"),
            QStringLiteral("u128"),
            QStringLiteral("u129"),
            QStringLiteral("u130"),
            QStringLiteral("u131"),
            QStringLiteral("u132"),
            QStringLiteral("u133"),
            QStringLiteral("u134"),
            QStringLiteral("u135")
    };

    QVector<QString> userIds;
    userIds.reserve(candidateIds.size());
    for (const QString& userId : candidateIds) {
        const User user = UserRepository::instance().requestUserDetail({userId});
        if (!user.id.isEmpty() && !user.isFriend) {
            userIds.push_back(user.id);
        }
    }
    return userIds;
}

void appendRotatingMember(QVector<QString>& members,
                          QSet<QString>& seen,
                          const QVector<QString>& userIds,
                          int start,
                          int index)
{
    if (userIds.isEmpty()) {
        return;
    }
    appendUniqueMember(members, seen, userIds.at((start + index) % userIds.size()));
}

void assignSampleMembers(Group& group,
                         const QVector<QString>& friendUserIds,
                         const QVector<QString>& nonFriendUserIds,
                         int ordinal)
{
    QVector<QString> members;
    QSet<QString> seen;
    members.reserve(group.memberNum);

    appendUniqueMember(members, seen, group.ownerId);
    for (const QString& adminId : group.adminsID) {
        appendUniqueMember(members, seen, adminId);
    }

    const int friendStart = friendUserIds.isEmpty() ? 0 : (ordinal * 11) % friendUserIds.size();
    const int nonFriendStart = nonFriendUserIds.isEmpty() ? 0 : (ordinal * 5) % nonFriendUserIds.size();
    appendRotatingMember(members, seen, nonFriendUserIds, nonFriendStart, 0);

    const int totalCandidateCount = friendUserIds.size() + nonFriendUserIds.size();
    for (int index = 0; members.size() < group.memberNum && index < totalCandidateCount * 2; ++index) {
        if (index % 4 == 2 && !nonFriendUserIds.isEmpty()) {
            appendRotatingMember(members, seen, nonFriendUserIds, nonFriendStart, index / 4 + 1);
        } else {
            appendRotatingMember(members, seen, friendUserIds, friendStart, index);
        }
    }

    group.membersID = members;
    group.memberNum = group.membersID.size();
}

QString effectiveCategoryIdFor(const Group& group)
{
    if (isCurrentUserOwnerOf(group)) {
        return kCreatedCategoryId;
    }
    if (isCurrentUserAdminOf(group)) {
        return kManagedCategoryId;
    }
    return normalizedBaseCategoryId(group);
}

QString effectiveCategoryNameFor(const Group& group)
{
    if (isCurrentUserOwnerOf(group)) {
        return kCreatedCategoryName;
    }
    if (isCurrentUserAdminOf(group)) {
        return kManagedCategoryName;
    }
    return normalizedBaseCategoryName(group);
}

int categorySortRank(const QString& categoryId)
{
    if (categoryId == kCreatedCategoryId) {
        return 0;
    }
    if (categoryId == kManagedCategoryId) {
        return 1;
    }
    if (categoryId == kJoinedCategoryId) {
        return 2;
    }
    if (categoryId == kCollegeCategoryId) {
        return 3;
    }
    if (categoryId == kWorkCategoryId) {
        return 4;
    }
    if (categoryId == kPerformanceCategoryId) {
        return 5;
    }
    return 99;
}

QString groupVisibleName(const Group& group)
{
    if (group.remark.isEmpty() || group.groupName.isEmpty()) {
        return group.groupName;
    }
    return QStringLiteral("%1(%2)").arg(group.remark, group.groupName);
}

QString groupVisibleSubtitle(const Group& group)
{
    return QStringLiteral("%1人").arg(group.memberNum);
}

bool matchesGroupSearchKeyword(const Group& group, const QString& keyword)
{
    if (keyword.isEmpty()) {
        return false;
    }

    return group.groupId.contains(keyword, Qt::CaseInsensitive) ||
           group.groupName.contains(keyword, Qt::CaseInsensitive) ||
           group.remark.contains(keyword, Qt::CaseInsensitive) ||
           group.introduction.contains(keyword, Qt::CaseInsensitive);
}

void sortGroupList(QVector<Group>& result)
{
    std::sort(result.begin(), result.end(), [](const Group& lhs, const Group& rhs) {
        const int lhsCategoryRank = categorySortRank(lhs.listGroupId);
        const int rhsCategoryRank = categorySortRank(rhs.listGroupId);
        if (lhsCategoryRank != rhsCategoryRank) {
            return lhsCategoryRank < rhsCategoryRank;
        }
        if (lhs.listGroupName != rhs.listGroupName) {
            static QCollator groupCollator(QLocale::Chinese);
            groupCollator.setNumericMode(true);
            return groupCollator.compare(lhs.listGroupName, rhs.listGroupName) < 0;
        }

        static QCollator localCollator(QLocale::Chinese);
        localCollator.setNumericMode(true);
        const int nameOrder = localCollator.compare(groupVisibleName(lhs), groupVisibleName(rhs));
        if (nameOrder != 0) {
            return nameOrder < 0;
        }

        const int subtitleOrder = localCollator.compare(groupVisibleSubtitle(lhs), groupVisibleSubtitle(rhs));
        if (subtitleOrder != 0) {
            return subtitleOrder < 0;
        }
        return lhs.groupId < rhs.groupId;
    });
}

class GroupListRequestOperation final
    : public RepositoryTemplate<GroupListRequest, QVector<Group>> {
public:
    explicit GroupListRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<Group> doRequest(const GroupListRequest& query) const override
    {
        QVector<Group> result;
        result.reserve(m_groups.size() * 2);
        for (const Group& group : m_groups) {
            const QString baseCategoryId = normalizedBaseCategoryId(group);
            const QString baseCategoryName = normalizedBaseCategoryName(group);
            if (!query.keyword.isEmpty() &&
                !group.groupName.contains(query.keyword, Qt::CaseInsensitive) &&
                !group.remark.contains(query.keyword, Qt::CaseInsensitive) &&
                !effectiveCategoryNameFor(group).contains(query.keyword, Qt::CaseInsensitive) &&
                !baseCategoryName.contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }

            if (isCurrentUserOwnerOf(group)) {
                appendListEntry(result, group, kCreatedCategoryId, kCreatedCategoryName);
            } else if (isCurrentUserAdminOf(group)) {
                appendListEntry(result, group, kManagedCategoryId, kManagedCategoryName);
            }
            appendListEntry(result, group, baseCategoryId, baseCategoryName);
        }
        return result;
    }

    void onAfterRequest(const GroupListRequest&, QVector<Group>& result) const override
    {
        sortGroupList(result);
    }

    QVector<Group> m_groups;
};

class GroupCategoryListRequestOperation final
    : public RepositoryTemplate<GroupCategoryListRequest, QVector<GroupCategorySummary>> {
public:
    explicit GroupCategoryListRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<GroupCategorySummary> doRequest(const GroupCategoryListRequest& query) const override
    {
        const QVector<Group> groups = GroupListRequestOperation(m_groups).request({query.keyword});
        QMap<QString, GroupCategorySummary> categories;
        for (const Group& group : groups) {
            GroupCategorySummary summary = categories.value(group.listGroupId);
            summary.categoryId = group.listGroupId;
            summary.categoryName = group.listGroupName;
            ++summary.groupCount;
            categories.insert(group.listGroupId, summary);
        }
        return QVector<GroupCategorySummary>::fromList(categories.values());
    }

    void onAfterRequest(const GroupCategoryListRequest&, QVector<GroupCategorySummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const GroupCategorySummary& lhs, const GroupCategorySummary& rhs) {
            const int lhsRank = categorySortRank(lhs.categoryId);
            const int rhsRank = categorySortRank(rhs.categoryId);
            if (lhsRank != rhsRank) {
                return lhsRank < rhsRank;
            }

            static QCollator collator(QLocale::Chinese);
            collator.setNumericMode(true);
            const int order = collator.compare(lhs.categoryName, rhs.categoryName);
            return order == 0 ? lhs.categoryId < rhs.categoryId : order < 0;
        });
    }

    QVector<Group> m_groups;
};

class GroupCategoryItemsRequestOperation final
    : public RepositoryTemplate<GroupCategoryItemsRequest, QVector<Group>> {
public:
    explicit GroupCategoryItemsRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<Group> doRequest(const GroupCategoryItemsRequest& query) const override
    {
        QVector<Group> result;
        const QVector<Group> groups = GroupListRequestOperation(m_groups).request({query.keyword});
        for (const Group& group : groups) {
            if (group.listGroupId == query.categoryId) {
                result.push_back(group);
            }
        }
        return result;
    }

    void onAfterRequest(const GroupCategoryItemsRequest& query, QVector<Group>& result) const override
    {
        const int offset = qBound(0, query.offset, result.size());
        const int limit = query.limit < 0 ? result.size() - offset : qMax(0, query.limit);
        result = result.mid(offset, limit);
    }

    QVector<Group> m_groups;
};

class GroupDetailRequestOperation final
    : public RepositoryTemplate<GroupDetailRequest, Group> {
public:
    explicit GroupDetailRequestOperation(QMap<QString, Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    Group doRequest(const GroupDetailRequest& query) const override
    {
        return m_groups.value(query.groupId, Group{});
    }

    QMap<QString, Group> m_groups;
};

Group makeGroup(const QString& groupId,
                const QString& groupName,
                int memberNum,
                const QString& ownerId,
                const QString& avatarPath,
                const QString& listGroupId,
                const QString& listGroupName,
                const QString& remark = QString(),
                bool isDnd = false,
                QVector<QString> adminsID = {})
{
    Group group;
    group.groupId = groupId;
    group.groupName = groupName;
    group.memberNum = memberNum;
    group.ownerId = ownerId;
    group.groupAvatarPath = avatarPath;
    group.isDnd = isDnd;
    group.adminsID = std::move(adminsID);
    group.remark = remark;
    group.introduction = QStringLiteral("%1，专注 Minecraft 创意建筑、红石机关、地图玩法和服务器协作。").arg(groupName);
    group.announcement = QStringLiteral("欢迎来到%1，请用设计稿、坐标和截图友好交流。").arg(groupName);
    group.currentUserNickname = CurrentUser::instance().getUserName();
    group.memberNicknames.insert(CurrentUser::instance().getUserId(), group.currentUserNickname);
    group.listGroupId = listGroupId;
    group.listGroupName = listGroupName;
    return group;
}

QJsonArray stringVectorToJson(const QVector<QString>& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QVector<QString> stringVectorFromJson(const QJsonArray& array)
{
    QVector<QString> values;
    values.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QString text = value.toString();
        if (!text.isEmpty()) {
            values.push_back(text);
        }
    }
    return values;
}

QJsonObject stringMapToJson(const QMap<QString, QString>& map)
{
    QJsonObject object;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        object.insert(it.key(), it.value());
    }
    return object;
}

QMap<QString, QString> stringMapFromJson(const QJsonObject& object)
{
    QMap<QString, QString> map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        map.insert(it.key(), it.value().toString());
    }
    return map;
}

Group groupFromJson(const QJsonObject& object)
{
    Group group;
    group.groupId = object.value(QStringLiteral("groupId")).toString();
    group.groupName = object.value(QStringLiteral("groupName")).toString();
    group.memberNum = object.value(QStringLiteral("memberNum")).toInt();
    group.ownerId = object.value(QStringLiteral("ownerId")).toString();
    group.groupAvatarPath = object.value(QStringLiteral("groupAvatarPath")).toString();
    group.isDnd = object.value(QStringLiteral("isDnd")).toBool(false);
    group.adminsID = stringVectorFromJson(object.value(QStringLiteral("adminsID")).toArray());
    group.remark = object.value(QStringLiteral("remark")).toString();
    group.introduction = object.value(QStringLiteral("introduction")).toString();
    group.announcement = object.value(QStringLiteral("announcement")).toString();
    group.currentUserNickname = object.value(QStringLiteral("currentUserNickname")).toString();
    group.memberNicknames = stringMapFromJson(object.value(QStringLiteral("memberNicknames")).toObject());
    group.membersID = stringVectorFromJson(object.value(QStringLiteral("membersID")).toArray());
    group.listGroupId = object.value(QStringLiteral("listGroupId")).toString(QStringLiteral("gg_joined"));
    group.listGroupName = object.value(QStringLiteral("listGroupName")).toString(QStringLiteral("我加入的群聊"));
    if (group.memberNum <= 0 && !group.membersID.isEmpty()) {
        group.memberNum = group.membersID.size();
    }
    return group;
}

QJsonObject groupToJson(const Group& group)
{
    return {
            {QStringLiteral("groupId"), group.groupId},
            {QStringLiteral("groupName"), group.groupName},
            {QStringLiteral("memberNum"), group.memberNum},
            {QStringLiteral("ownerId"), group.ownerId},
            {QStringLiteral("groupAvatarPath"), group.groupAvatarPath},
            {QStringLiteral("isDnd"), group.isDnd},
            {QStringLiteral("adminsID"), stringVectorToJson(group.adminsID)},
            {QStringLiteral("remark"), group.remark},
            {QStringLiteral("introduction"), group.introduction},
            {QStringLiteral("announcement"), group.announcement},
            {QStringLiteral("currentUserNickname"), group.currentUserNickname},
            {QStringLiteral("memberNicknames"), stringMapToJson(group.memberNicknames)},
            {QStringLiteral("membersID"), stringVectorToJson(group.membersID)},
            {QStringLiteral("listGroupId"), group.listGroupId},
            {QStringLiteral("listGroupName"), group.listGroupName}
    };
}

} // namespace

GroupRepository::GroupRepository(QObject* parent)
        : QObject(parent)
{
    LocalDataStore& store = LocalDataStore::instance();
    if (!store.hasDomain(QStringLiteral("groups"))) {
        const QJsonArray groups = store.seedArray(QStringLiteral(":/resources/data/groups.json"));
        for (const QJsonValue& value : groups) {
            Group group = groupFromJson(value.toObject());
            if (!group.groupId.isEmpty()) {
                if (group.introduction.isEmpty()) {
                    group.introduction = QStringLiteral("%1，专注 Minecraft 创意建筑、红石机关、地图玩法和服务器协作。")
                            .arg(group.groupName);
                }
                if (group.announcement.isEmpty()) {
                    group.announcement = QStringLiteral("欢迎来到%1，请用设计稿、坐标和截图友好交流。")
                            .arg(group.groupName);
                }
                if (group.currentUserNickname.isEmpty()) {
                    group.currentUserNickname = CurrentUser::instance().getUserName();
                }
                store.upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
            }
        }
    }

    for (const QJsonObject& object : store.values(QStringLiteral("groups"))) {
        const Group group = groupFromJson(object);
        if (!group.groupId.isEmpty()) {
            groupMap.insert(group.groupId, group);
        }
    }

    QVector<QString> friendUserIds;
    const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
    friendUserIds.reserve(friends.size());
    for (const FriendSummary& friendSummary : friends) {
        friendUserIds.push_back(friendSummary.userId);
    }
    const QVector<QString> nonFriendUserIds = sampleNonFriendMemberIds();

    int ordinal = 0;
    for (Group& group : groupMap) {
        if (group.membersID.isEmpty()) {
            assignSampleMembers(group, friendUserIds, nonFriendUserIds, ordinal);
            store.upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
        }
        ++ordinal;
    }
}

GroupRepository& GroupRepository::instance()
{
    static GroupRepository repo;
    return repo;
}

QVector<Group> GroupRepository::requestGroupList(const GroupListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupListRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
}

QVector<GroupCategorySummary> GroupRepository::requestGroupCategorySummaries(const GroupCategoryListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupCategoryListRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
}

QVector<Group> GroupRepository::requestGroupsInCategory(const GroupCategoryItemsRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupCategoryItemsRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
}

QVector<Group> GroupRepository::requestGroupSearch(const QString& keyword, int limit, int offset) const
{
    const QString trimmedKeyword = keyword.trimmed();
    QMutexLocker locker(&mutex);

    QVector<Group> result;
    result.reserve(qMin(qMax(0, limit), groupMap.size()));
    for (const Group& group : groupMap) {
        if (!matchesGroupSearchKeyword(group, trimmedKeyword)) {
            continue;
        }
        result.push_back(group);
    }

    locker.unlock();
    sortGroupList(result);

    const int boundedOffset = qBound(0, offset, result.size());
    const int boundedLimit = limit < 0 ? result.size() - boundedOffset : qMax(0, limit);
    return result.mid(boundedOffset, boundedLimit);
}

Group GroupRepository::requestGroupDetail(const GroupDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupDetailRequestOperation(groupMap).request(query);
}

QString GroupRepository::requestGroupAvatarPath(const QString& groupId) const
{
    return requestGroupDetail({groupId}).groupAvatarPath;
}

QString GroupRepository::requestGroupAvatarImageAsync(const QString& groupId, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString source = requestGroupAvatarPath(groupId);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, groupId, source, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        QImageReader reader(source);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        QMetaObject::invokeMethod(this, [this, requestId, groupId, image]() {
            if (image.isNull()) {
                emit groupAvatarImageFailed(requestId, groupId);
                return;
            }
            emit groupAvatarImageReady(requestId, groupId, image);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

QMap<QString, QString> GroupRepository::requestGroupCategories() const
{
    return {
            {kJoinedCategoryId, kJoinedCategoryName},
            {kCollegeCategoryId, kCollegeCategoryName},
            {kWorkCategoryId, kWorkCategoryName},
            {kPerformanceCategoryId, kPerformanceCategoryName}
    };
}

QString GroupRepository::effectiveGroupCategoryId(const Group& group) const
{
    return effectiveCategoryIdFor(group);
}

QString GroupRepository::effectiveGroupCategoryName(const Group& group) const
{
    return effectiveCategoryNameFor(group);
}

bool GroupRepository::isCurrentUserGroupOwner(const Group& group) const
{
    return isCurrentUserOwnerOf(group);
}

bool GroupRepository::isCurrentUserGroupAdmin(const Group& group) const
{
    return isCurrentUserAdminOf(group);
}

bool GroupRepository::contains(const QString& groupId) const
{
    QMutexLocker locker(&mutex);
    return groupMap.contains(groupId);
}

void GroupRepository::saveGroup(const Group& group)
{
    QMutexLocker locker(&mutex);
    groupMap[group.groupId] = group;
    locker.unlock();
    LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
    emit groupListChanged();
}

void GroupRepository::addMember(const QString& groupId, const QString& userId)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = groupMap.find(groupId);
        if (it == groupMap.end()) {
            return;
        }
        Group& group = it.value();
        if (!group.membersID.contains(userId)) {
            group.membersID.push_back(userId);
            group.memberNum = group.membersID.size();
            changed = true;
        }
        if (changed) {
            LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
        }
    }
    if (changed) {
        emit groupListChanged();
    }
}

void GroupRepository::removeMember(const QString& groupId, const QString& userId)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = groupMap.find(groupId);
        if (it == groupMap.end()) {
            return;
        }
        Group& group = it.value();
        changed = group.membersID.removeAll(userId) > 0;
        group.adminsID.removeAll(userId);
        if (changed) {
            group.memberNum = group.membersID.size();
            LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
        }
    }
    if (changed) {
        emit groupListChanged();
    }
}

void GroupRepository::setAdmin(const QString& groupId, const QString& userId, bool enabled)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = groupMap.find(groupId);
        if (it == groupMap.end()) {
            return;
        }
        Group& group = it.value();
        const bool isAdmin = group.adminsID.contains(userId);
        if (enabled && !isAdmin) {
            group.adminsID.push_back(userId);
            changed = true;
        } else if (!enabled && isAdmin) {
            group.adminsID.removeAll(userId);
            changed = true;
        }
        if (!group.membersID.contains(userId)) {
            group.membersID.push_back(userId);
            group.memberNum = group.membersID.size();
            changed = true;
        }
        if (changed) {
            LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
        }
    }
    if (changed) {
        emit groupListChanged();
    }
}

void GroupRepository::transferOwner(const QString& groupId, const QString& userId)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = groupMap.find(groupId);
        if (it == groupMap.end() || it->ownerId == userId) {
            return;
        }
        Group& group = it.value();
        group.adminsID.removeAll(userId);
        if (!group.ownerId.isEmpty() && !group.adminsID.contains(group.ownerId)) {
            group.adminsID.push_back(group.ownerId);
        }
        group.ownerId = userId;
        if (!group.membersID.contains(userId)) {
            group.membersID.push_back(userId);
            group.memberNum = group.membersID.size();
        }
        changed = true;
        LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
    }
    if (changed) {
        emit groupListChanged();
    }
}

void GroupRepository::removeGroup(const QString& groupID)
{
    QMutexLocker locker(&mutex);
    const bool removed = groupMap.remove(groupID) > 0;
    locker.unlock();
    if (removed) {
        LocalDataStore::instance().removeValue(QStringLiteral("groups"), groupID);
        emit groupListChanged();
    }
}
