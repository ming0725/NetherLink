#pragma once

#include <QDateTime>
#include <QSize>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "ChatMessage.h"
#include "Group.h"
#include "User.h"

struct EmptyRequest {
};

struct FriendListRequest {
    QString keyword;
};

struct FriendGroupListRequest {
    QString keyword;
};

struct FriendGroupItemsRequest {
    QString groupId;
    QString keyword;
    int offset = 0;
    int limit = 64;
};

struct GroupListRequest {
    QString keyword;
};

struct GroupCategoryListRequest {
    QString keyword;
};

struct GroupCategoryItemsRequest {
    QString categoryId;
    QString keyword;
    int offset = 0;
    int limit = 64;
};

struct UserDetailRequest {
    QString userId;
};

struct GroupDetailRequest {
    QString groupId;
};

struct ConversationListRequest {
    QString keyword;
};

struct ConversationMessagesRequest {
    QString conversationId;
    int offsetFromLatest = 0;
    int limit = 30;
};

struct ConversationMetaRequest {
    QString conversationId;
};

struct ConversationThreadRequest {
    QString conversationId;
    int offsetFromLatest = 0;
    int limit = 30;
};

struct ConversationThreadUntilMessageRequest {
    QString conversationId;
    QString messageId;
    int offsetFromLatest = 0;
};

struct AiChatListRequest {
    int offset = 0;
    int limit = 20;
};

struct PostFeedRequest {
    int offset = 0;
    int limit = 20;
    bool followOnly = false;
};

struct PostDetailRequest {
    QString postId;
};

struct PostCommentsRequest {
    QString postId;
    int offset = 0;
    int limit = 20;
};

struct FriendSummary {
    QString userId;
    QString displayName;
    QString avatarPath;
    UserStatus status = Offline;
    QString signature;
    bool isDoNotDisturb = false;
    QString groupId = "default";
    QString groupName = "默认分组";
    QString nickName;
    QString remark;
};

struct FriendGroupSummary {
    QString groupId = "default";
    QString friendGroupId = "default";
    QString groupName = "默认分组";
    int friendCount = 0;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct GroupCategorySummary {
    QString categoryId = "gg_joined";
    QString categoryName = "我加入的群聊";
    int groupCount = 0;
};

struct ConversationSummary {
    QString conversationId;
    QString title;
    QString avatarPath;
    QString previewText;
    QDateTime lastMessageTime;
    QDateTime messageListTime;
    int unreadCount = 0;
    bool isDoNotDisturb = false;
    bool isPinned = false;
    bool isGroup = false;
    int memberCount = 0;
    QString peerUserId;
    QString groupId;
};

struct ConversationSyncState {
    QString conversationId;
    int unreadCount = 0;
    bool isDoNotDisturb = false;
    bool isPinned = false;
    QDateTime messageListTime;
    QDateTime lastReadAt;
    QDateTime hiddenAt;
};

struct ConversationMeta {
    QString conversationId;
    QString title;
    QString avatarPath;
    bool isGroup = false;
    int memberCount = 0;
    UserStatus status = Offline;
    bool isDoNotDisturb = false;
    bool isPinned = false;
    QString peerUserId;
    QString groupId;
};

using ChatMessageList = QVector<QSharedPointer<ChatMessage>>;

struct ConversationThreadData {
    ConversationMeta meta;
    ChatMessageList messages;
    int unreadCount = 0;
    int loadedMessageCount = 0;
    bool hasMoreBefore = false;
};

struct GroupMembersPage {
    QString groupId;
    QString keyword;
    QVector<GroupMemberProfile> members;
    int offset = 0;
    int totalCount = 0;
    bool hasMore = false;
};

struct AiChatListEntry {
    QString conversationId;
    QString title;
    QDateTime time;
    bool hasUnreadDot = false;
};

struct AiChatTraceStep {
    int sequence = 0;
    QString stepId;
    QString phase;
    QString status;
    QString text;
    QString tool;
    QString query;
    QVector<QString> urls;
};

struct AiChatTrace {
    QString traceId;
    QString status;
    QString summary;
    QVector<AiChatTraceStep> steps;
    QVector<QString> sourceRefs;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isValid() const
    {
        return !traceId.isEmpty() || !summary.isEmpty() || !steps.isEmpty();
    }
};

struct AiChatMessage {
    QString messageId;
    QString conversationId;
    QString text;
    bool isFromUser = false;
    QDateTime time;
    AiChatTrace trace;
};

struct AiChatStreamStatus {
    bool active = false;
    bool userVisible = false;
    QString stepId;
    int sequence = 0;
    QString phase;
    QString status;
    QString tool;
    QString query;
    QVector<QString> urls;
    QString message;
};

struct AiChatStreamChunk {
    QString streamId;
    QString delta;
    QString kind;
    bool transient = false;
    QString stepId;
    QString segmentId;
    bool segmentStart = false;
    bool segmentEnd = false;
};

struct AiChatProgressSegment {
    QString stepId;
    QString segmentId;
    QString text;
    bool complete = false;
};

struct AiChatContextUsageRequest {
    QString conversationId;
};

struct AiChatMessagesRequest {
    QString conversationId;
};

struct AiChatContextUsage {
    QString conversationId;
    int usedTokens = 0;
    int maxTokens = 0;
    bool available = false;
};

struct AiChatRequestOptions {
    QString model = "deepseek-v4-pro";
    QString thinkingType = "enabled";
    QString reasoningEffort = "high";
};

struct PostSummary {
    QString postId;
    QString title;
    QString thumbnailImagePath;
    QSize thumbnailImageSize;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    int likeCount = 0;
    int commentCount = 0;
    bool isLiked = false;
    bool isFollowedAuthor = false;
    QDateTime createdAt;
};

struct PostDetailData {
    QString postId;
    QString title;
    QString content;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    QVector<QString> imagePaths;
    int likeCount = 0;
    int commentCount = 0;
    bool isLiked = false;
    bool isFollowedAuthor = false;
    QDateTime createdAt;
    QDateTime contentCreatedAt;
};

struct PostCommentReply {
    QString replyId;
    QString commentId;
    QString postId;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    QString targetUserId;
    QString targetUserName;
    QString targetReplyId;
    QString content;
    QDateTime createdAt;
    int likeCount = 0;
    bool isLiked = false;
};

struct PostComment {
    QString commentId;
    QString postId;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    QString content;
    QDateTime createdAt;
    int likeCount = 0;
    bool isLiked = false;
    QVector<PostCommentReply> replies;
    int totalReplyCount = 0;
};

struct PostCommentsPage {
    QString postId;
    QVector<PostComment> comments;
    int offset = 0;
    int totalCount = 0;
    bool hasMore = false;
};
