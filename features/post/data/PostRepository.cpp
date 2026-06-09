#include "PostRepository.h"

#include <QImageReader>
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

struct AuthorIdentity {
    QString name;
    QString avatarPath;
};

AuthorIdentity authorIdentityForUserId(const QString& userId,
                                       const QString& fallbackName = {},
                                       const QString& fallbackAvatarPath = {})
{
    if (!fallbackName.isEmpty() || !fallbackAvatarPath.isEmpty()) {
        return {fallbackName.isEmpty() ? userId : fallbackName, fallbackAvatarPath};
    }

    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return {currentUser.getUserName(), currentUser.getAvatarPath()};
    }

    const User author = UserRepository::instance().requestUserDetail({userId});
    return {author.nick, author.avatarPath};
}

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
    const QString value = firstString(object, keys);
    return QDateTime::fromString(value, Qt::ISODateWithMs);
}

QSize imageSizeForSource(const QString& source)
{
    if (source.isEmpty()) {
        return {};
    }

    QImageReader reader(source);
    reader.setAutoTransform(true);
    return reader.size();
}

bool boolFromViewer(const QJsonObject& object, const QString& key, bool fallback = false)
{
    if (object.contains(key)) {
        return object.value(key).toBool(fallback);
    }

    const QJsonObject viewer = object.value(QStringLiteral("viewer")).toObject();
    return viewer.value(key).toBool(fallback);
}

QVector<QString> mediaUrlsFromJson(const QJsonObject& object)
{
    QVector<QString> urls;
    const QJsonArray pictures = object.value(QStringLiteral("picturesPath")).toArray();
    for (const QJsonValue& value : pictures) {
        const QString url = value.toString();
        if (!url.isEmpty()) {
            urls.push_back(url);
        }
    }

    const QJsonArray media = object.value(QStringLiteral("media")).toArray();
    for (const QJsonValue& value : media) {
        const QJsonObject mediaObject = value.toObject();
        const QString url = firstString(mediaObject, {QStringLiteral("url"),
                                                      QStringLiteral("thumbnailUrl"),
                                                      QStringLiteral("fileUrl")});
        if (!url.isEmpty()) {
            urls.push_back(url);
        }
    }
    return urls;
}

Post postFromJson(const QJsonObject& object)
{
    Post post;
    post.postID = firstString(object, {QStringLiteral("postID"),
                                       QStringLiteral("postId"),
                                       QStringLiteral("id")});
    post.title = object.value(QStringLiteral("title")).toString();
    post.content = object.value(QStringLiteral("content")).toString();
    post.likes = object.value(QStringLiteral("likes")).toInt(
            object.value(QStringLiteral("likeCount")).toInt());
    post.commentCount = object.value(QStringLiteral("commentCount")).toInt(
            object.value(QStringLiteral("commentsCount")).toInt());

    const QJsonObject author = object.value(QStringLiteral("author")).toObject();
    post.authorID = firstString(object, {QStringLiteral("authorUuid"),
                                         QStringLiteral("authorUserUuid"),
                                         QStringLiteral("senderUuid"),
                                         QStringLiteral("userUuid"),
                                         QStringLiteral("authorID"),
                                         QStringLiteral("authorId"),
                                         QStringLiteral("authorUserId")});
    if (post.authorID.isEmpty()) {
        post.authorID = firstString(author, {QStringLiteral("userUuid"),
                                             QStringLiteral("uuid"),
                                             QStringLiteral("id"),
                                             QStringLiteral("userId")});
    }
    post.authorName = firstString(object, {QStringLiteral("authorName")});
    if (post.authorName.isEmpty()) {
        post.authorName = firstString(author, {QStringLiteral("nickName"),
                                               QStringLiteral("displayName"),
                                               QStringLiteral("name"),
                                               QStringLiteral("userId")});
    }
    post.authorAvatarPath = firstString(object, {QStringLiteral("authorAvatarPath"),
                                                 QStringLiteral("authorAvatarUrl")});
    if (post.authorAvatarPath.isEmpty()) {
        post.authorAvatarPath = firstString(author, {QStringLiteral("avatarUrl"),
                                                     QStringLiteral("avatarPath")});
    }

    post.createdAt = dateTimeFromJson(object, {QStringLiteral("createdAt"),
                                               QStringLiteral("updatedAt")});
    post.contentCreatedAt = dateTimeFromJson(object, {QStringLiteral("contentCreatedAt"),
                                                      QStringLiteral("createdAt")});
    post.thumbnailPath = firstString(object, {QStringLiteral("thumbnailPath"),
                                              QStringLiteral("thumbnailUrl"),
                                              QStringLiteral("coverUrl")});
    post.picturesPath = mediaUrlsFromJson(object);
    if (post.thumbnailPath.isEmpty() && !post.picturesPath.isEmpty()) {
        post.thumbnailPath = post.picturesPath.first();
    }
    post.thumbnailSize = imageSizeForSource(post.thumbnailPath);
    post.isLiked = boolFromViewer(object, QStringLiteral("isLiked"));
    post.isFollowedAuthor = boolFromViewer(object, QStringLiteral("isFollowedAuthor"));
    return post;
}

