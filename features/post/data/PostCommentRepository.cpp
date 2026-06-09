#include "PostCommentRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUuid>

#include "app/state/CurrentUser.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryFunctionOperation.h"

namespace {

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

QDateTime dateTimeFromJson(const QJsonObject& object, const QStringList& keys)
{
    return QDateTime::fromString(firstString(object, keys), Qt::ISODateWithMs);
}

bool boolFromViewer(const QJsonObject& object, const QString& key, bool fallback = false)
{
    if (object.contains(key)) {
        return object.value(key).toBool(fallback);
    }
    const QJsonObject viewer = object.value(QStringLiteral("viewer")).toObject();
    return viewer.value(key).toBool(fallback);
}

QString visibleName(const QString& userId)
{
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return CurrentUser::instance().getUserName();
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    if (!user.remark.isEmpty()) {
        return user.remark;
    }
    return user.nick.isEmpty() ? userId : user.nick;
}

QString avatarPath(const QString& userId)
{
    if (CurrentUser::instance().isCurrentUserId(userId)) {
        return CurrentUser::instance().getAvatarPath();
    }
    return UserRepository::instance().requestUserAvatarPath(userId);
}

PostCommentReply replyFromJson(const QString& postId,
                               const QString& commentId,
                               const QJsonObject& object)
{
    PostCommentReply reply;
    reply.replyId = firstString(object, {QStringLiteral("replyId"),
                                         QStringLiteral("commentId"),
                                         QStringLiteral("id")});
    reply.commentId = commentId;
    reply.postId = firstString(object, {QStringLiteral("postId")});
    if (reply.postId.isEmpty()) {
        reply.postId = postId;
    }
    reply.authorId = firstString(object, {QStringLiteral("authorUuid"),
                                          QStringLiteral("authorUserUuid"),
                                          QStringLiteral("senderUuid"),
                                          QStringLiteral("userUuid"),
                                          QStringLiteral("authorId"),
                                          QStringLiteral("authorUserId"),
                                          QStringLiteral("userId")});
    reply.authorName = firstString(object, {QStringLiteral("authorName"), QStringLiteral("nickName")});
    if (reply.authorName.isEmpty()) {
        reply.authorName = visibleName(reply.authorId);
    }
    reply.authorAvatarPath = firstString(object, {QStringLiteral("authorAvatarPath"),
                                                  QStringLiteral("avatarUrl")});
    if (reply.authorAvatarPath.isEmpty()) {
        reply.authorAvatarPath = avatarPath(reply.authorId);
    }
    reply.targetUserId = firstString(object, {QStringLiteral("targetUserUuid"),
                                              QStringLiteral("replyToUserUuid"),
                                              QStringLiteral("targetUserId"),
                                              QStringLiteral("replyToUserId")});
    reply.targetUserName = firstString(object, {QStringLiteral("targetUserName"), QStringLiteral("replyToUserName")});
    if (reply.targetUserName.isEmpty() && !reply.targetUserId.isEmpty()) {
        reply.targetUserName = visibleName(reply.targetUserId);
    }
    reply.targetReplyId = firstString(object, {QStringLiteral("targetReplyId"), QStringLiteral("parentReplyId")});
    reply.content = firstString(object, {QStringLiteral("content"), QStringLiteral("text")});
    reply.createdAt = dateTimeFromJson(object, {QStringLiteral("createdAt"), QStringLiteral("updatedAt")});
    reply.likeCount = object.value(QStringLiteral("likeCount")).toInt(object.value(QStringLiteral("likes")).toInt());
    reply.isLiked = boolFromViewer(object, QStringLiteral("isLiked"));
    return reply;
}

PostComment commentFromJson(const QJsonObject& object)
{
    PostComment comment;
    comment.commentId = firstString(object, {QStringLiteral("commentId"), QStringLiteral("id")});
    comment.postId = firstString(object, {QStringLiteral("postId")});
    comment.authorId = firstString(object, {QStringLiteral("authorUuid"),
                                            QStringLiteral("authorUserUuid"),
                                            QStringLiteral("senderUuid"),
                                            QStringLiteral("userUuid"),
                                            QStringLiteral("authorId"),
                                            QStringLiteral("authorUserId"),
                                            QStringLiteral("userId")});
    comment.authorName = firstString(object, {QStringLiteral("authorName"), QStringLiteral("nickName")});
    if (comment.authorName.isEmpty()) {
        comment.authorName = visibleName(comment.authorId);
    }
    comment.authorAvatarPath = firstString(object, {QStringLiteral("authorAvatarPath"),
                                                    QStringLiteral("avatarUrl")});
    if (comment.authorAvatarPath.isEmpty()) {
        comment.authorAvatarPath = avatarPath(comment.authorId);
    }
    comment.content = firstString(object, {QStringLiteral("content"), QStringLiteral("text")});
    comment.createdAt = dateTimeFromJson(object, {QStringLiteral("createdAt"), QStringLiteral("updatedAt")});
    comment.likeCount = object.value(QStringLiteral("likeCount")).toInt(object.value(QStringLiteral("likes")).toInt());
    comment.isLiked = boolFromViewer(object, QStringLiteral("isLiked"));

    const QJsonArray replies = object.value(QStringLiteral("replies")).toArray();
    for (const QJsonValue& value : replies) {
        const PostCommentReply reply = replyFromJson(comment.postId, comment.commentId, value.toObject());
        if (!reply.replyId.isEmpty()) {
            comment.replies.push_back(reply);
        }
    }
    comment.totalReplyCount = object.value(QStringLiteral("totalReplyCount")).toInt(comment.replies.size());
    return comment;
}

QJsonObject replyToJson(const PostCommentReply& reply)
{
    return {
            {QStringLiteral("replyId"), reply.replyId},
            {QStringLiteral("commentId"), reply.commentId},
            {QStringLiteral("postId"), reply.postId},
            {QStringLiteral("authorId"), reply.authorId},
            {QStringLiteral("authorName"), reply.authorName},
            {QStringLiteral("authorAvatarPath"), reply.authorAvatarPath},
            {QStringLiteral("targetUserId"), reply.targetUserId},
            {QStringLiteral("targetUserName"), reply.targetUserName},
            {QStringLiteral("targetReplyId"), reply.targetReplyId},
            {QStringLiteral("content"), reply.content},
            {QStringLiteral("createdAt"), reply.createdAt.toUTC().toString(Qt::ISODateWithMs)},
            {QStringLiteral("likeCount"), reply.likeCount},
            {QStringLiteral("isLiked"), reply.isLiked}
    };
}

QJsonObject commentToJson(const PostComment& comment)
{
    QJsonArray replies;
    for (const PostCommentReply& reply : comment.replies) {
        replies.append(replyToJson(reply));
    }
    return {
            {QStringLiteral("commentId"), comment.commentId},
            {QStringLiteral("postId"), comment.postId},
            {QStringLiteral("authorId"), comment.authorId},
            {QStringLiteral("authorName"), comment.authorName},
            {QStringLiteral("authorAvatarPath"), comment.authorAvatarPath},
            {QStringLiteral("content"), comment.content},
            {QStringLiteral("createdAt"), comment.createdAt.toUTC().toString(Qt::ISODateWithMs)},
            {QStringLiteral("likeCount"), comment.likeCount},
            {QStringLiteral("isLiked"), comment.isLiked},
            {QStringLiteral("replies"), replies},
            {QStringLiteral("totalReplyCount"), comment.totalReplyCount}
    };
}

} // namespace

