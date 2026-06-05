#pragma once

#include "shared/network/NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

class PostRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static PostRemoteDataSource& instance();

    QString fetchFeed(int offset, int limit, bool followOnly);
    QString fetchPostDetail(const QString& postId);
    QString fetchComments(const QString& postId, int offset, int limit);
    QString setPostLiked(const QString& postId, bool liked);
    QString setAuthorFollowed(const QString& authorId, bool followed);
    QString setCommentLiked(const QString& commentId, bool liked);
    QString setReplyLiked(const QString& replyId, bool liked);
    QString createComment(const QString& postId, const QString& content);
    QString createReply(const QString& commentId,
                        const QString& content,
                        const QString& targetUserId = {},
                        const QString& targetReplyId = {});

signals:
    void feedFetched(const QString& requestId,
                     int offset,
                     int limit,
                     bool followOnly,
                     const QJsonObject& response);
    void postDetailFetched(const QString& requestId,
                           const QString& postId,
                           const QJsonObject& response);
    void commentsFetched(const QString& requestId,
                         const QString& postId,
                         int offset,
                         int limit,
                         const QJsonObject& response);
    void postLikeUpdated(const QString& requestId,
                         const QString& postId,
                         const QJsonObject& response);
    void authorFollowUpdated(const QString& requestId,
                             const QString& authorId,
                             bool followed,
                             const QJsonObject& response);
    void commentLikeUpdated(const QString& requestId,
                            const QString& commentId,
                            const QJsonObject& response);
    void replyLikeUpdated(const QString& requestId,
                          const QString& replyId,
                          const QJsonObject& response);
    void commentCreated(const QString& requestId,
                        const QString& postId,
                        const QJsonObject& response);
    void replyCreated(const QString& requestId,
                      const QString& commentId,
                      const QJsonObject& response);
    void operationFailed(const QString& requestId,
                         const QString& operation,
                         const QString& targetId,
                         const NetworkError& error);

private:
    enum class Operation {
        FetchFeed,
        FetchDetail,
        FetchComments,
        LikePost,
        FollowAuthor,
        LikeComment,
        LikeReply,
        CreateComment,
        CreateReply
    };

    struct PendingOperation {
        Operation operation = Operation::FetchFeed;
        QString targetId;
        QString parentId;
        int offset = 0;
        int limit = 0;
        bool flag = false;
    };

    explicit PostRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRemoteDataSource)

    QString send(NetworkRequest request, PendingOperation pending);
    QString operationName(Operation operation) const;
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, PendingOperation> m_pendingOperations;
};
