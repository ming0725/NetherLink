#pragma once

#include <QMap>
#include <QMutex>
#include <QObject>

#include "shared/types/RepositoryTypes.h"

class QJsonArray;
class QJsonObject;

class PostCommentRepository : public QObject
{
    Q_OBJECT

public:
    static PostCommentRepository& instance();

    PostCommentsPage requestPostComments(const PostCommentsRequest& query) const;
    QString requestPostCommentsAsync(const PostCommentsRequest& query, int delayMs = 0);
    bool setCommentLiked(const QString& commentId, bool liked);
    bool setReplyLiked(const QString& replyId, bool liked);
    PostCommentsPage upsertCommentsFromJson(const QString& postId,
                                            const QJsonArray& comments,
                                            int offset,
                                            int totalCount,
                                            bool hasMore);
    PostComment upsertCommentFromJson(const QString& postId, const QJsonObject& object);
    PostCommentReply upsertReplyFromJson(const QString& commentId, const QJsonObject& object);
    bool applyCommentLikeResult(const QString& commentId, const QJsonObject& object);
    bool applyReplyLikeResult(const QString& replyId, const QJsonObject& object);

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