PostCommentRepository::PostCommentRepository(QObject* parent)
    : QObject(parent)
{
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("post_comments"))) {
        const PostComment comment = commentFromJson(object);
        if (!comment.commentId.isEmpty()) {
            m_comments.insert(comment.commentId, comment);
        }
    }

    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("comment_like_states"))) {
        const QString id = object.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            m_commentLikeStates.insert(id, {
                    object.value(QStringLiteral("likes")).toInt(),
                    object.value(QStringLiteral("isLiked")).toBool(false)
            });
        }
    }
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("reply_like_states"))) {
        const QString id = object.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            m_replyLikeStates.insert(id, {
                    object.value(QStringLiteral("likes")).toInt(),
                    object.value(QStringLiteral("isLiked")).toBool(false)
            });
        }
    }
}

PostCommentRepository& PostCommentRepository::instance()
{
    static PostCommentRepository repo;
    return repo;
}

PostCommentsPage PostCommentRepository::requestPostComments(const PostCommentsRequest& query) const
{
    auto handler = [this](const PostCommentsRequest& request) {
        QMutexLocker locker(&m_mutex);

        PostCommentsPage page;
        page.postId = request.postId;
        page.offset = qMax(0, request.offset);
        if (request.postId.isEmpty() || request.limit <= 0) {
            return page;
        }

        QVector<PostComment> comments;
        for (PostComment comment : m_comments) {
            if (comment.postId != request.postId) {
                continue;
            }
            if (const auto likeIt = m_commentLikeStates.constFind(comment.commentId);
                likeIt != m_commentLikeStates.constEnd()) {
                comment.likeCount = likeIt->likes;
                comment.isLiked = likeIt->isLiked;
            }
            for (PostCommentReply& reply : comment.replies) {
                if (const auto likeIt = m_replyLikeStates.constFind(reply.replyId);
                    likeIt != m_replyLikeStates.constEnd()) {
                    reply.likeCount = likeIt->likes;
                    reply.isLiked = likeIt->isLiked;
                }
            }
            comments.push_back(comment);
        }

        std::sort(comments.begin(), comments.end(), [](const PostComment& lhs, const PostComment& rhs) {
            return lhs.createdAt > rhs.createdAt;
        });

        page.totalCount = comments.size();
        if (page.offset >= comments.size()) {
            return page;
        }

        page.comments = comments.mid(page.offset, qMax(0, request.limit));
        page.hasMore = page.offset + page.comments.size() < page.totalCount;
        return page;
    };

    return RepositoryFunctionOperation<PostCommentsRequest, PostCommentsPage, decltype(handler)>(handler)
            .request(query);
}

