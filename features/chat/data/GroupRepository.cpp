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
const QString kGroupCategoriesDomain = QStringLiteral("group_categories");

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

QDateTime dateTimeFromString(const QString& value)
{
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
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

QString groupMemberKey(const QString& groupId, const QString& userId)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return {};
    }
    return groupId + QLatin1Char(':') + userId;
}

QString groupMemberRoleToString(GroupMemberRoleValue role)
{
    switch (role) {
    case GroupMemberRoleValue::Owner:
        return QStringLiteral("owner");
    case GroupMemberRoleValue::Admin:
        return QStringLiteral("admin");
    case GroupMemberRoleValue::Ai:
        return QStringLiteral("ai");
    case GroupMemberRoleValue::Member:
    default:
        return QStringLiteral("member");
    }
}

GroupMemberRoleValue groupMemberRoleFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("owner")) {
        return GroupMemberRoleValue::Owner;
    }
    if (normalized == QStringLiteral("admin")) {
        return GroupMemberRoleValue::Admin;
    }
    if (normalized == QStringLiteral("ai")) {
        return GroupMemberRoleValue::Ai;
    }
    return GroupMemberRoleValue::Member;
}

bool isCurrentUserOwnerOf(const Group& group)
{
    if (group.role.trimmed().toLower() == QStringLiteral("owner")) {
        return true;
    }
    return !group.ownerId.isEmpty() && group.ownerId == currentUserId();
}

bool isCurrentUserAdminOf(const Group& group)
{
    if (group.role.trimmed().toLower() == QStringLiteral("admin")) {
        return true;
    }
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
    return 99;
}

QJsonObject groupCategoryToJson(const QString& categoryId, const QString& categoryName)
{
    return {
            {QStringLiteral("categoryId"), categoryId},
            {QStringLiteral("categoryName"), categoryName}
    };
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

    return group.groupPublicId.contains(keyword, Qt::CaseInsensitive) ||
           group.groupName.contains(keyword, Qt::CaseInsensitive) ||
           group.remark.contains(keyword, Qt::CaseInsensitive);
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
                !group.groupPublicId.contains(query.keyword, Qt::CaseInsensitive) &&
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
            } else {
                appendListEntry(result, group, baseCategoryId, baseCategoryName);
            }
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
    GroupCategoryListRequestOperation(QVector<Group> groups, QMap<QString, QString> customCategories)
        : m_groups(std::move(groups))
        , m_customCategories(std::move(customCategories))
    {
    }

private:
    QVector<GroupCategorySummary> doRequest(const GroupCategoryListRequest& query) const override
    {
        const QVector<Group> groups = GroupListRequestOperation(m_groups).request({query.keyword});
        QMap<QString, GroupCategorySummary> categories;
        auto addSystemCategory = [&categories, &query](const QString& categoryId, const QString& categoryName) {
            if (query.keyword.isEmpty() || categoryName.contains(query.keyword, Qt::CaseInsensitive)) {
                categories.insert(categoryId, GroupCategorySummary{categoryId, categoryName, 0});
            }
        };
        addSystemCategory(kCreatedCategoryId, kCreatedCategoryName);
        addSystemCategory(kManagedCategoryId, kManagedCategoryName);
        addSystemCategory(kJoinedCategoryId, kJoinedCategoryName);
        for (auto it = m_customCategories.constBegin(); it != m_customCategories.constEnd(); ++it) {
            if (it.key().isEmpty() || it.key() == kJoinedCategoryId) {
                continue;
            }
            if (!query.keyword.isEmpty() && !it.value().contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }
            categories.insert(it.key(), GroupCategorySummary{it.key(), it.value(), 0});
        }
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
    QMap<QString, QString> m_customCategories;
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
            continue;
        }

        QJsonObject object = value.toObject();
        const QJsonObject nestedUser = object.value(QStringLiteral("user")).toObject();
        if (!nestedUser.isEmpty()) {
            object = nestedUser;
        }
        const QString userUuid = firstString(object, {QStringLiteral("userUuid"),
                                                      QStringLiteral("memberUserUuid"),
                                                      QStringLiteral("uuid"),
                                                      QStringLiteral("id"),
                                                      QStringLiteral("userId")});
        if (!userUuid.isEmpty()) {
            values.push_back(userUuid);
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
    group.groupPublicId = firstString(object, {QStringLiteral("groupPublicId"),
                                               QStringLiteral("publicId"),
                                               QStringLiteral("public_id")});
    group.version = object.value(QStringLiteral("version")).toInt();
    group.etag = object.value(QStringLiteral("etag")).toString();
    group.groupName = object.value(QStringLiteral("groupName")).toString(object.value(QStringLiteral("name")).toString());
    group.memberNum = object.value(QStringLiteral("memberNum")).toInt(object.value(QStringLiteral("memberCount")).toInt());
    group.ownerId = object.value(QStringLiteral("ownerUuid")).toString(object.value(QStringLiteral("ownerId")).toString());
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
    group.currentUserNickname = object.value(QStringLiteral("currentUserNickname")).toString(
            object.value(QStringLiteral("nickname")).toString());
    group.memberNicknames = stringMapFromJson(object.value(QStringLiteral("memberNicknames")).toObject());
    group.membersID = stringVectorFromJson(object.value(QStringLiteral("membersID")).toArray());
    group.listGroupId = object.value(QStringLiteral("listGroupId")).toString();
    group.listGroupName = object.value(QStringLiteral("listGroupName")).toString();
    group.role = object.value(QStringLiteral("role")).toString();
    if (group.memberNum <= 0 && !group.membersID.isEmpty()) {
        group.memberNum = group.membersID.size();
    }
    return group;
}

