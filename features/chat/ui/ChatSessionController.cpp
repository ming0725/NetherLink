#include "features/chat/ui/ChatSessionController.h"

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/GroupRemoteDataSource.h"
#include "features/chat/data/ConversationRemoteDataSource.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendRemoteDataSource.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"

#include <QCollator>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {
constexpr int kPanelMemberPreviewLimit = 5;

QString memberNickname(const Group& group, const QString& userId)
{
    const QString storedNickname = group.memberNicknames.value(userId).trimmed();
    if (!storedNickname.isEmpty()) {
        return storedNickname;
    }
    if (CurrentUser::instance().isCurrentUserId(userId) && !group.currentUserNickname.trimmed().isEmpty()) {
        return group.currentUserNickname.trimmed();
    }
    return {};
}

QString currentUserUuid()
{
    const CurrentUserProfile profile = CurrentUser::instance().identity();
    return profile.userUuid.isEmpty() ? CurrentUser::instance().getUserId() : profile.userUuid;
}

QString memberDisplayName(const Group& group, const User& user)
{
    const QString groupNickname = memberNickname(group, user.id);
    if (!groupNickname.isEmpty()) {
        return groupNickname;
    }
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(user.id)) {
        return currentUser.getUserName();
    }
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    return user.nick.trimmed().isEmpty() ? user.id : user.nick.trimmed();
}

GroupRole memberRole(const Group& group, const QString& userId)
{
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        const QString currentRole = group.role.trimmed().toLower();
        if (currentRole == QStringLiteral("owner")) {
            return GroupRole::Owner;
        }
        if (currentRole == QStringLiteral("admin")) {
            return GroupRole::Admin;
        }
    }
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

QString groupMemberRoleToString(GroupMemberRoleValue role)
{
    switch (role) {
    case GroupMemberRoleValue::Owner:
        return QStringLiteral("owner");
    case GroupMemberRoleValue::Admin:
        return QStringLiteral("admin");
    case GroupMemberRoleValue::Member:
    default:
        return QStringLiteral("member");
    }
}

QJsonObject userToProfileJson(const User& user)
{
    return {
            {QStringLiteral("id"), user.id},
            {QStringLiteral("userUuid"), user.userUuid.isEmpty() ? user.id : user.userUuid},
            {QStringLiteral("userId"), user.userId},
            {QStringLiteral("nick"), user.nick},
            {QStringLiteral("remark"), user.remark},
            {QStringLiteral("avatarPath"), user.avatarPath},
            {QStringLiteral("avatarVersion"), user.avatarVersion},
            {QStringLiteral("avatarEtag"), user.avatarEtag},
            {QStringLiteral("avatarContentHash"), user.avatarContentHash},
            {QStringLiteral("version"), user.version},
            {QStringLiteral("etag"), user.etag},
            {QStringLiteral("signature"), user.signature},
            {QStringLiteral("isDnd"), user.isDnd},
            {QStringLiteral("region"), user.region}
    };
}

QJsonObject groupMemberToProfileJson(const GroupMemberProfile& member)
{
    QJsonObject object{
            {QStringLiteral("groupId"), member.groupId},
            {QStringLiteral("userUuid"), member.userUuid},
            {QStringLiteral("nickname"), member.nickname},
            {QStringLiteral("role"), groupMemberRoleToString(member.role)},
            {QStringLiteral("isDnd"), member.isDnd},
            {QStringLiteral("joinedAt"), member.joinedAt.isValid()
                                      ? member.joinedAt.toUTC().toString(Qt::ISODateWithMs)
                                      : QString()},
            {QStringLiteral("version"), member.version}
    };
    if (!member.user.id.isEmpty() || !member.user.userUuid.isEmpty()) {
        object.insert(QStringLiteral("user"), userToProfileJson(member.user));
    }
    return object;
}

void persistGroupMemberProfile(const GroupMemberProfile& member)
{
    if (!member.user.id.isEmpty() || !member.user.userUuid.isEmpty()) {
        UserRepository::instance().upsertUserProfile(userToProfileJson(member.user));
    }
    GroupRepository::instance().upsertGroupMember(groupMemberToProfileJson(member));
}

GroupRole memberRole(const Group& group, const User& user)
{
    return memberRole(group, user.id);
}