QJsonObject postLikeStateToJson(const QString& postId, int likes, bool isLiked)
{
    return {
            {QStringLiteral("postId"), postId},
            {QStringLiteral("likes"), likes},
            {QStringLiteral("isLiked"), isLiked}
    };
}

} // namespace

PostRepository::PostRepository(QObject* parent)
        : QObject(parent)
{
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("posts"))) {
        const Post post = postFromJson(object);
        if (!post.postID.isEmpty()) {
            m_posts.insert(post.postID, post);
        }
    }

    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("post_like_states"))) {
        const QString postId = object.value(QStringLiteral("postId")).toString();
        if (!postId.isEmpty()) {
            m_likeStates.insert(postId, {
                    object.value(QStringLiteral("likes")).toInt(),
                    object.value(QStringLiteral("isLiked")).toBool(false)
            });
        }
    }

    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("post_comment_count_deltas"))) {
        const QString postId = object.value(QStringLiteral("postId")).toString();
        if (!postId.isEmpty()) {
            m_commentCountDeltas.insert(postId, object.value(QStringLiteral("delta")).toInt());
        }
    }
}

PostRepository& PostRepository::instance()
{
    static PostRepository repo;
    return repo;
}

QVector<PostSummary> PostRepository::requestPostFeed(const PostFeedRequest& query) const
{
    auto handler = [this](const PostFeedRequest& request) {
        QMutexLocker locker(&mutex);
        QVector<PostSummary> result;
        if (request.limit <= 0 || request.offset < 0) {
            return result;
        }

        QVector<Post> posts = QVector<Post>::fromList(m_posts.values());
        std::sort(posts.begin(), posts.end(), [](const Post& lhs, const Post& rhs) {
            return lhs.createdAt > rhs.createdAt;
        });

        int skipped = 0;
        for (Post post : posts) {
            if (request.followOnly && !post.isFollowedAuthor) {
                continue;
            }
            if (skipped < request.offset) {
                ++skipped;
                continue;
            }
            result.push_back(buildSummary(post));
        }
        return result;
    };

    return RepositoryFunctionOperation<PostFeedRequest, QVector<PostSummary>, decltype(handler)>(handler)
            .request(query);
}

PostDetailData PostRepository::requestPostDetail(const PostDetailRequest& query) const
{
    auto handler = [this](const PostDetailRequest& request) {
        QMutexLocker locker(&mutex);
        Post post = m_posts.value(request.postId);
        if (post.postID.isEmpty()) {
            return PostDetailData{};
        }

        if (const auto likeIt = m_likeStates.constFind(post.postID);
            likeIt != m_likeStates.constEnd()) {
            post.likes = likeIt->likes;
            post.isLiked = likeIt->isLiked;
        }
        post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
        const AuthorIdentity author = authorIdentityForUserId(post.authorID,
                                                              post.authorName,
                                                              post.authorAvatarPath);
        return PostDetailData{
                post.postID,
                post.title,
                post.content,
                post.authorID,
                author.name,
                author.avatarPath,
                post.picturesPath,
                post.likes,
                post.commentCount,
                post.isLiked,
                post.isFollowedAuthor,
                post.createdAt,
                post.contentCreatedAt
        };
    };

    return RepositoryFunctionOperation<PostDetailRequest, PostDetailData, decltype(handler)>(handler)
            .request(query);
}