QJsonObject groupToJson(const Group& group)
{
    return {
            {QStringLiteral("groupId"), group.groupId},
            {QStringLiteral("groupPublicId"), group.groupPublicId},
            {QStringLiteral("version"), group.version},
            {QStringLiteral("etag"), group.etag},
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
            {QStringLiteral("listGroupName"), group.listGroupName},
            {QStringLiteral("role"), group.role}
    };
}

bool objectHasAnyKey(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.contains(key)) {
            return true;
        }
    }
    return false;
}

bool objectHasAvatarFields(const QJsonObject& object)
{
    return objectHasAnyKey(object,
                           {QStringLiteral("groupAvatarPath"),
                            QStringLiteral("avatarUrl"),
                            QStringLiteral("avatarFileId"),
                            QStringLiteral("avatar_file_id"),
                            QStringLiteral("fileId"),
                            QStringLiteral("avatar"),
                            QStringLiteral("avatarVersion"),
                            QStringLiteral("avatarEtag"),
                            QStringLiteral("avatarContentHash")});
}

Group mergedGroupFromObject(const QJsonObject& object, const Group& previous)
{
    Group group = groupFromJson(object);
    if (group.groupId.isEmpty() || previous.groupId.isEmpty()) {
        return group;
    }

    if (group.groupName.isEmpty()) {
        group.groupName = previous.groupName;
    }
    group.groupPublicId = objectHasAnyKey(object,
                                          {QStringLiteral("groupPublicId"),
                                           QStringLiteral("publicId"),
                                           QStringLiteral("public_id")})
            ? group.groupPublicId
            : previous.groupPublicId;
    group.version = group.version > 0 ? group.version : previous.version;
    group.etag = object.contains(QStringLiteral("etag")) ? group.etag : previous.etag;
    group.memberNum = group.memberNum > 0 ? group.memberNum : previous.memberNum;
    if (!objectHasAnyKey(object, {QStringLiteral("ownerUuid"), QStringLiteral("ownerId")})) {
        group.ownerId = previous.ownerId;
    }
    if (!objectHasAvatarFields(object)) {
        group.groupAvatarPath = previous.groupAvatarPath;
        group.avatarVersion = previous.avatarVersion;
        group.avatarEtag = previous.avatarEtag;
        group.avatarContentHash = previous.avatarContentHash;
    } else if (group.groupAvatarPath.isEmpty() && !previous.groupAvatarPath.isEmpty()) {
        group.groupAvatarPath = previous.groupAvatarPath;
    }
    group.isDnd = object.contains(QStringLiteral("isDnd")) ? group.isDnd : previous.isDnd;
    group.adminsID = object.contains(QStringLiteral("adminsID")) ? group.adminsID : previous.adminsID;
    group.remark = object.contains(QStringLiteral("remark")) ? group.remark : previous.remark;
    group.introduction = object.contains(QStringLiteral("introduction")) ? group.introduction : previous.introduction;
    group.announcement = object.contains(QStringLiteral("announcement")) ? group.announcement : previous.announcement;
    group.currentUserNickname = object.contains(QStringLiteral("currentUserNickname"))
            ? group.currentUserNickname
            : previous.currentUserNickname;
    group.memberNicknames = object.contains(QStringLiteral("memberNicknames"))
            ? group.memberNicknames
            : previous.memberNicknames;
    group.membersID = object.contains(QStringLiteral("membersID")) || object.contains(QStringLiteral("members"))
            ? group.membersID
            : previous.membersID;
    group.listGroupId = object.contains(QStringLiteral("listGroupId")) ? group.listGroupId : previous.listGroupId;
    group.listGroupName = object.contains(QStringLiteral("listGroupName")) ? group.listGroupName : previous.listGroupName;
    group.role = object.contains(QStringLiteral("role")) ? group.role : previous.role;
    return group;
}

