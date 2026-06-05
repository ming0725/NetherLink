#include "PostRemoteDataSource.h"

#include "shared/network/HttpClient.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

namespace {

QString newClientOperationId(const QString& prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject bodyWithClientOperationId(const QString& prefix)
{
    return {
            {QStringLiteral("clientOperationId"), newClientOperationId(prefix)}
    };
}

} // namespace

PostRemoteDataSource& PostRemoteDataSource::instance()
{
    static PostRemoteDataSource dataSource;
    return dataSource;
}

PostRemoteDataSource::PostRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &PostRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &PostRemoteDataSource::handleRequestFailed);
}

QString PostRemoteDataSource::fetchFeed(int offset, int limit, bool followOnly)
{
    if (limit <= 0 || offset < 0) {
        return {};
    }

    NetworkRequest request = NetworkRequest::json(HttpMethod::Get,
                                                  QStringLiteral("/posts"),
                                                  {},
                                                  {{QStringLiteral("offset"), offset},
                                                   {QStringLiteral("limit"), limit},
                                                   {QStringLiteral("followOnly"), followOnly}});
    return send(request, PendingOperation{Operation::FetchFeed, {}, {}, offset, limit, followOnly});
}

QString PostRemoteDataSource::fetchPostDetail(const QString& postId)
{
    if (postId.isEmpty()) {
        return {};
    }

    return send(NetworkRequest::json(HttpMethod::Get,
                                     QStringLiteral("/posts/%1").arg(postId)),
                PendingOperation{Operation::FetchDetail, postId});
}

QString PostRemoteDataSource::fetchComments(const QString& postId, int offset, int limit)
{
    if (postId.isEmpty() || offset < 0 || limit <= 0) {
        return {};
    }

    NetworkRequest request = NetworkRequest::json(HttpMethod::Get,
                                                  QStringLiteral("/posts/%1/comments").arg(postId),
                                                  {},
                                                  {{QStringLiteral("offset"), offset},
                                                   {QStringLiteral("limit"), limit}});
    return send(request, PendingOperation{Operation::FetchComments, postId, {}, offset, limit});
}

QString PostRemoteDataSource::setPostLiked(const QString& postId, bool liked)
{
    if (postId.isEmpty()) {
        return {};
    }

    return send(NetworkRequest::json(HttpMethod::Post,
                                     QStringLiteral("/posts/%1/like").arg(postId),
                                     {{QStringLiteral("liked"), liked}}),
                PendingOperation{Operation::LikePost, postId, {}, 0, 0, liked});
}

QString PostRemoteDataSource::setAuthorFollowed(const QString& authorId, bool followed)
{
    if (authorId.isEmpty()) {
        return {};
    }

    return send(NetworkRequest::json(HttpMethod::Post,
                                     QStringLiteral("/users/%1/follow").arg(authorId),
                                     {{QStringLiteral("followed"), followed}}),
                PendingOperation{Operation::FollowAuthor, authorId, {}, 0, 0, followed});
}

QString PostRemoteDataSource::setCommentLiked(const QString& commentId, bool liked)
{
    if (commentId.isEmpty()) {
        return {};
    }

    return send(NetworkRequest::json(HttpMethod::Post,
                                     QStringLiteral("/comments/%1/like").arg(commentId),
                                     {{QStringLiteral("liked"), liked}}),
                PendingOperation{Operation::LikeComment, commentId, {}, 0, 0, liked});
}

QString PostRemoteDataSource::setReplyLiked(const QString& replyId, bool liked)
{
    if (replyId.isEmpty()) {
        return {};
    }

    return send(NetworkRequest::json(HttpMethod::Post,
                                     QStringLiteral("/replies/%1/like").arg(replyId),
                                     {{QStringLiteral("liked"), liked}}),
                PendingOperation{Operation::LikeReply, replyId, {}, 0, 0, liked});
}

QString PostRemoteDataSource::createComment(const QString& postId, const QString& content)
{
    const QString trimmed = content.trimmed();
    if (postId.isEmpty() || trimmed.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(QStringLiteral("op_post_comment"));
    body.insert(QStringLiteral("content"), trimmed);
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/posts/%1/comments").arg(postId),
                                                  body);
    request.maxRetries = 3;
    return send(request, PendingOperation{Operation::CreateComment, postId});
}

QString PostRemoteDataSource::createReply(const QString& commentId,
                                          const QString& content,
                                          const QString& targetUserId,
                                          const QString& targetReplyId)
{
    const QString trimmed = content.trimmed();
    if (commentId.isEmpty() || trimmed.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(QStringLiteral("op_post_reply"));
    body.insert(QStringLiteral("content"), trimmed);
    body.insert(QStringLiteral("targetUserUuid"),
                targetUserId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(targetUserId));
    body.insert(QStringLiteral("targetReplyId"),
                targetReplyId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(targetReplyId));
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/comments/%1/replies").arg(commentId),
                                                  body);
    request.maxRetries = 3;
    return send(request, PendingOperation{Operation::CreateReply, commentId});
}

QString PostRemoteDataSource::send(NetworkRequest request, PendingOperation pending)
{
    const QString requestId = HttpClient::instance().send(request);
    if (!requestId.isEmpty()) {
        m_pendingOperations.insert(requestId, pending);
    }
    return requestId;
}

QString PostRemoteDataSource::operationName(Operation operation) const
{
    switch (operation) {
    case Operation::FetchFeed:
        return QStringLiteral("fetch_feed");
    case Operation::FetchDetail:
        return QStringLiteral("fetch_detail");
    case Operation::FetchComments:
        return QStringLiteral("fetch_comments");
    case Operation::LikePost:
        return QStringLiteral("like_post");
    case Operation::FollowAuthor:
        return QStringLiteral("follow_author");
    case Operation::LikeComment:
        return QStringLiteral("like_comment");
    case Operation::LikeReply:
        return QStringLiteral("like_reply");
    case Operation::CreateComment:
        return QStringLiteral("create_comment");
    case Operation::CreateReply:
        return QStringLiteral("create_reply");
    }
    return QStringLiteral("post_operation");
}

void PostRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    const QJsonObject object = response.object();
    switch (pending.operation) {
    case Operation::FetchFeed:
        emit feedFetched(requestId, pending.offset, pending.limit, pending.flag, object);
        break;
    case Operation::FetchDetail:
        emit postDetailFetched(requestId, pending.targetId, object);
        break;
    case Operation::FetchComments:
        emit commentsFetched(requestId, pending.targetId, pending.offset, pending.limit, object);
        break;
    case Operation::LikePost:
        emit postLikeUpdated(requestId, pending.targetId, object);
        break;
    case Operation::FollowAuthor:
        emit authorFollowUpdated(requestId, pending.targetId, pending.flag, object);
        break;
    case Operation::LikeComment:
        emit commentLikeUpdated(requestId, pending.targetId, object);
        break;
    case Operation::LikeReply:
        emit replyLikeUpdated(requestId, pending.targetId, object);
        break;
    case Operation::CreateComment:
        emit commentCreated(requestId, pending.targetId, object);
        break;
    case Operation::CreateReply:
        emit replyCreated(requestId, pending.targetId, object);
        break;
    }
}

void PostRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    emit operationFailed(requestId, operationName(pending.operation), pending.targetId, error);
}