QVector<User> sortedGroupMembers(const Group& group,
                                 const QVector<User>& members,
                                 const QString& keyword = {})
{
    QVector<User> filtered;
    filtered.reserve(members.size());
    const QString normalizedKeyword = keyword.trimmed();
    for (const User& user : members) {
        const QString displayName = memberDisplayName(group, user);
        if (!normalizedKeyword.isEmpty() &&
            !displayName.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.nick.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.remark.contains(normalizedKeyword, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.push_back(user);
    }

    QCollator collator(QLocale::Chinese);
    collator.setNumericMode(true);
    std::sort(filtered.begin(), filtered.end(), [&group, &collator](const User& lhs, const User& rhs) {
        const GroupRole lhsRole = memberRole(group, lhs);
        const GroupRole rhsRole = memberRole(group, rhs);
        const int lhsRank = lhsRole == GroupRole::Owner ? 0 : (lhsRole == GroupRole::Admin ? 1 : 2);
        const int rhsRank = rhsRole == GroupRole::Owner ? 0 : (rhsRole == GroupRole::Admin ? 1 : 2);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        const int nameOrder = collator.compare(memberDisplayName(group, lhs), memberDisplayName(group, rhs));
        return nameOrder == 0 ? lhs.id < rhs.id : nameOrder < 0;
    });
    return filtered;
}

QVector<User> requestGroupMembers(const Group& group)
{
    QVector<User> members;
    members.reserve(group.membersID.size());
    for (const QString& memberId : group.membersID) {
        User user;
        const CurrentUser& currentUser = CurrentUser::instance();
        if (currentUser.isCurrentUserId(memberId)) {
            const CurrentUserProfile profile = currentUser.identity();
            user.id = profile.userId;
            user.nick = profile.nickName;
            user.avatarPath = profile.avatarPath;
            user.status = profile.status;
            user.isFriend = false;
        } else {
            user = UserRepository::instance().requestUserDetail({memberId});
        }
        if (!user.id.isEmpty()) {
            members.push_back(user);
        }
    }
    return members;
}

QString memberDisplayNameForId(const Group& group,
                               const QString& userId,
                               const QHash<QString, User>& usersById = {})
{
    if (userId.isEmpty()) {
        return {};
    }

    const auto cachedUser = usersById.constFind(userId);
    if (cachedUser != usersById.cend()) {
        return memberDisplayName(group, cachedUser.value());
    }

    User user;
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        const CurrentUserProfile profile = currentUser.identity();
        user.id = profile.userId;
        user.nick = profile.nickName;
        user.avatarPath = profile.avatarPath;
        user.status = profile.status;
    } else {
        user = UserRepository::instance().requestUserDetail({userId});
    }
    if (user.id.isEmpty()) {
        return group.memberNicknames.value(userId, userId);
    }
    return memberDisplayName(group, user);
}

void appendPreviewMember(QVector<User>& members, QSet<QString>& seen, const QString& userId)
{
    if (userId.isEmpty() || seen.contains(userId) || members.size() >= kPanelMemberPreviewLimit) {
        return;
    }

    User user;
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        const CurrentUserProfile profile = currentUser.identity();
        user.id = profile.userId;
        user.nick = profile.nickName;
        user.avatarPath = profile.avatarPath;
        user.status = profile.status;
        user.isFriend = false;
    } else {
        user = UserRepository::instance().requestUserDetail({userId});
    }
    if (user.id.isEmpty()) {
        return;
    }

    members.push_back(user);
    seen.insert(user.id);
}

QVector<User> requestGroupMemberPreview(const Group& group)
{
    QVector<User> members;
    QSet<QString> seen;
    members.reserve(kPanelMemberPreviewLimit);
    appendPreviewMember(members, seen, group.ownerId);
    for (const QString& adminId : group.adminsID) {
        appendPreviewMember(members, seen, adminId);
    }
    for (const QString& memberId : group.membersID) {
        appendPreviewMember(members, seen, memberId);
    }
    return sortedGroupMembers(group, members);
}
} // namespace