QString PostRepository::requestPostDetailAsync(const PostDetailRequest& query, int delayMs)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, query, delayMs]() {
        const int boundedDelayMs = qMax(0, delayMs);
        if (boundedDelayMs > 0) {
            QThread::msleep(static_cast<unsigned long>(boundedDelayMs));
        }

        const PostDetailData detail = requestPostDetail(query);
        QMetaObject::invokeMethod(this, [this, requestId, detail]() {
            emit postDetailReady(requestId, detail);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

bool PostRepository::setPostLiked(const QString& postId, bool liked)
{
    PostSummary summary;

    {
        QMutexLocker locker(&mutex);
        Post post = m_posts.value(postId);
        if (post.postID.isEmpty()) {
            return false;
        }

        if (post.isLiked != liked) {
            post.isLiked = liked;
            post.likes = qMax(0, post.likes + (liked ? 1 : -1));
            m_likeStates.insert(post.postID, {post.likes, post.isLiked});
            LocalDataStore::instance().upsertValue(QStringLiteral("post_like_states"),
                                                   post.postID,
                                                   postLikeStateToJson(post.postID, post.likes, post.isLiked));
        }
        summary = buildSummary(post);
    }

    emit postUpdated(summary);
    return true;
}

bool PostRepository::adjustPostCommentCount(const QString& postId, int delta)
{
    if (postId.isEmpty() || delta == 0) {
        return false;
    }

    PostSummary summary;
    {
        QMutexLocker locker(&mutex);
        Post post = m_posts.value(postId);
        if (post.postID.isEmpty()) {
            return false;
        }

        const int nextCount = qMax(0, post.commentCount + delta);
        m_commentCountDeltas.insert(postId,
                                    m_commentCountDeltas.value(postId, 0) + nextCount - post.commentCount);
        LocalDataStore::instance().upsertValue(QStringLiteral("post_comment_count_deltas"),
                                               postId,
                                               {{QStringLiteral("postId"), postId},
                                                {QStringLiteral("delta"), m_commentCountDeltas.value(postId)}});
        post.commentCount = nextCount;
        summary = buildSummary(post);
    }

    emit postUpdated(summary);
    return true;
}

void PostRepository::refreshAuthorFollowState(const QString& authorId)
{
    if (authorId.isEmpty()) {
        return;
    }

    QVector<PostSummary> updatedPosts;
    {
        QMutexLocker locker(&mutex);
        for (Post post : m_posts) {
            if (post.authorID == authorId) {
                if (const auto likeIt = m_likeStates.constFind(post.postID);
                    likeIt != m_likeStates.constEnd()) {
                    post.likes = likeIt->likes;
                    post.isLiked = likeIt->isLiked;
                }
                post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
                updatedPosts.push_back(buildSummary(post));
            }
        }
    }

    for (const PostSummary& summary : updatedPosts) {
        emit postUpdated(summary);
    }
}

QVector<PostSummary> PostRepository::upsertPostsFromJson(const QJsonArray& posts)
{
    QVector<PostSummary> summaries;
    QVector<PostSummary> emittedSummaries;
    {
        QMutexLocker locker(&mutex);
        summaries.reserve(posts.size());
        for (const QJsonValue& value : posts) {
            const QJsonObject object = value.toObject();
            Post post = postFromJson(object);
            if (post.postID.isEmpty()) {
                continue;
            }

            if (const auto likeIt = m_likeStates.constFind(post.postID);
                likeIt != m_likeStates.constEnd()) {
                post.likes = likeIt->likes;
                post.isLiked = likeIt->isLiked;
            }
            post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
            m_posts.insert(post.postID, post);
            LocalDataStore::instance().upsertValue(QStringLiteral("posts"), post.postID, object);
            const PostSummary summary = buildSummary(post);
            summaries.push_back(summary);
            emittedSummaries.push_back(summary);
        }
    }

    for (const PostSummary& summary : emittedSummaries) {
        emit postUpdated(summary);
    }
    return summaries;
}

PostDetailData PostRepository::upsertPostFromJson(const QJsonObject& object)
{
    QJsonObject postObject = object;
    if (postObject.value(QStringLiteral("post")).isObject()) {
        postObject = postObject.value(QStringLiteral("post")).toObject();
    }

    PostDetailData detail;
    PostSummary summary;
    {
        QMutexLocker locker(&mutex);
        Post post = postFromJson(postObject);
        if (post.postID.isEmpty()) {
            return {};
        }

        if (const auto likeIt = m_likeStates.constFind(post.postID);
            likeIt != m_likeStates.constEnd()) {
            post.likes = likeIt->likes;
            post.isLiked = likeIt->isLiked;
        }
        post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
        m_posts.insert(post.postID, post);
        LocalDataStore::instance().upsertValue(QStringLiteral("posts"), post.postID, postObject);

        summary = buildSummary(post);
        const AuthorIdentity author = authorIdentityForUserId(post.authorID,
                                                              post.authorName,
                                                              post.authorAvatarPath);
        detail = PostDetailData{
                post.postID,
                post.title,
                post.content,
                post.authorID,
                author.name,
                author.avatarPath,
                post.picturesPath,
                post.likes,
                post.commentCount,
                post.isLiked,
                post.isFollowedAuthor,
                post.createdAt,
                post.contentCreatedAt
        };
    }

    emit postUpdated(summary);
    return detail;
}

bool PostRepository::applyPostLikeResult(const QString& postId, const QJsonObject& object)
{
    PostSummary summary;
    {
        QMutexLocker locker(&mutex);
        Post post = m_posts.value(postId);
        if (post.postID.isEmpty()) {
            return false;
        }

        post.likes = object.value(QStringLiteral("likeCount")).toInt(post.likes);
        post.isLiked = object.value(QStringLiteral("isLiked")).toBool(post.isLiked);
        m_posts.insert(post.postID, post);
        m_likeStates.insert(post.postID, {post.likes, post.isLiked});
        LocalDataStore::instance().upsertValue(QStringLiteral("post_like_states"),
                                               post.postID,
                                               postLikeStateToJson(post.postID, post.likes, post.isLiked));
        summary = buildSummary(post);
    }

    emit postUpdated(summary);
    return true;
}

bool PostRepository::applyAuthorFollowState(const QString& authorId, bool followed)
{
    if (authorId.isEmpty()) {
        return false;
    }

    QVector<PostSummary> updatedPosts;
    {
        QMutexLocker locker(&mutex);
        for (auto it = m_posts.begin(); it != m_posts.end(); ++it) {
            if (it->authorID != authorId) {
                continue;
            }
            it->isFollowedAuthor = followed;
            updatedPosts.push_back(buildSummary(*it));
        }
    }

    for (const PostSummary& summary : updatedPosts) {
        emit postUpdated(summary);
    }
    return !updatedPosts.isEmpty();
}

PostSummary PostRepository::buildSummary(const Post& post) const
{
    const AuthorIdentity author = authorIdentityForUserId(post.authorID,
                                                          post.authorName,
                                                          post.authorAvatarPath);
    return PostSummary{
            post.postID,
            post.title,
            post.thumbnailPath,
            post.thumbnailSize,
            post.authorID,
            author.name,
            author.avatarPath,
            post.likes,
            post.commentCount,
            post.isLiked,
            post.isFollowedAuthor,
            post.createdAt
    };
}