GroupMemberProfile groupMemberFromJson(QJsonObject object)
{
    const QJsonObject member = object.value(QStringLiteral("member")).toObject();
    if (!member.isEmpty()) {
        object = member;
    }

    GroupMemberProfile profile;
    profile.groupId = firstString(object, {QStringLiteral("groupId"),
                                           QStringLiteral("groupID")});
    profile.userUuid = firstString(object, {QStringLiteral("userUuid"),
                                            QStringLiteral("memberUserUuid"),
                                            QStringLiteral("uuid"),
                                            QStringLiteral("id"),
                                            QStringLiteral("userId")});
    profile.nickname = firstString(object, {QStringLiteral("nickname"),
                                            QStringLiteral("nickName"),
                                            QStringLiteral("groupNickname"),
                                            QStringLiteral("memberNickname")});
    profile.role = groupMemberRoleFromString(object.value(QStringLiteral("role")).toString());
    profile.isDnd = object.value(QStringLiteral("isDnd")).toBool(false);
    profile.joinedAt = dateTimeFromString(object.value(QStringLiteral("joinedAt")).toString());
    profile.version = object.value(QStringLiteral("version")).toInt();
    return profile;
}

QJsonObject groupMemberToJson(const GroupMemberProfile& profile)
{
    return {
            {QStringLiteral("groupId"), profile.groupId},
            {QStringLiteral("userUuid"), profile.userUuid},
            {QStringLiteral("nickname"), profile.nickname},
            {QStringLiteral("role"), groupMemberRoleToString(profile.role)},
            {QStringLiteral("isDnd"), profile.isDnd},
            {QStringLiteral("joinedAt"), profile.joinedAt.isValid()
                                          ? profile.joinedAt.toUTC().toString(Qt::ISODateWithMs)
                                          : QString()},
            {QStringLiteral("version"), profile.version}
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
                } else if (domain == kGroupCategoriesDomain) {
                    emit groupListChanged();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent& event) {
        if (type == QStringLiteral("group.deleted")) {
            const QString groupId = firstString(payload, {QStringLiteral("groupId"),
                                                          QStringLiteral("id")});
            if (!groupId.isEmpty()) {
                removeGroup(groupId);
            }
            return;
        }

        if (type == QStringLiteral("group.my_settings.updated")) {
            QJsonObject object = payload.value(QStringLiteral("group")).toObject();
            if (object.isEmpty()) {
                object = payload.value(QStringLiteral("settings")).toObject();
            }
            if (object.isEmpty()) {
                object = payload;
            }
            const QString groupId = firstString(object, {QStringLiteral("groupId"),
                                                         QStringLiteral("id")});
            Group previous = requestGroupDetail({groupId});
            if (previous.groupId.isEmpty()) {
                previous = groupFromJson(object);
            }
            if (previous.groupId.isEmpty()) {
                return;
            }
            if (object.contains(QStringLiteral("remark"))) {
                previous.remark = object.value(QStringLiteral("remark")).toString();
            }
            if (object.contains(QStringLiteral("listGroupId"))) {
                previous.listGroupId = object.value(QStringLiteral("listGroupId")).toString();
            }
            if (object.contains(QStringLiteral("listGroupName"))) {
                previous.listGroupName = object.value(QStringLiteral("listGroupName")).toString();
            }
            if (object.contains(QStringLiteral("isDnd"))) {
                previous.isDnd = object.value(QStringLiteral("isDnd")).toBool(previous.isDnd);
            }
            if (object.contains(QStringLiteral("role"))) {
                previous.role = object.value(QStringLiteral("role")).toString(previous.role);
            }
            saveGroup(previous);
            return;
        }

        if (type == QStringLiteral("group.members.added")) {
            const QString groupId = firstString(payload, {QStringLiteral("groupId"),
                                                          QStringLiteral("id")});
            QJsonArray members = payload.value(QStringLiteral("members")).toArray();
            if (members.isEmpty() && payload.value(QStringLiteral("member")).isObject()) {
                members.append(payload.value(QStringLiteral("member")));
            }
            for (const QJsonValue& value : members) {
                QJsonObject object = value.toObject();
                if (!groupId.isEmpty() && !object.contains(QStringLiteral("groupId"))) {
                    object.insert(QStringLiteral("groupId"), groupId);
                }
                upsertGroupMember(object);
            }
            return;
        }

        if (type == QStringLiteral("group.member.updated")) {
            QJsonObject object = payload.value(QStringLiteral("member")).toObject();
            if (object.isEmpty()) {
                object = payload;
            }
            QString groupId = payload.value(QStringLiteral("groupId")).toString();
            if (groupId.isEmpty()) {
                groupId = payload.value(QStringLiteral("group")).toObject()
                                  .value(QStringLiteral("groupId")).toString();
            }
            if (!groupId.isEmpty() && !object.contains(QStringLiteral("groupId"))) {
                object.insert(QStringLiteral("groupId"), groupId);
            }
            upsertGroupMember(object);
            return;
        }

        if (type == QStringLiteral("group.member.removed")) {
            const QString groupId = firstString(payload, {QStringLiteral("groupId"),
                                                          QStringLiteral("id")});
            const QString userId = firstString(payload, {QStringLiteral("userUuid"),
                                                         QStringLiteral("memberUserUuid"),
                                                         QStringLiteral("removedUserUuid"),
                                                         QStringLiteral("userId")});
            if (!groupId.isEmpty() && !userId.isEmpty()) {
                if (CurrentUser::instance().isCurrentUserId(userId)) {
                    removeGroup(groupId);
                } else {
                    removeMember(groupId, userId);
                }
            }
            return;
        }

        if (type == QStringLiteral("group.bot.created")) {
            QJsonObject agent = payload.value(QStringLiteral("agent")).toObject();
            if (agent.isEmpty()) {
                agent = payload;
            }
            QString groupId = firstString(payload, {QStringLiteral("groupId"),
                                                    QStringLiteral("groupID")});
            if (groupId.isEmpty()) {
                groupId = firstString(event.raw, {QStringLiteral("groupId"),
                                                  QStringLiteral("groupID"),
                                                  QStringLiteral("aggregateId"),
                                                  QStringLiteral("subjectId")});
            }
            const QString botUserId = firstString(agent, {QStringLiteral("botUserId"),
                                                          QStringLiteral("bot_user_id"),
                                                          QStringLiteral("userUuid"),
                                                          QStringLiteral("user_uuid")});
            if (groupId.isEmpty() || botUserId.isEmpty()) {
                return;
            }

            QJsonObject user{
                    {QStringLiteral("userUuid"), botUserId},
                    {QStringLiteral("id"), botUserId},
                    {QStringLiteral("userId"), firstString(agent, {QStringLiteral("userId"),
                                                                   QStringLiteral("publicId"),
                                                                   QStringLiteral("agentId"),
                                                                   QStringLiteral("agent_id")})},
                    {QStringLiteral("nick"), firstString(agent, {QStringLiteral("name"),
                                                                 QStringLiteral("nick"),
                                                                 QStringLiteral("displayName")})},
                    {QStringLiteral("isAi"), true},
                    {QStringLiteral("kind"), firstString(agent, {QStringLiteral("kind"),
                                                                 QStringLiteral("agentKind")})},
                    {QStringLiteral("agentId"), firstString(agent, {QStringLiteral("agentId"),
                                                                    QStringLiteral("agent_id")})},
                    {QStringLiteral("aiStatus"), agent.value(QStringLiteral("status")).toString()}
            };
            const QString avatarFileId = firstString(agent, {QStringLiteral("avatarFileId"),
                                                             QStringLiteral("avatar_file_id"),
                                                             QStringLiteral("fileId")});
            if (!avatarFileId.isEmpty()) {
                user.insert(QStringLiteral("avatarFileId"), avatarFileId);
            }

            upsertGroupMember(QJsonObject{
                    {QStringLiteral("groupId"), groupId},
                    {QStringLiteral("userUuid"), botUserId},
                    {QStringLiteral("role"), QStringLiteral("ai")},
                    {QStringLiteral("user"), user}
            });
            return;
        }

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

        upsertGroup(object);
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

    QMap<QString, GroupMemberProfile> nextMembers;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("group_members"))) {
        const GroupMemberProfile member = groupMemberFromJson(object);
        const QString key = groupMemberKey(member.groupId, member.userUuid);
        if (!key.isEmpty()) {
            nextMembers.insert(key, member);
            Group& group = nextGroups[member.groupId];
            group.groupId = member.groupId;
            if (!group.membersID.contains(member.userUuid)) {
                group.membersID.push_back(member.userUuid);
            }
            if (!member.nickname.isEmpty()) {
                group.memberNicknames.insert(member.userUuid, member.nickname);
            }
            if (member.role == GroupMemberRoleValue::Owner) {
                group.ownerId = member.userUuid;
            } else if (member.role == GroupMemberRoleValue::Admin &&
                       !group.adminsID.contains(member.userUuid)) {
                group.adminsID.push_back(member.userUuid);
            } else if (member.role == GroupMemberRoleValue::Ai) {
                group.adminsID.removeAll(member.userUuid);
            }
            if (group.memberNum <= 0) {
                group.memberNum = group.membersID.size();
            }
        }
    }

    {
        QMutexLocker locker(&mutex);
        groupMap = nextGroups;
        groupMemberMap = nextMembers;
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
    const QVector<Group> groups = QVector<Group>::fromList(groupMap.values());
    locker.unlock();
    return GroupCategoryListRequestOperation(groups, requestGroupCategories()).request(query);
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
    QMap<QString, QString> categories;
    categories.insert(kJoinedCategoryId, kJoinedCategoryName);

    for (const QJsonObject& object : LocalDataStore::instance().values(kGroupCategoriesDomain)) {
        const QString categoryId = firstString(object, {QStringLiteral("categoryId"),
                                                        QStringLiteral("listGroupId"),
                                                        QStringLiteral("id")});
        const QString categoryName = firstString(object, {QStringLiteral("categoryName"),
                                                          QStringLiteral("listGroupName"),
                                                          QStringLiteral("name")});
        if (!categoryId.isEmpty() && categoryId != kJoinedCategoryId && !categoryName.isEmpty()) {
            categories.insert(categoryId, categoryName);
        }
    }

    QMutexLocker locker(&mutex);
    for (const Group& group : groupMap) {
        if (!group.listGroupId.isEmpty() && !group.listGroupName.isEmpty()) {
            categories.insert(group.listGroupId, group.listGroupName);
        }
    }
    return categories;
}

