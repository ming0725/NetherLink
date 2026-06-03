#pragma once

#include <QMap>
#include <QMutex>
#include <QObject>

#include "shared/types/RepositoryTypes.h"

class PostCommentRepository : public QObject
{
    Q_OBJECT

public:
    static PostCommentRepository& instance();

    PostCommentsPage requestPostComments(const PostCommentsRequest& query) const;
    QString requestPostCommentsAsync(const PostCommentsRequest& query, int delayMs = 0);
    bool setCommentLiked(const QString& commentId, bool liked);
    bool setReplyLiked(const QString& replyId, bool liked);

signals:
    void postCommentsReady(const QString& requestId, const PostCommentsPage& page);

private:
    explicit PostCommentRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostCommentRepository)

    struct LikeState {
        int likes = 0;
        bool isLiked = false;
    };

    QMap<QString, PostComment> m_comments;
    QMap<QString, LikeState> m_commentLikeStates;
    QMap<QString, LikeState> m_replyLikeStates;
    mutable QMutex m_mutex;
};
