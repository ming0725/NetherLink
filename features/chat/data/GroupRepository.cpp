#include "GroupRepository.h"

#include <QCollator>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include <algorithm>

#include "app/state/CurrentUser.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/network/AppEventBus.h"
#include "shared/services/AvatarSource.h"
#include "shared/services/ImageService.h"

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

QString avatarFileIdFrom(const QJsonObject& object)
{
    const QJsonObject avatar = object.value(QStringLiteral("avatar")).toObject();
    const QString nestedFileId = firstString(avatar, {QStringLiteral("fileId"), QStringLiteral("id")});
    if (!nestedFileId.isEmpty()) {
        return nestedFileId;
    }

    return firstString(object, {QStringLiteral("avatarFileId"),
                                QStringLiteral("avatar_file_id"),
                                QStringLiteral("fileId")});
}

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
    group.groupId = object.value(QStringLiteral("groupId")).toString(object.value(QStringLiteral("id")).toString());
    group.groupName = object.value(QStringLiteral("groupName")).toString(object.value(QStringLiteral("name")).toString());
    group.memberNum = object.value(QStringLiteral("memberNum")).toInt(object.value(QStringLiteral("memberCount")).toInt());
    group.ownerId = object.value(QStringLiteral("ownerId")).toString(object.value(QStringLiteral("ownerUuid")).toString());
    group.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    group.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    group.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = object.value(QStringLiteral("groupAvatarPath")).toString(
                object.value(QStringLiteral("avatarUrl")).toString());
    }
    group.groupAvatarPath = AvatarSource::versioned(
            avatarPath,
            group.avatarVersion,
            group.avatarEtag,
            group.avatarContentHash);
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
            {QStringLiteral("avatarVersion"), group.avatarVersion},
            {QStringLiteral("avatarEtag"), group.avatarEtag},
            {QStringLiteral("avatarContentHash"), group.avatarContentHash},
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
    reloadFromStore();

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
                if (domain == QStringLiteral("groups")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
        if (type != QStringLiteral("group.updated")) {
            return;
        }

        QJsonObject object = payload.value(QStringLiteral("group")).toObject();
        if (object.isEmpty()) {
            object = payload;
        }
        const QString groupId = payload.value(QStringLiteral("groupId")).toString();
        if (!groupId.isEmpty() && !object.contains(QStringLiteral("groupId"))) {
            object.insert(QStringLiteral("groupId"), groupId);
        }

        Group group = groupFromJson(object);
        if (group.groupId.isEmpty()) {
            return;
        }

        const Group previous = requestGroupDetail({group.groupId});
        if (!previous.groupId.isEmpty()) {
            if (group.groupName.isEmpty()) {
                group.groupName = previous.groupName;
            }
            group.memberNum = group.memberNum > 0 ? group.memberNum : previous.memberNum;
            group.ownerId = object.contains(QStringLiteral("ownerId")) ? group.ownerId : previous.ownerId;
            group.isDnd = previous.isDnd;
            group.adminsID = object.contains(QStringLiteral("adminsID")) ? group.adminsID : previous.adminsID;
            group.remark = previous.remark;
            group.introduction = object.contains(QStringLiteral("introduction")) ? group.introduction : previous.introduction;
            group.announcement = object.contains(QStringLiteral("announcement")) ? group.announcement : previous.announcement;
            group.currentUserNickname = previous.currentUserNickname;
            group.memberNicknames = previous.memberNicknames;
            group.membersID = object.contains(QStringLiteral("membersID")) ? group.membersID : previous.membersID;
            group.listGroupId = previous.listGroupId;
            group.listGroupName = previous.listGroupName;
        }

        saveGroup(group);
    });
}

GroupRepository& GroupRepository::instance()
{
    static GroupRepository repo;
    return repo;
}

void GroupRepository::reloadFromStore()
{
    QMap<QString, Group> nextGroups;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("groups"))) {
        const Group group = groupFromJson(object);
        if (!group.groupId.isEmpty()) {
            nextGroups.insert(group.groupId, group);
        }
    }

    {
        QMutexLocker locker(&mutex);
        groupMap = nextGroups;
    }
    emit groupListChanged();
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

        QImageReader reader(AvatarSource::cleanForIo(source));
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
    const QString oldAvatarPath = requestGroupAvatarPath(group.groupId);
    QMutexLocker locker(&mutex);
    groupMap[group.groupId] = group;
    locker.unlock();
    LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
    if (!oldAvatarPath.isEmpty() && oldAvatarPath != group.groupAvatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
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
