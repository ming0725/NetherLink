#include "MessageRemoteDataSource.h"

#include "shared/network/HttpClient.h"
#include "shared/network/ReferenceDataResolver.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>
#include <QVariantMap>

namespace {

QJsonArray messageArrayFromResponse(const QJsonObject& object)
{
    for (const QString& key : {QStringLiteral("messages"),
                               QStringLiteral("items"),
                               QStringLiteral("results"),
                               QStringLiteral("data")}) {
        if (object.value(key).isArray()) {
            return object.value(key).toArray();
        }
    }
    if (object.value(QStringLiteral("data")).isObject()) {
        return messageArrayFromResponse(object.value(QStringLiteral("data")).toObject());
    }
    return {};
}

bool hasMoreAfterFromResponse(const QJsonObject& object)
{
    if (object.contains(QStringLiteral("hasMoreAfter"))) {
        return object.value(QStringLiteral("hasMoreAfter")).toBool();
    }
    if (object.value(QStringLiteral("data")).isObject()) {
        return hasMoreAfterFromResponse(object.value(QStringLiteral("data")).toObject());
    }
    return false;
}

} // namespace

MessageRemoteDataSource& MessageRemoteDataSource::instance()
{
    static MessageRemoteDataSource dataSource;
    return dataSource;
}

MessageRemoteDataSource::MessageRemoteDataSource(QObject* parent)
    : QObject(parent)
{
}

MessageRemoteDataSource::FetchResult MessageRemoteDataSource::fetchLatestMessagesBlocking(
        const QString& conversationId,
        int limit)
{
    return fetchMessagesBlocking(conversationId, 0, 0, limit);
}

MessageRemoteDataSource::FetchResult MessageRemoteDataSource::fetchOlderMessagesBlocking(
        const QString& conversationId,
        int beforeMessageSeq,
        int limit)
{
    return fetchMessagesBlocking(conversationId, beforeMessageSeq, 0, limit);
}

MessageRemoteDataSource::FetchResult MessageRemoteDataSource::fetchNewerMessagesBlocking(
        const QString& conversationId,
        int afterMessageSeq,
        int limit)
{
    return fetchMessagesBlocking(conversationId, 0, afterMessageSeq, limit);
}

MessageRemoteDataSource::FetchResult MessageRemoteDataSource::fetchMessagesBlocking(
        const QString& conversationId,
        int beforeMessageSeq,
        int afterMessageSeq,
        int limit)
{
    FetchResult result;
    if (conversationId.isEmpty() || limit <= 0) {
        return result;
    }

    QVariantMap query{{QStringLiteral("limit"), limit}};
    if (beforeMessageSeq > 0) {
        query.insert(QStringLiteral("beforeMessageSeq"), beforeMessageSeq);
    }
    if (afterMessageSeq > 0) {
        query.insert(QStringLiteral("afterMessageSeq"), afterMessageSeq);
    }

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Get,
            QStringLiteral("/conversations/%1/messages").arg(conversationId),
            {},
            query);
    request.maxRetries = 3;

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QString requestId;
    QMetaObject::Connection succeededConnection;
    QMetaObject::Connection failedConnection;

    succeededConnection = connect(&HttpClient::instance(),
                                  &HttpClient::requestSucceeded,
                                  &loop,
                                  [&](const QString& completedRequestId, const NetworkResponse& response) {
                                      if (completedRequestId != requestId) {
                                          return;
                                      }

                                      ReferenceDataResolver::instance().consumePayload(response.object());
                                      const QJsonObject responseObject = response.object();
                                      const QJsonArray messages = messageArrayFromResponse(responseObject);
                                      result.messages.reserve(messages.size());
                                      for (const QJsonValue& value : messages) {
                                          QJsonObject object = value.toObject();
                                          if (!object.contains(QStringLiteral("conversationId"))) {
                                              object.insert(QStringLiteral("conversationId"), conversationId);
                                          }
                                          result.messages.push_back(object);
                                      }
                                      result.hasMoreAfter = hasMoreAfterFromResponse(responseObject);
                                      result.completed = true;
                                      loop.quit();
                                  });
    failedConnection = connect(&HttpClient::instance(),
                               &HttpClient::requestFailed,
                               &loop,
                               [&](const QString& completedRequestId, const NetworkError&) {
                                   if (completedRequestId != requestId) {
                                       return;
                                   }
                                   result.completed = true;
                                   loop.quit();
                               });
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        loop.quit();
    });

    requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        disconnect(succeededConnection);
        disconnect(failedConnection);
        return result;
    }

    timeoutTimer.start(request.timeoutMs + 1000);
    loop.exec();
    disconnect(succeededConnection);
    disconnect(failedConnection);
    return result;
}