bool GroupRepository::upsertGroupCategory(const QString& categoryId, const QString& categoryName)
{
    const QString normalizedId = categoryId.trimmed();
    const QString normalizedName = categoryName.trimmed();
    if (normalizedId.isEmpty() || normalizedId == kJoinedCategoryId || normalizedName.isEmpty()) {
        return false;
    }

    return LocalDataStore::instance().upsertValue(kGroupCategoriesDomain,
                                                  normalizedId,
                                                  groupCategoryToJson(normalizedId, normalizedName));
}

bool GroupRepository::removeGroupCategory(const QString& categoryId)
{
    if (categoryId.isEmpty() || categoryId == kJoinedCategoryId) {
        return false;
    }
    return LocalDataStore::instance().removeValue(kGroupCategoriesDomain, categoryId);
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

GroupMemberProfile GroupRepository::requestGroupMember(const QString& groupId, const QString& userId) const
{
    QMutexLocker locker(&mutex);
    return groupMemberMap.value(groupMemberKey(groupId, userId), GroupMemberProfile{});
}

QString GroupRepository::requestGroupMemberNickname(const QString& groupId, const QString& userId) const
{
    return requestGroupMember(groupId, userId).nickname;
}

int GroupRepository::requestGroupMemberVersion(const QString& groupId, const QString& userId) const
{
    return requestGroupMember(groupId, userId).version;
}

bool GroupRepository::upsertGroup(const QJsonObject& object)
{
    Group group = groupFromJson(object);
    if (group.groupId.isEmpty()) {
        return false;
    }

    const Group previous = requestGroupDetail({group.groupId});
    if (!previous.groupId.isEmpty()) {
        group = mergedGroupFromObject(object, previous);
    }
    if (group.groupId.isEmpty()) {
        return false;
    }
    if (!previous.groupId.isEmpty() && groupToJson(previous) == groupToJson(group)) {
        return false;
    }

    saveGroup(group);
    return true;
}

void GroupRepository::saveGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return;
    }

    const QString oldAvatarPath = requestGroupAvatarPath(group.groupId);
    QMutexLocker locker(&mutex);
    const Group previous = groupMap.value(group.groupId);
    if (!previous.groupId.isEmpty() && groupToJson(previous) == groupToJson(group)) {
        return;
    }
    groupMap[group.groupId] = group;
    locker.unlock();
    LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
    if (!oldAvatarPath.isEmpty() && oldAvatarPath != group.groupAvatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    emit groupListChanged();
}