QString PostCommentRepository::requestPostCommentsAsync(const PostCommentsRequest& query, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, query, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        const PostCommentsPage page = requestPostComments(query);
        QMetaObject::invokeMethod(this, [this, requestId, page]() {
            emit postCommentsReady(requestId, page);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

bool PostCommentRepository::setCommentLiked(const QString& commentId, bool liked)
{
    QMutexLocker locker(&m_mutex);
    if (!m_comments.contains(commentId)) {
        return false;
    }

    const int baseLikes = m_comments.value(commentId).likeCount;
    LikeState state = m_commentLikeStates.value(commentId, {baseLikes, false});
    if (state.isLiked != liked) {
        state.isLiked = liked;
        state.likes = qMax(0, state.likes + (liked ? 1 : -1));
        m_commentLikeStates.insert(commentId, state);
        LocalDataStore::instance().upsertValue(QStringLiteral("comment_like_states"),
                                               commentId,
                                               {{QStringLiteral("id"), commentId},
                                                {QStringLiteral("likes"), state.likes},
                                                {QStringLiteral("isLiked"), state.isLiked}});
    }
    return true;
}

bool PostCommentRepository::setReplyLiked(const QString& replyId, bool liked)
{
    QMutexLocker locker(&m_mutex);
    int baseLikes = -1;
    for (const PostComment& comment : m_comments) {
        for (const PostCommentReply& reply : comment.replies) {
            if (reply.replyId == replyId) {
                baseLikes = reply.likeCount;
                break;
            }
        }
    }
    if (baseLikes < 0) {
        return false;
    }

    LikeState state = m_replyLikeStates.value(replyId, {baseLikes, false});
    if (state.isLiked != liked) {
        state.isLiked = liked;
        state.likes = qMax(0, state.likes + (liked ? 1 : -1));
        m_replyLikeStates.insert(replyId, state);
        LocalDataStore::instance().upsertValue(QStringLiteral("reply_like_states"),
                                               replyId,
                                               {{QStringLiteral("id"), replyId},
                                                {QStringLiteral("likes"), state.likes},
                                                {QStringLiteral("isLiked"), state.isLiked}});
    }
    return true;
}

PostCommentsPage PostCommentRepository::upsertCommentsFromJson(const QString& postId,
                                                               const QJsonArray& comments,
                                                               int offset,
                                                               int totalCount,
                                                               bool hasMore)
{
    PostCommentsPage page;
    page.postId = postId;
    page.offset = qMax(0, offset);
    page.totalCount = qMax(0, totalCount);
    page.hasMore = hasMore;

    QMutexLocker locker(&m_mutex);
    for (const QJsonValue& value : comments) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("comment")).isObject()) {
            object = object.value(QStringLiteral("comment")).toObject();
        }
        PostComment comment = commentFromJson(object);
        if (comment.postId.isEmpty()) {
            comment.postId = postId;
        }
        if (comment.commentId.isEmpty()) {
            continue;
        }
        if (const auto likeIt = m_commentLikeStates.constFind(comment.commentId);
            likeIt != m_commentLikeStates.constEnd()) {
            comment.likeCount = likeIt->likes;
            comment.isLiked = likeIt->isLiked;
        }
        for (PostCommentReply& reply : comment.replies) {
            if (const auto likeIt = m_replyLikeStates.constFind(reply.replyId);
                likeIt != m_replyLikeStates.constEnd()) {
                reply.likeCount = likeIt->likes;
                reply.isLiked = likeIt->isLiked;
            }
        }
        m_comments.insert(comment.commentId, comment);
        LocalDataStore::instance().upsertValue(QStringLiteral("post_comments"),
                                               comment.commentId,
                                               commentToJson(comment));
        page.comments.push_back(comment);
    }

    if (page.totalCount <= 0) {
        page.totalCount = page.offset + page.comments.size() + (page.hasMore ? 1 : 0);
    }
    return page;
}