ChatSessionController::ChatSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&MessageRepository::instance(), &MessageRepository::conversationListChanged,
            this, [this](const QString& changedConversationId) {
                if (hasCurrentConversation(changedConversationId)) {
                    refreshSessionData(true);
                }
            });
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, [this]() {
                if (!m_meta.conversationId.isEmpty() && m_meta.isGroup) {
                    refreshSessionData(true);
                }
            });
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, [this]() {
                if (!m_meta.conversationId.isEmpty() && !m_meta.isGroup) {
                    refreshSessionData(true);
                }
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendUpdated,
            this,
            [this](const QString&, const User& user) {
                if (m_meta.isGroup || !hasCurrentConversation(user.id)) {
                    return;
                }
                refreshSessionData(true);
            },
            Qt::QueuedConnection);
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendDeleted,
            this,
            [this](const QString&, const QString& userId) {
                if (m_meta.isGroup || !hasCurrentConversation(userId)) {
                    return;
                }
                UserRepository::instance().removeUser(userId);
                refreshSessionData(true);
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendUpdateFailed,
            this,
            [this](const QString&, const QString& userId, const NetworkError& error) {
                if (!m_meta.isGroup && hasCurrentConversation(userId)) {
                    emit friendUpdateFailed(userId, error);
                }
            });
    connect(&FriendRemoteDataSource::instance(),
            &FriendRemoteDataSource::friendDeleteFailed,
            this,
            [this](const QString&, const QString& userId, const NetworkError& error) {
                if (!m_meta.isGroup && hasCurrentConversation(userId)) {
                    emit friendDeleteFailed(userId, error);
                }
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupUpdated,
            this,
            [this](const QString&, const Group& group) {
                if (!m_meta.isGroup || !hasCurrentConversation(group.groupId)) {
                    return;
                }
                GroupRepository::instance().saveGroup(group);
                refreshSessionData(true);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupFetched,
            this,
            [this](const QString& requestId, const Group& group) {
                if (requestId != m_panelGroupRequestId ||
                    !m_meta.isGroup ||
                    !hasCurrentConversation(group.groupId)) {
                    return;
                }

                m_panelGroupRequestId.clear();
                GroupRepository::instance().saveGroup(group);
                m_group = group;
                m_meta = MessageRepository::instance().requestConversationMeta({m_meta.conversationId});
                emit groupPanelDataLoaded(m_meta,
                                          m_group,
                                          m_panelPreviewMembers,
                                          m_group.memberNum,
                                          canEditGroupInfo(m_group),
                                          canExitGroup());
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMySettingsUpdated,
            this,
            [this](const QString&, const Group& group) {
                if (!m_meta.isGroup || !hasCurrentConversation(group.groupId)) {
                    return;
                }
                GroupRepository::instance().saveGroup(group);
                refreshSessionData(true);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupLeft,
            this,
            [this](const QString&, const QString& groupId) {
                if (!m_meta.isGroup || !hasCurrentConversation(groupId)) {
                    return;
                }
                MessageRepository::instance().removeConversation(m_meta.conversationId);
                GroupRepository::instance().removeGroup(groupId);
                close();
                emit conversationRemoved();
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupUpdateFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                if (m_meta.isGroup && hasCurrentConversation(groupId)) {
                    emit groupUpdateFailed(groupId, error);
                }
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMySettingsUpdateFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                if (m_meta.isGroup && hasCurrentConversation(groupId)) {
                    emit groupMySettingsUpdateFailed(groupId, error);
                }
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupLeaveFailed,
            this,
            [this](const QString&, const QString& groupId, const NetworkError& error) {
                if (m_meta.isGroup && hasCurrentConversation(groupId)) {
                    emit groupLeaveFailed(groupId, error);
                }
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMembersFetched,
            this,
            [this](const QString& requestId,
                   const QString& groupId,
                   const QString& keyword,
                   int offset,
                   int limit,
                   const QVector<GroupMemberProfile>& members,
                   int totalCount,
                   bool hasMore) {
                if (!m_meta.isGroup || !hasCurrentConversation(groupId)) {
                    return;
                }

                for (const GroupMemberProfile& member : members) {
                    persistGroupMemberProfile(member);
                }

                if (requestId == m_panelPreviewMembersRequestId) {
                    m_panelPreviewMembersRequestId.clear();
                    m_panelPreviewMembers = members.mid(0, limit);
                    Group group = GroupRepository::instance().requestGroupDetail({groupId});
                    if (group.groupId.isEmpty()) {
                        group = m_group;
                    }
                    if (totalCount > 0) {
                        group.memberNum = totalCount;
                    }
                    m_group = group;
                    m_meta = MessageRepository::instance().requestConversationMeta({m_meta.conversationId});
                    emit groupPanelDataLoaded(m_meta,
                                              m_group,
                                              m_panelPreviewMembers,
                                              qMax(totalCount, m_group.memberNum),
                                              canEditGroupInfo(m_group),
                                              canExitGroup());
                    return;
                }

                if (requestId != m_memberPageRequestId) {
                    return;
                }

                GroupMembersPage page;
                page.groupId = groupId;
                page.keyword = keyword;
                page.offset = offset;
                page.totalCount = totalCount;
                page.members = members.mid(0, limit);
                page.hasMore = hasMore;
                m_memberPageRequestId.clear();
                emit groupMembersPageLoaded(m_meta, page);
            });
    connect(&GroupRemoteDataSource::instance(),
            &GroupRemoteDataSource::groupMembersFetchFailed,
            this,
            [this](const QString& requestId,
                   const QString& groupId,
                   const QString& keyword,
                   int offset,
                   int,
                   const NetworkError&) {
                if (requestId == m_panelPreviewMembersRequestId &&
                    m_meta.isGroup &&
                    hasCurrentConversation(groupId)) {
                    m_panelPreviewMembersRequestId.clear();
                    m_panelPreviewMembers.clear();
                    emit groupPanelDataLoaded(m_meta,
                                              m_group,
                                              m_panelPreviewMembers,
                                              m_group.memberNum,
                                              canEditGroupInfo(m_group),
                                              canExitGroup());
                    return;
                }

                if (requestId != m_memberPageRequestId ||
                    !m_meta.isGroup ||
                    !hasCurrentConversation(groupId)) {
                    return;
                }

                GroupMembersPage page;
                page.groupId = groupId;
                page.keyword = keyword;
                page.offset = offset;
                page.totalCount = offset;
                page.hasMore = false;
                m_memberPageRequestId.clear();
                emit groupMembersPageLoaded(m_meta, page);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::conversationFetched,
            this,
            [this](const QString& requestId, const QString& conversationId) {
                if (requestId != m_panelConversationRequestId ||
                    !hasCurrentConversation(conversationId)) {
                    return;
                }

                m_panelConversationRequestId.clear();
                m_meta = MessageRepository::instance().requestConversationMeta({m_meta.conversationId});
                if (m_meta.isGroup) {
                    emit groupPanelDataLoaded(m_meta,
                                              m_group,
                                              m_panelPreviewMembers,
                                              m_group.memberNum,
                                              canEditGroupInfo(m_group),
                                              canExitGroup());
                } else {
                    emit directPanelDataLoaded(m_meta, m_directUser);
                }
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::pinnedUpdated,
            this,
            [this](const QString&, const QString& conversationId, bool pinned) {
                if (!hasCurrentConversation(conversationId)) {
                    return;
                }
                m_meta.isPinned = pinned;
                refreshSessionData(true);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::doNotDisturbUpdated,
            this,
            [this](const QString&, const QString& conversationId, bool enabled) {
                if (!hasCurrentConversation(conversationId)) {
                    return;
                }
                m_meta.isDoNotDisturb = enabled;
                refreshSessionData(true);
            });
}

void ChatSessionController::open(const ConversationMeta& meta)
{
    cancelPanelLoads();
    m_meta = meta;
    refreshSessionData(true);
}

void ChatSessionController::close()
{
    cancelPanelLoads();
    m_meta = {};
    m_directUser = {};
    m_group = {};
    emit sessionChanged(m_meta, m_directUser, m_group);
}

QString ChatSessionController::conversationId() const
{
    return m_meta.conversationId;
}

bool ChatSessionController::isGroup() const
{
    return m_meta.isGroup;
}

ConversationMeta ChatSessionController::meta() const
{
    return m_meta;
}

User ChatSessionController::directUser() const
{
    return m_directUser;
}

Group ChatSessionController::group() const
{
    return m_group;
}

bool ChatSessionController::canEditGroupInfo() const
{
    return canEditGroupInfo(m_group);
}

bool ChatSessionController::canExitGroup() const
{
    return !groupId().isEmpty();
}

void ChatSessionController::loadPanelData()
{
    if (m_meta.conversationId.isEmpty()) {
        return;
    }

    const int token = ++m_panelLoadToken;
    const ConversationMeta meta = m_meta;

    if (meta.isGroup) {
        const QString currentGroupId = groupId();
        if (currentGroupId.isEmpty()) {
            return;
        }

        m_panelPreviewMembers.clear();
        m_group = GroupRepository::instance().requestGroupDetail({currentGroupId});
        m_meta = MessageRepository::instance().requestConversationMeta({meta.conversationId});
        if (m_meta.conversationId.isEmpty()) {
            m_meta = meta;
        }
        emit groupPanelDataLoaded(m_meta,
                                  m_group,
                                  m_panelPreviewMembers,
                                  m_group.memberNum,
                                  canEditGroupInfo(m_group),
                                  canExitGroup());

        m_panelConversationRequestId = ConversationRemoteDataSource::instance().fetchConversation(meta.conversationId);
        m_panelGroupRequestId = GroupRemoteDataSource::instance().fetchGroup(currentGroupId);
        m_panelPreviewMembersRequestId = GroupRemoteDataSource::instance().fetchMembers(currentGroupId,
                                                                                       {},
                                                                                       0,
                                                                                       kPanelMemberPreviewLimit);
        Q_UNUSED(token);
        return;
    }

    QPointer<ChatSessionController> controller(this);

    QThread* thread = QThread::create([controller, token, meta]() {
        ConversationMeta loadedMeta = MessageRepository::instance().requestConversationMeta({meta.conversationId});
        if (loadedMeta.conversationId.isEmpty()) {
            loadedMeta = meta;
        }

        const QString directUserId = loadedMeta.peerUserId.isEmpty()
                ? meta.conversationId
                : loadedMeta.peerUserId;
        const User directUser = UserRepository::instance().requestUserDetail({directUserId});
        if (!controller) {
            return;
        }
        QMetaObject::invokeMethod(controller.data(), [controller, token, loadedMeta, directUser]() {
            if (!controller || token != controller->m_panelLoadToken ||
                loadedMeta.conversationId != controller->m_meta.conversationId) {
                return;
            }

            controller->m_meta = loadedMeta;
            controller->m_directUser = directUser;
            controller->m_group = {};
            emit controller->directPanelDataLoaded(loadedMeta, directUser);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ChatSessionController::loadGroupMembersPage(const QString& keyword, int offset, int limit)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || limit <= 0) {
        return;
    }

    const QString normalizedKeyword = keyword.trimmed();
    const int safeOffset = qMax(0, offset);
    const int safeLimit = qMax(1, limit);
    ++m_memberPageLoadToken;
    m_memberPageRequestId = GroupRemoteDataSource::instance().fetchMembers(currentGroupId,
                                                                           normalizedKeyword,
                                                                           safeOffset,
                                                                           safeLimit);
}

void ChatSessionController::cancelPanelLoads()
{
    ++m_panelLoadToken;
    ++m_memberPageLoadToken;
    m_panelPreviewMembers.clear();
    m_memberPageRequestId.clear();
    m_panelConversationRequestId.clear();
    m_panelGroupRequestId.clear();
    m_panelPreviewMembersRequestId.clear();
}

void ChatSessionController::saveGroupName(const QString& name)
{
    saveGroupField(name, [](Group& group, const QString& value) {
        group.groupName = value;
    });
}

void ChatSessionController::saveGroupIntroduction(const QString& introduction)
{
    saveGroupField(introduction, [](Group& group, const QString& value) {
        group.introduction = value;
    });
}

void ChatSessionController::saveGroupAnnouncement(const QString& announcement)
{
    saveGroupField(announcement, [](Group& group, const QString& value) {
        group.announcement = value;
    });
}

void ChatSessionController::saveCurrentUserGroupNickname(const QString& nickname)
{
    saveGroupMemberNickname(currentUserUuid(), nickname);
}

void ChatSessionController::saveGroupMemberNickname(const QString& userId, const QString& nickname)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canEditMemberNickname(group, userId)) {
        return;
    }

    const QString nextNickname = nickname.trimmed();
    const QString previousNickname = memberNickname(group, userId);
    if (previousNickname == nextNickname &&
        (!CurrentUser::instance().isCurrentUserId(userId) || group.currentUserNickname == nextNickname)) {
        return;
    }

    if (CurrentUser::instance().isCurrentUserId(userId)) {
        group.currentUserNickname = nextNickname;
    }
    GroupRemoteDataSource::instance().updateMemberNickname(group, userId, nextNickname);
}

void ChatSessionController::promoteGroupMemberToAdmin(const QString& userId)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canPromoteMemberToAdmin(group, userId)) {
        return;
    }

    if (!group.membersID.contains(userId)) {
        return;
    }

    GroupRemoteDataSource::instance().setMemberAdmin(group, userId, true);
}

void ChatSessionController::cancelGroupMemberAdmin(const QString& userId)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canCancelMemberAdmin(group, userId)) {
        return;
    }

    GroupRemoteDataSource::instance().setMemberAdmin(group, userId, false);
}

void ChatSessionController::inviteGroupMembers(const QStringList& userIds)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userIds.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canEditGroupInfo(group)) {
        return;
    }

    QSet<QString> existingMemberIds;
    existingMemberIds.reserve(group.membersID.size());
    for (const QString& memberId : std::as_const(group.membersID)) {
        if (!memberId.isEmpty()) {
            existingMemberIds.insert(memberId);
        }
    }

    QStringList newUserIds;
    QSet<QString> pendingUserIds;
    for (const QString& userId : userIds) {
        if (userId.isEmpty() || existingMemberIds.contains(userId) || pendingUserIds.contains(userId)) {
            continue;
        }
        pendingUserIds.insert(userId);
        newUserIds.push_back(userId);
    }

    if (newUserIds.isEmpty()) {
        return;
    }

    QHash<QString, User> usersById;
    for (const User& user : UserRepository::instance().requestUserDetails(newUserIds)) {
        usersById.insert(user.id, user);
    }

    for (const QString& userId : std::as_const(newUserIds)) {
        const User user = usersById.value(userId);
        const QString displayName = user.nick.trimmed().isEmpty() ? userId : user.nick.trimmed();
        group.memberNicknames.insert(userId, displayName);
    }

    GroupRemoteDataSource::instance().addMembers(group, newUserIds);
}

void ChatSessionController::removeGroupMember(const QString& userId)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canRemoveMember(group, userId)) {
        return;
    }

    GroupRemoteDataSource::instance().removeMember(group, userId);
}

void ChatSessionController::removeGroupMembers(const QStringList& userIds)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userIds.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty()) {
        return;
    }

    QSet<QString> removableIds;
    for (const QString& userId : userIds) {
        if (userId.isEmpty() || removableIds.contains(userId) || !canRemoveMember(group, userId)) {
            continue;
        }
        removableIds.insert(userId);
    }

    if (removableIds.isEmpty()) {
        return;
    }

    QStringList removableList;
    removableList.reserve(removableIds.size());
    for (const QString& userId : std::as_const(removableIds)) {
        removableList.push_back(userId);
    }
    GroupRemoteDataSource::instance().removeMembers(group, removableList);
}

