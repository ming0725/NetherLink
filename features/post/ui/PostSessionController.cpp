#include "features/post/ui/PostSessionController.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include "features/post/data/PostCommentRepository.h"
#include "features/post/data/PostRemoteDataSource.h"
#include "features/post/data/PostRepository.h"

namespace {

QJsonArray arrayFromResponse(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.value(key).isArray()) {
            return object.value(key).toArray();
        }
    }
    if (object.value(QStringLiteral("data")).isObject()) {
        return arrayFromResponse(object.value(QStringLiteral("data")).toObject(), keys);
    }
    return {};
}

int totalFromResponse(const QJsonObject& object, int fallback)
{
    if (object.contains(QStringLiteral("total"))) {
        return object.value(QStringLiteral("total")).toInt(fallback);
    }
    if (object.contains(QStringLiteral("totalCount"))) {
        return object.value(QStringLiteral("totalCount")).toInt(fallback);
    }
    if (object.value(QStringLiteral("data")).isObject()) {
        return totalFromResponse(object.value(QStringLiteral("data")).toObject(), fallback);
    }
    return fallback;
}

bool hasMoreFromResponse(const QJsonObject& object, int offset, int limit, int count, int total)
{
    if (object.contains(QStringLiteral("hasMore"))) {
        return object.value(QStringLiteral("hasMore")).toBool(false);
    }
    if (object.value(QStringLiteral("data")).isObject()) {
        return hasMoreFromResponse(object.value(QStringLiteral("data")).toObject(), offset, limit, count, total);
    }
    if (total > 0) {
        return offset + count < total;
    }
    return count >= limit;
}

QJsonObject objectFromResponse(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.value(key).isObject()) {
            return object.value(key).toObject();
        }
    }
    return object;
}

} // namespace

