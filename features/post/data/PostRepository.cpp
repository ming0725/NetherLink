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

constexpr int kSamplePostCount = 160;

struct AuthorIdentity {
    QString name;
    QString avatarPath;
};

AuthorIdentity authorIdentityForUserId(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return {currentUser.getUserName(), currentUser.getAvatarPath()};
    }

    const User author = UserRepository::instance().requestUserDetail({userId});
    return {author.nick, author.avatarPath};
}

const QStringList& postTitleSamples()
{
    static const QStringList samples = []() {
        QStringList result;
        const QJsonArray array = LocalDataStore::instance()
                .seedObject(QStringLiteral(":/resources/data/post_samples.json"))
                .value(QStringLiteral("titles")).toArray();
        for (const QJsonValue& value : array) {
            result.append(value.toString());
        }
        return result.isEmpty() ? QStringList{QStringLiteral("新帖子")} : result;
    }();
    return samples;
}

const QStringList& postContentSamples()
{
    static const QStringList samples = []() {
        QStringList result;
        const QJsonArray array = LocalDataStore::instance()
                .seedObject(QStringLiteral(":/resources/data/post_samples.json"))
                .value(QStringLiteral("contents")).toArray();
        for (const QJsonValue& value : array) {
            result.append(value.toString());
        }
        return result.isEmpty() ? QStringList{QStringLiteral("帖子内容")} : result;
    }();
    return samples;
}

const QStringList& weightedPostAuthorIds()
{
    static const QStringList authorIds = []() {
        QStringList ids;
        const auto add = [&ids](const QString& userId, int count) {
            for (int i = 0; i < count; ++i) {
                ids.append(userId);
            }
        };

        const QJsonObject weights = LocalDataStore::instance()
                .seedObject(QStringLiteral(":/resources/data/post_samples.json"))
                .value(QStringLiteral("authorWeights")).toObject();
        for (auto it = weights.constBegin(); it != weights.constEnd(); ++it) {
            add(it.key(), qMax(1, it.value().toInt(1)));
        }
        if (ids.isEmpty()) {
            add(QStringLiteral("u001"), 1);
        }
        return ids;
    }();
    return authorIds;
}

QJsonObject postLikeStateToJson(const QString& postId, int likes, bool isLiked)
{
    return {
            {QStringLiteral("postId"), postId},
            {QStringLiteral("likes"), likes},
            {QStringLiteral("isLiked"), isLiked}
    };
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

} // namespace

PostRepository::PostRepository(QObject* parent)
        : QObject(parent)
{
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

        result.reserve(request.limit);
        int skipped = 0;
        for (int index = 0; index < kSamplePostCount && result.size() < request.limit; ++index) {
            const Post post = buildPostAt(index);
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
        const int index = postIndexForId(request.postId);
        if (index < 0) {
            return PostDetailData{};
        }

        const Post post = buildPostAt(index);
        const AuthorIdentity author = authorIdentityForUserId(post.authorID);
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
        const int index = postIndexForId(postId);
        if (index < 0) {
            return false;
        }

        Post post = buildPostAt(index);
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
        const int index = postIndexForId(postId);
        if (index < 0) {
            return false;
        }

        Post post = buildPostAt(index);
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
        for (int index = 0; index < kSamplePostCount; ++index) {
            const Post post = buildPostAt(index);
            if (post.authorID == authorId) {
                updatedPosts.push_back(buildSummary(post));
            }
        }
    }

    for (const PostSummary& summary : updatedPosts) {
        emit postUpdated(summary);
    }
}

Post PostRepository::buildPostAt(int index) const
{
    if (index < 0 || index >= kSamplePostCount) {
        return {};
    }

    const QDateTime baseTime = QDateTime::fromString("2024-05-21T18:00:00", Qt::ISODate);
    const QStringList& authorIDs = weightedPostAuthorIds();

    Post post;
    post.postID = QString("p%1").arg(index + 1, 3, 10, QChar('0'));
    post.title = postTitleSamples().at((index * 5 + 2) % postTitleSamples().size());
    post.content = postContentSamples().at((index * 7 + 1) % postContentSamples().size());
    post.likes = (index * 137 + 211) % 1000;
    post.commentCount = (index * 29 + 17) % 200;
    post.authorID = authorIDs.at((index * 17 + index / 3) % authorIDs.size());
    post.isFollowedAuthor = UserRepository::instance().isFriend(post.authorID);
    post.createdAt = baseTime.addSecs(-index * 1800);
    post.contentCreatedAt = post.createdAt;
    const int imageSeed = (index * 7 + 3) % 10;
    const QString imagePath = QString(":/resources/post/%1.jpg").arg(imageSeed);
    post.thumbnailPath = imagePath;
    post.thumbnailSize = imageSizeForSource(imagePath);
    const int pictureCount = 2 + (index % 3);
    post.picturesPath.reserve(pictureCount);
    for (int pictureIndex = 0; pictureIndex < pictureCount; ++pictureIndex) {
        post.picturesPath.append(QString(":/resources/post/%1.jpg")
                                         .arg((imageSeed + pictureIndex * 3) % 10));
    }

    const auto likeIt = m_likeStates.constFind(post.postID);
    if (likeIt != m_likeStates.constEnd()) {
        post.likes = likeIt->likes;
        post.isLiked = likeIt->isLiked;
    }
    post.commentCount = qMax(0, post.commentCount + m_commentCountDeltas.value(post.postID, 0));
    return post;
}

int PostRepository::postIndexForId(const QString& postId) const
{
    if (!postId.startsWith(QLatin1Char('p'))) {
        return -1;
    }

    bool ok = false;
    const int serial = postId.mid(1).toInt(&ok);
    const int index = serial - 1;
    return ok && index >= 0 && index < kSamplePostCount ? index : -1;
}

PostSummary PostRepository::buildSummary(const Post& post) const
{
    const AuthorIdentity author = authorIdentityForUserId(post.authorID);
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