void ChatSessionController::transferGroupOwner(const QString& userId)
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty() || !canTransferOwner(group, userId)) {
        return;
    }

    GroupRemoteDataSource::instance().transferOwner(group, userId);
}

void ChatSessionController::saveGroupRemark(const QString& remark)
{
    saveGroupField(remark, [](Group& group, const QString& value) {
        group.remark = value;
    });
}

void ChatSessionController::saveDirectRemark(const QString& remark)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isGroup) {
        return;
    }

    const QString directUserId = m_meta.peerUserId.isEmpty()
            ? m_meta.conversationId
            : m_meta.peerUserId;
    User user = UserRepository::instance().requestUserDetail({directUserId});
    const QString nextRemark = remark.trimmed();
    if (user.id.isEmpty() || user.remark == nextRemark) {
        return;
    }

    user.remark = nextRemark;
    FriendRemoteDataSource::instance().updateFriend(user);
}

void ChatSessionController::setPinned(bool pinned)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isPinned == pinned) {
        return;
    }

    ConversationRemoteDataSource::instance().setPinned(m_meta.conversationId, pinned);
}

void ChatSessionController::setDoNotDisturb(bool enabled)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isDoNotDisturb == enabled) {
        return;
    }

    ConversationRemoteDataSource::instance().setDoNotDisturb(m_meta.conversationId, enabled);
}