PostComment PostCommentRepository::upsertCommentFromJson(const QString& postId, const QJsonObject& object)
{
    QJsonObject commentObject = object;
    if (commentObject.value(QStringLiteral("comment")).isObject()) {
        commentObject = commentObject.value(QStringLiteral("comment")).toObject();
    }

    PostComment comment = commentFromJson(commentObject);
    if (comment.postId.isEmpty()) {
        comment.postId = postId;
    }
    if (comment.commentId.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    m_comments.insert(comment.commentId, comment);
    LocalDataStore::instance().upsertValue(QStringLiteral("post_comments"),
                                           comment.commentId,
                                           commentToJson(comment));
    return comment;
}

PostCommentReply PostCommentRepository::upsertReplyFromJson(const QString& commentId, const QJsonObject& object)
{
    QJsonObject replyObject = object;
    if (replyObject.value(QStringLiteral("reply")).isObject()) {
        replyObject = replyObject.value(QStringLiteral("reply")).toObject();
    }

    QMutexLocker locker(&m_mutex);
    PostComment parent = m_comments.value(commentId);
    if (parent.commentId.isEmpty()) {
        return {};
    }

    PostCommentReply reply = replyFromJson(parent.postId, commentId, replyObject);
    if (reply.replyId.isEmpty()) {
        return {};
    }

    bool replaced = false;
    for (PostCommentReply& existing : parent.replies) {
        if (existing.replyId == reply.replyId) {
            existing = reply;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        parent.replies.push_back(reply);
    }
    parent.totalReplyCount = qMax(parent.totalReplyCount, parent.replies.size());
    m_comments.insert(parent.commentId, parent);
    LocalDataStore::instance().upsertValue(QStringLiteral("post_comments"),
                                           parent.commentId,
                                           commentToJson(parent));
    return reply;
}

bool PostCommentRepository::applyCommentLikeResult(const QString& commentId, const QJsonObject& object)
{
    QMutexLocker locker(&m_mutex);
    if (!m_comments.contains(commentId)) {
        return false;
    }

    PostComment comment = m_comments.value(commentId);
    comment.likeCount = object.value(QStringLiteral("likeCount")).toInt(comment.likeCount);
    comment.isLiked = object.value(QStringLiteral("isLiked")).toBool(comment.isLiked);
    m_comments.insert(commentId, comment);
    m_commentLikeStates.insert(commentId, {comment.likeCount, comment.isLiked});
    LocalDataStore::instance().upsertValue(QStringLiteral("comment_like_states"),
                                           commentId,
                                           {{QStringLiteral("id"), commentId},
                                            {QStringLiteral("likes"), comment.likeCount},
                                            {QStringLiteral("isLiked"), comment.isLiked}});
    LocalDataStore::instance().upsertValue(QStringLiteral("post_comments"),
                                           commentId,
                                           commentToJson(comment));
    return true;
}

bool PostCommentRepository::applyReplyLikeResult(const QString& replyId, const QJsonObject& object)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_comments.begin(); it != m_comments.end(); ++it) {
        PostComment comment = it.value();
        for (PostCommentReply& reply : comment.replies) {
            if (reply.replyId != replyId) {
                continue;
            }
            reply.likeCount = object.value(QStringLiteral("likeCount")).toInt(reply.likeCount);
            reply.isLiked = object.value(QStringLiteral("isLiked")).toBool(reply.isLiked);
            it.value() = comment;
            m_replyLikeStates.insert(replyId, {reply.likeCount, reply.isLiked});
            LocalDataStore::instance().upsertValue(QStringLiteral("reply_like_states"),
                                                   replyId,
                                                   {{QStringLiteral("id"), replyId},
                                                    {QStringLiteral("likes"), reply.likeCount},
                                                    {QStringLiteral("isLiked"), reply.isLiked}});
            LocalDataStore::instance().upsertValue(QStringLiteral("post_comments"),
                                                   comment.commentId,
                                                   commentToJson(comment));
            return true;
        }
    }
    return false;
}
