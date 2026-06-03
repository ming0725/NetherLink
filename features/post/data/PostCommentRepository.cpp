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
#include "shared/data/RepositoryFunctionOperation.h"
#include "shared/data/LocalDataStore.h"

namespace {

constexpr int kSampleCommentCount = 72;

const QStringList& commentSamples()
{
    static const QStringList samples = []() {
        QStringList result;
        const QJsonArray array = LocalDataStore::instance()
                .seedObject(QStringLiteral(":/resources/data/comment_samples.json"))
                .value(QStringLiteral("comments")).toArray();
        for (const QJsonValue& value : array) {
            result.append(value.toString());
        }
        return result.isEmpty() ? QStringList{QStringLiteral("评论内容")} : result;
    }();
    return samples;
}

const QStringList& replySamples()
{
    static const QStringList samples = []() {
        QStringList result;
        const QJsonArray array = LocalDataStore::instance()
                .seedObject(QStringLiteral(":/resources/data/comment_samples.json"))
                .value(QStringLiteral("replies")).toArray();
        for (const QJsonValue& value : array) {
            result.append(value.toString());
        }
        return result.isEmpty() ? QStringList{QStringLiteral("回复内容")} : result;
    }();
    return samples;
}

QStringList userIds()
{
    return {
            QStringLiteral("u001"),
            QStringLiteral("u002"),
            QStringLiteral("u003"),
            QStringLiteral("u004"),
            QStringLiteral("u005"),
            QStringLiteral("u006"),
            QStringLiteral("u007"),
            QStringLiteral("u008"),
            QStringLiteral("u009"),
            QStringLiteral("u010"),
            QStringLiteral("u011"),
            QStringLiteral("u012")
    };
}

QString visibleName(const User& user)
{
    if (!user.remark.isEmpty()) {
        return user.remark;
    }
    return user.nick;
}

struct CommentUserIdentity {
    QString visibleName;
    QString avatarPath;
};

CommentUserIdentity commentUserIdentityForUserId(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return {currentUser.getUserName(), currentUser.getAvatarPath()};
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    return {visibleName(user), user.avatarPath};
}

} // namespace

PostCommentRepository::PostCommentRepository(QObject* parent)
    : QObject(parent)
{
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
        page.totalCount = kSampleCommentCount;
        if (request.postId.isEmpty() || request.limit <= 0 || page.offset >= kSampleCommentCount) {
            page.hasMore = false;
            return page;
        }

        const int end = qMin(kSampleCommentCount, page.offset + request.limit);
        page.comments.reserve(end - page.offset);
        for (int index = page.offset; index < end; ++index) {
            page.comments.append(buildCommentAt(request.postId, index));
        }
        page.hasMore = end < kSampleCommentCount;
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
    const int index = commentIndexForId(commentId);
    if (index < 0) {
        return false;
    }

    const int baseLikes = (index * 31 + 19) % 300;
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
    const int index = replyIndexForId(replyId);
    if (index < 0) {
        return false;
    }

    const int baseLikes = (index * 13 + 7) % 80;
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

PostComment PostCommentRepository::buildCommentAt(const QString& postId, int index) const
{
    const QStringList ids = userIds();
    const QString authorId = ids.at((index * 5 + 2) % ids.size());
    const CommentUserIdentity author = commentUserIdentityForUserId(authorId);
    const QString commentId = QStringLiteral("%1-c%2").arg(postId).arg(index + 1, 3, 10, QChar('0'));
    const int baseLikes = (index * 31 + 19) % 300;

    PostComment comment;
    comment.commentId = commentId;
    comment.postId = postId;
    comment.authorId = authorId;
    comment.authorName = author.visibleName;
    comment.authorAvatarPath = author.avatarPath;
    comment.content = commentSamples().at(index % commentSamples().size());
    comment.createdAt = QDateTime::fromString(QStringLiteral("2024-05-22T12:00:00"), Qt::ISODate)
                                .addSecs(-index * 430);
    comment.likeCount = baseLikes;
    comment.isLiked = false;
    comment.totalReplyCount = (index * 7 + 3) % 6;

    if (const auto likeIt = m_commentLikeStates.constFind(commentId);
        likeIt != m_commentLikeStates.constEnd()) {
        comment.likeCount = likeIt->likes;
        comment.isLiked = likeIt->isLiked;
    }

    for (int replyIndex = 0; replyIndex < comment.totalReplyCount; ++replyIndex) {
        comment.replies.append(buildReplyAt(postId, commentId, authorId, index, replyIndex));
    }
    return comment;
}

PostCommentReply PostCommentRepository::buildReplyAt(const QString& postId,
                                                     const QString& commentId,
                                                     const QString& parentAuthorId,
                                                     int commentIndex,
                                                     int replyIndex) const
{
    const QStringList ids = userIds();
    const QString authorId = ids.at((commentIndex * 3 + replyIndex * 2 + 5) % ids.size());
    const QString targetUserId = replyIndex == 0
            ? parentAuthorId
            : ids.at((commentIndex * 3 + replyIndex * 2 + 3) % ids.size());
    const CommentUserIdentity author = commentUserIdentityForUserId(authorId);
    const CommentUserIdentity target = commentUserIdentityForUserId(targetUserId);
    const QString replyId = QStringLiteral("%1-r%2").arg(commentId).arg(replyIndex + 1, 2, 10, QChar('0'));
    const int baseLikes = ((commentIndex + 1) * (replyIndex + 3) * 13 + 7) % 80;

    PostCommentReply reply;
    reply.replyId = replyId;
    reply.commentId = commentId;
    reply.postId = postId;
    reply.authorId = authorId;
    reply.authorName = author.visibleName;
    reply.authorAvatarPath = author.avatarPath;
    reply.targetUserId = targetUserId;
    reply.targetUserName = target.visibleName;
    if (replyIndex > 0) {
        reply.targetReplyId = QStringLiteral("%1-r%2").arg(commentId).arg(replyIndex, 2, 10, QChar('0'));
    }
    reply.content = replySamples().at((commentIndex + replyIndex) % replySamples().size());
    reply.createdAt = QDateTime::fromString(QStringLiteral("2024-05-22T12:30:00"), Qt::ISODate)
                              .addSecs(-(commentIndex * 430 + replyIndex * 83));
    reply.likeCount = baseLikes;
    reply.isLiked = false;

    if (const auto likeIt = m_replyLikeStates.constFind(replyId);
        likeIt != m_replyLikeStates.constEnd()) {
        reply.likeCount = likeIt->likes;
        reply.isLiked = likeIt->isLiked;
    }
    return reply;
}

int PostCommentRepository::commentIndexForId(const QString& commentId) const
{
    const int cPos = commentId.lastIndexOf(QStringLiteral("-c"));
    if (cPos < 0) {
        return -1;
    }

    bool ok = false;
    const int serial = commentId.mid(cPos + 2, 3).toInt(&ok);
    return ok && serial > 0 && serial <= kSampleCommentCount ? serial - 1 : -1;
}

int PostCommentRepository::replyIndexForId(const QString& replyId) const
{
    const int rPos = replyId.lastIndexOf(QStringLiteral("-r"));
    if (rPos < 0) {
        return -1;
    }

    bool ok = false;
    const int serial = replyId.mid(rPos + 2, 2).toInt(&ok);
    return ok && serial > 0 ? serial - 1 : -1;
}