void ChatSessionController::clearMessages()
{
    if (m_meta.conversationId.isEmpty()) {
        return;
    }

    MessageRepository::instance().clearConversationMessages(m_meta.conversationId);
    emit messagesCleared();
    ConversationRemoteDataSource::instance().clearMessages(m_meta.conversationId);
}

void ChatSessionController::deleteFriend()
{
    if (m_meta.conversationId.isEmpty() || m_meta.isGroup) {
        return;
    }

    const QString friendId = m_meta.peerUserId.isEmpty()
            ? m_meta.conversationId
            : m_meta.peerUserId;
    FriendRemoteDataSource::instance().deleteFriend(friendId);
}

void ChatSessionController::exitGroup()
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty()) {
        return;
    }

    GroupRemoteDataSource::instance().leaveGroup(currentGroupId, currentUserUuid());
}

void ChatSessionController::refreshSessionData(bool emitChange)
{
    if (m_meta.conversationId.isEmpty()) {
        m_directUser = {};
        m_group = {};
    } else {
        m_meta = MessageRepository::instance().requestConversationMeta({m_meta.conversationId});
        m_group = {};
        m_directUser = {};
    }

    if (emitChange) {
        emit sessionChanged(m_meta, m_directUser, m_group);
    }
}

bool ChatSessionController::hasCurrentConversation(const QString& changedConversationId) const
{
    return !changedConversationId.isEmpty() &&
           (changedConversationId == m_meta.conversationId ||
            (m_meta.isGroup &&
             !m_meta.groupId.isEmpty() &&
             changedConversationId == m_meta.groupId) ||
            (!m_meta.isGroup &&
             !m_meta.peerUserId.isEmpty() &&
             changedConversationId == m_meta.peerUserId));
}