bool GroupRepository::upsertGroupMember(const QJsonObject& object)
{
    QJsonObject memberObject = object.value(QStringLiteral("member")).toObject();
    if (memberObject.isEmpty()) {
        memberObject = object;
    }

    const QJsonObject userObject = memberObject.value(QStringLiteral("user")).toObject();
    if (!userObject.isEmpty()) {
        QJsonObject normalizedUserObject = userObject;
        const QString role = memberObject.value(QStringLiteral("role")).toString().trimmed().toLower();
        if (role == QStringLiteral("ai")) {
            normalizedUserObject.insert(QStringLiteral("isAi"), true);
            if (!normalizedUserObject.contains(QStringLiteral("kind"))) {
                normalizedUserObject.insert(QStringLiteral("kind"), QStringLiteral("group_bot"));
            }
        }
        UserRepository::instance().upsertUserProfile(normalizedUserObject);
        if (!memberObject.contains(QStringLiteral("userUuid"))) {
            memberObject.insert(QStringLiteral("userUuid"),
                                firstString(normalizedUserObject, {QStringLiteral("userUuid"),
                                                                   QStringLiteral("id"),
                                                                   QStringLiteral("userId")}));
        }
    }

    GroupMemberProfile member = groupMemberFromJson(memberObject);
    const QString key = groupMemberKey(member.groupId, member.userUuid);
    if (key.isEmpty()) {
        return false;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        const GroupMemberProfile previous = groupMemberMap.value(key);
        if (previous.version > 0 && member.version > 0 && member.version < previous.version) {
            return false;
        }

        changed = previous.groupId != member.groupId ||
                  previous.userUuid != member.userUuid ||
                  previous.nickname != member.nickname ||
                  previous.role != member.role ||
                  previous.isDnd != member.isDnd ||
                  previous.joinedAt != member.joinedAt ||
                  previous.version != member.version;
        groupMemberMap.insert(key, member);

        Group group = groupMap.value(member.groupId);
        if (group.groupId.isEmpty()) {
            group.groupId = member.groupId;
        }
        if (!group.membersID.contains(member.userUuid)) {
            group.membersID.push_back(member.userUuid);
            changed = true;
        }
        if (!member.nickname.isEmpty() &&
            group.memberNicknames.value(member.userUuid) != member.nickname) {
            group.memberNicknames.insert(member.userUuid, member.nickname);
            changed = true;
        }
        if (member.role == GroupMemberRoleValue::Owner &&
            group.ownerId != member.userUuid) {
            group.ownerId = member.userUuid;
            changed = true;
        }
        const bool shouldBeAdmin = member.role == GroupMemberRoleValue::Admin;
        if (shouldBeAdmin && !group.adminsID.contains(member.userUuid)) {
            group.adminsID.push_back(member.userUuid);
            changed = true;
        } else if (!shouldBeAdmin && group.adminsID.removeAll(member.userUuid) > 0) {
            changed = true;
        }
        if (group.memberNum <= 0 || group.memberNum < group.membersID.size()) {
            group.memberNum = group.membersID.size();
            changed = true;
        }
        groupMap.insert(group.groupId, group);
    }

    LocalDataStore::instance().upsertValue(QStringLiteral("group_members"), key, groupMemberToJson(member));
    const Group group = requestGroupDetail({member.groupId});
    if (!group.groupId.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("groups"), group.groupId, groupToJson(group));
    }
    if (changed) {
        emit groupListChanged();
    }
    return true;
}

bool GroupRepository::needsGroupMemberRefresh(const QString& groupId,
                                              const QString& userId,
                                              int remoteVersion) const
{
    const GroupMemberProfile member = requestGroupMember(groupId, userId);
    if (member.userUuid.isEmpty()) {
        return true;
    }
    return remoteVersion > 0 && (member.version <= 0 || member.version < remoteVersion);
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