PostSessionController::PostSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&PostRepository::instance(), &PostRepository::postUpdated,
            this, [this](const PostSummary& summary) {
                emit postUpdated(summary);
                if (!m_currentPostId.isEmpty() && summary.postId == m_currentPostId) {
                    emit currentPostUpdated(summary);
                }
            });
    connect(&PostRepository::instance(), &PostRepository::postDetailReady,
            this, [this](const QString& requestId, const PostDetailData& detail) {
                if (requestId != m_currentDetailRequestId || detail.postId != m_currentPostId) {
                    return;
                }

                m_currentDetailRequestId.clear();
                emit currentPostDetailLoaded(detail);
            });
    connect(&PostCommentRepository::instance(), &PostCommentRepository::postCommentsReady,
            this, &PostSessionController::postCommentsLoaded);
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::feedFetched,
            this,
            [this](const QString& requestId, int offset, int limit, bool followOnly, const QJsonObject& response) {
                const QJsonArray posts = arrayFromResponse(response,
                                                           {QStringLiteral("posts"),
                                                            QStringLiteral("items"),
                                                            QStringLiteral("results"),
                                                            QStringLiteral("data")});
                const QVector<PostSummary> summaries = PostRepository::instance().upsertPostsFromJson(posts);
                const int total = totalFromResponse(response, offset + summaries.size());
                const bool hasMore = hasMoreFromResponse(response, offset, limit, summaries.size(), total);
                emit postFeedPageLoaded(requestId, offset, limit, followOnly, summaries, hasMore);
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::postDetailFetched,
            this,
            [this](const QString& requestId, const QString& postId, const QJsonObject& response) {
                if (requestId != m_currentDetailRequestId || postId != m_currentPostId) {
                    return;
                }
                m_currentDetailRequestId.clear();
                const PostDetailData detail = PostRepository::instance().upsertPostFromJson(
                        objectFromResponse(response, {QStringLiteral("post")}));
                if (!detail.postId.isEmpty()) {
                    emit currentPostDetailLoaded(detail);
                }
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::commentsFetched,
            this,
            [this](const QString& requestId, const QString& postId, int offset, int limit, const QJsonObject& response) {
                const QJsonArray comments = arrayFromResponse(response,
                                                              {QStringLiteral("comments"),
                                                               QStringLiteral("items"),
                                                               QStringLiteral("results"),
                                                               QStringLiteral("data")});
                const int total = totalFromResponse(response, offset + comments.size());
                const bool hasMore = hasMoreFromResponse(response, offset, limit, comments.size(), total);
                const PostCommentsPage page = PostCommentRepository::instance().upsertCommentsFromJson(
                        postId, comments, offset, total, hasMore);
                emit postCommentsLoaded(requestId, page);
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::postLikeUpdated,
            this,
            [](const QString&, const QString& postId, const QJsonObject& response) {
                PostRepository::instance().applyPostLikeResult(postId, response);
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::authorFollowUpdated,
            this,
            [](const QString&, const QString& authorId, bool followed, const QJsonObject&) {
                PostRepository::instance().applyAuthorFollowState(authorId, followed);
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::commentLikeUpdated,
            this,
            [this](const QString&, const QString& commentId, const QJsonObject& response) {
                if (PostCommentRepository::instance().applyCommentLikeResult(commentId, response)) {
                    emit commentLikeUpdated(commentId, response.value(QStringLiteral("isLiked")).toBool());
                }
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::replyLikeUpdated,
            this,
            [this](const QString& requestId, const QString& replyId, const QJsonObject& response) {
                const QString commentId = m_replyCommentByRequestId.take(requestId);
                if (PostCommentRepository::instance().applyReplyLikeResult(replyId, response)) {
                    emit replyLikeUpdated(commentId, replyId, response.value(QStringLiteral("isLiked")).toBool());
                }
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::commentCreated,
            this,
            [this](const QString&, const QString& postId, const QJsonObject& response) {
                const PostComment comment = PostCommentRepository::instance().upsertCommentFromJson(postId, response);
                if (!comment.commentId.isEmpty()) {
                    PostRepository::instance().adjustPostCommentCount(postId, 1);
                    emit commentCreated(comment);
                }
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::replyCreated,
            this,
            [this](const QString& requestId, const QString& commentId, const QJsonObject& response) {
                const QString targetReplyId = m_replyTargetByRequestId.take(requestId);
                const PostCommentReply reply = PostCommentRepository::instance().upsertReplyFromJson(commentId, response);
                if (!reply.replyId.isEmpty()) {
                    PostRepository::instance().adjustPostCommentCount(reply.postId, 1);
                    emit replyCreated(commentId, targetReplyId, reply);
                }
            });
    connect(&PostRemoteDataSource::instance(),
            &PostRemoteDataSource::operationFailed,
            this,
            [this](const QString& requestId, const QString& operation, const QString& targetId, const NetworkError&) {
                m_replyCommentByRequestId.remove(requestId);
                m_replyTargetByRequestId.remove(requestId);
                if (requestId == m_currentDetailRequestId) {
                    m_currentDetailRequestId.clear();
                }
                emit postOperationFailed(operation, targetId);
            });
}

QVector<PostSummary> PostSessionController::loadFeedPage(int offset, int limit, bool followOnly) const
{
    return PostRepository::instance().requestPostFeed({offset, limit, followOnly});
}

QString PostSessionController::requestFeedPage(int offset, int limit, bool followOnly)
{
    return PostRemoteDataSource::instance().fetchFeed(offset, limit, followOnly);
}

void PostSessionController::openPost(const PostSummary& summary)
{
    m_currentPostId = summary.postId;
    m_currentDetailRequestId = PostRemoteDataSource::instance().fetchPostDetail(summary.postId);
    if (m_currentDetailRequestId.isEmpty()) {
        m_currentDetailRequestId = PostRepository::instance().requestPostDetailAsync({summary.postId});
    }
}

void PostSessionController::closePost()
{
    m_currentPostId.clear();
    m_currentDetailRequestId.clear();
}

QString PostSessionController::currentPostId() const
{
    return m_currentPostId;
}

QString PostSessionController::requestPostComments(const QString& postId, int offset, int limit)
{
    const QString requestId = PostRemoteDataSource::instance().fetchComments(postId, offset, limit);
    if (!requestId.isEmpty()) {
        return requestId;
    }
    return PostCommentRepository::instance().requestPostCommentsAsync({postId, offset, limit});
}

bool PostSessionController::setCurrentPostLiked(bool liked)
{
    if (m_currentPostId.isEmpty()) {
        return false;
    }
    return setPostLiked(m_currentPostId, liked);
}

bool PostSessionController::setPostLiked(const QString& postId, bool liked)
{
    if (postId.isEmpty()) {
        return false;
    }
    return !PostRemoteDataSource::instance().setPostLiked(postId, liked).isEmpty();
}

bool PostSessionController::setAuthorFollowed(const QString& authorId, bool followed)
{
    if (authorId.isEmpty()) {
        return false;
    }

    return !PostRemoteDataSource::instance().setAuthorFollowed(authorId, followed).isEmpty();
}

bool PostSessionController::setCommentLiked(const QString& commentId, bool liked)
{
    if (commentId.isEmpty()) {
        return false;
    }
    return !PostRemoteDataSource::instance().setCommentLiked(commentId, liked).isEmpty();
}

bool PostSessionController::setReplyLiked(const QString& commentId, const QString& replyId, bool liked)
{
    if (commentId.isEmpty() || replyId.isEmpty()) {
        return false;
    }
    const QString requestId = PostRemoteDataSource::instance().setReplyLiked(replyId, liked);
    if (requestId.isEmpty()) {
        return false;
    }
    m_replyCommentByRequestId.insert(requestId, commentId);
    return true;
}

bool PostSessionController::adjustCurrentPostCommentCount(int delta)
{
    if (m_currentPostId.isEmpty() || delta == 0) {
        return false;
    }
    return PostRepository::instance().adjustPostCommentCount(m_currentPostId, delta);
}

bool PostSessionController::createComment(const QString& postId, const QString& text)
{
    return !PostRemoteDataSource::instance().createComment(postId, text).isEmpty();
}

bool PostSessionController::createReply(const QString& commentId,
                                        const QString& text,
                                        const QString& targetUserId,
                                        const QString& targetReplyId)
{
    const QString requestId = PostRemoteDataSource::instance().createReply(commentId,
                                                                          text,
                                                                          targetUserId,
                                                                          targetReplyId);
    if (requestId.isEmpty()) {
        return false;
    }
    m_replyTargetByRequestId.insert(requestId, targetReplyId);
    return true;
}