QString ChatSessionController::groupId() const
{
    if (!m_meta.isGroup) {
        return {};
    }
    return m_meta.groupId.isEmpty() ? m_meta.conversationId : m_meta.groupId;
}

bool ChatSessionController::canEditGroupInfo(const Group& group) const
{
    return !group.groupId.isEmpty() &&
           (GroupRepository::instance().isCurrentUserGroupOwner(group) ||
            GroupRepository::instance().isCurrentUserGroupAdmin(group));
}

bool ChatSessionController::canEditMemberNickname(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = currentUserUuid();
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return true;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

bool ChatSessionController::canPromoteMemberToAdmin(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = currentUserUuid();
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Member;
}

bool ChatSessionController::canCancelMemberAdmin(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = currentUserUuid();
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Admin;
}

bool ChatSessionController::canRemoveMember(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = currentUserUuid();
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

bool ChatSessionController::canTransferOwner(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty() || !group.membersID.contains(userId)) {
        return false;
    }

    const QString currentUserId = currentUserUuid();
    return !currentUserId.isEmpty() &&
           CurrentUser::instance().isCurrentUserId(group.ownerId) &&
           !CurrentUser::instance().isCurrentUserId(userId);
}

void ChatSessionController::saveGroupField(const QString& value, void (*assign)(Group&, const QString&))
{
    const QString currentGroupId = groupId();
    if (currentGroupId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({currentGroupId});
    if (group.groupId.isEmpty()) {
        return;
    }

    const QString nextValue = value.trimmed();
    const Group previousGroup = group;
    assign(group, nextValue);
    if (group.groupName != previousGroup.groupName && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.introduction != previousGroup.introduction && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.announcement != previousGroup.announcement && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.groupName == previousGroup.groupName &&
        group.introduction == previousGroup.introduction &&
        group.announcement == previousGroup.announcement &&
        group.currentUserNickname == previousGroup.currentUserNickname &&
        group.remark == previousGroup.remark) {
        return;
    }

    if (group.groupName != previousGroup.groupName ||
        group.introduction != previousGroup.introduction ||
        group.announcement != previousGroup.announcement) {
        GroupRemoteDataSource::instance().updateGroup(group);
        return;
    }

    GroupRemoteDataSource::instance().updateMySettings(group);
}
