#pragma once

#include <QHash>
#include <QObject>

#include "shared/types/RepositoryTypes.h"

class PostSessionController : public QObject
{
    Q_OBJECT

public:
    explicit PostSessionController(QObject* parent = nullptr);

    QVector<PostSummary> loadFeedPage(int offset, int limit, bool followOnly = false) const;
    QString requestFeedPage(int offset, int limit, bool followOnly = false);
    void openPost(const PostSummary& summary);
    void closePost();
    QString currentPostId() const;

    QString requestPostComments(const QString& postId, int offset, int limit);

    bool setCurrentPostLiked(bool liked);
    bool setPostLiked(const QString& postId, bool liked);
    bool setAuthorFollowed(const QString& authorId, bool followed);
    bool setCommentLiked(const QString& commentId, bool liked);
    bool setReplyLiked(const QString& commentId, const QString& replyId, bool liked);
    bool adjustCurrentPostCommentCount(int delta);
    bool createComment(const QString& postId, const QString& text);
    bool createReply(const QString& commentId,
                     const QString& text,
                     const QString& targetUserId = {},
                     const QString& targetReplyId = {});

signals:
    void postUpdated(const PostSummary& summary);
    void currentPostUpdated(const PostSummary& summary);
    void currentPostDetailLoaded(const PostDetailData& detail);
    void postFeedPageLoaded(const QString& requestId,
                            int offset,
                            int limit,
                            bool followOnly,
                            const QVector<PostSummary>& posts,
                            bool hasMore);
    void postCommentsLoaded(const QString& requestId, const PostCommentsPage& page);
    void commentLikeUpdated(const QString& commentId, bool liked);
    void replyLikeUpdated(const QString& commentId, const QString& replyId, bool liked);
    void commentCreated(const PostComment& comment);
    void replyCreated(const QString& commentId, const QString& targetReplyId, const PostCommentReply& reply);
    void postOperationFailed(const QString& operation, const QString& targetId);

private:
    QString m_currentPostId;
    QString m_currentDetailRequestId;
    QHash<QString, QString> m_replyCommentByRequestId;
    QHash<QString, QString> m_replyTargetByRequestId;
};
