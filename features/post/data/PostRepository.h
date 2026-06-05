#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QMap>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/Post.h"

class QJsonArray;
class QJsonObject;

class PostRepository : public QObject {
    Q_OBJECT
public:
    static PostRepository& instance();
    QVector<PostSummary> requestPostFeed(const PostFeedRequest& query = {}) const;
    PostDetailData requestPostDetail(const PostDetailRequest& query) const;
    QString requestPostDetailAsync(const PostDetailRequest& query, int delayMs = 120);
    bool setPostLiked(const QString& postId, bool liked);
    bool adjustPostCommentCount(const QString& postId, int delta);
    void refreshAuthorFollowState(const QString& authorId);
    QVector<PostSummary> upsertPostsFromJson(const QJsonArray& posts);
    PostDetailData upsertPostFromJson(const QJsonObject& object);
    bool applyPostLikeResult(const QString& postId, const QJsonObject& object);
    bool applyAuthorFollowState(const QString& authorId, bool followed);

signals:
    void postUpdated(const PostSummary& summary);
    void postDetailReady(const QString& requestId, const PostDetailData& detail);

private:
    explicit PostRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRepository)

    struct PostLikeState {
        int likes = 0;
        bool isLiked = false;
    };

    PostSummary buildSummary(const Post& post) const;

    QMap<QString, Post> m_posts;
    QMap<QString, PostLikeState> m_likeStates;
    QMap<QString, int> m_commentCountDeltas;
    mutable QMutex mutex;
};
