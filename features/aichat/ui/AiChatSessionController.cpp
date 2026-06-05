#include "features/aichat/ui/AiChatSessionController.h"

#include "features/aichat/data/AiChatRepository.h"
#include "features/aichat/data/AiChatStreamClient.h"
#include "shared/network/HttpClient.h"
#include "shared/network/NetworkTypes.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {

constexpr int kCreateConversationTimeoutMs = 20000;
constexpr int kConversationMutationTimeoutMs = 15000;
constexpr int kTemporaryTitleMaxLength = 80;

QString newClientOperationId()
{
    return QStringLiteral("op_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool isUuid(const QString& value)
{
    return !QUuid::fromString(value).isNull();
}

QDateTime dateTimeFromString(const QString& value)
{
    QDateTime time = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(value, Qt::ISODate);
    }
    return time;
}

QString temporaryTitleFromUserMessage(const QString& message)
{
    QString title = message.trimmed();
    const int lineBreak = title.indexOf(QLatin1Char('\n'));
    if (lineBreak >= 0) {
        title = title.left(lineBreak).trimmed();
    }
    title = title.simplified();
    if (title.size() > kTemporaryTitleMaxLength) {
        title = title.left(kTemporaryTitleMaxLength).trimmed() + QStringLiteral("...");
    }
    return title.isEmpty() ? QStringLiteral("新对话") : title;
}

QJsonArray arrayFromResponse(const NetworkResponse& response, const QString& preferredKey)
{
    if (response.body.isArray()) {
        return response.body.array();
    }

    const QJsonObject root = response.object();
    const QStringList keys = preferredKey.isEmpty()
            ? QStringList{QStringLiteral("items"), QStringLiteral("data"), QStringLiteral("results"), QStringLiteral("entries"), QStringLiteral("conversations"), QStringLiteral("messages")}
            : QStringList{preferredKey, QStringLiteral("items"), QStringLiteral("data"), QStringLiteral("results"), QStringLiteral("entries"), QStringLiteral("conversations"), QStringLiteral("messages")};
    for (const QString& key : keys) {
        const QJsonValue value = root.value(key);
        if (value.isArray()) {
            return value.toArray();
        }
        if (value.isObject()) {
            const QJsonObject object = value.toObject();
            for (const QString& nestedKey : keys) {
                if (object.value(nestedKey).isArray()) {
                    return object.value(nestedKey).toArray();
                }
            }
        }
    }
    return {};
}

QJsonObject conversationObjectFromResponse(QJsonObject object)
{
    if (object.value(QStringLiteral("conversation")).isObject()) {
        return object.value(QStringLiteral("conversation")).toObject();
    }
    return object;
}

QString conversationIdFromObject(const QJsonObject& object)
{
    return object.value(QStringLiteral("conversationId")).toString(
            object.value(QStringLiteral("id")).toString());
}

QString conversationTitleFromObject(const QJsonObject& object, const QString& fallbackTitle)
{
    const QString title = object.value(QStringLiteral("title")).toString().trimmed();
    return title.isEmpty() ? fallbackTitle : title;
}

QDateTime conversationTimeFromObject(const QJsonObject& object)
{
    QDateTime time = dateTimeFromString(object.value(QStringLiteral("lastMessageAt")).toString(
            object.value(QStringLiteral("updatedAt")).toString(
                    object.value(QStringLiteral("createdAt")).toString())));
    return time.isValid() ? time : QDateTime::currentDateTime();
}

AiChatListEntry conversationEntryFromObject(const QJsonObject& object)
{
    AiChatListEntry entry;
    entry.conversationId = conversationIdFromObject(object);
    entry.title = conversationTitleFromObject(object, QStringLiteral("新对话"));
    entry.time = conversationTimeFromObject(object);
    entry.hasUnreadDot = object.value(QStringLiteral("hasUnreadDot")).toBool(
            object.value(QStringLiteral("unread")).toBool(false));
    return entry;
}

QString aiMessageTextFromObject(const QJsonObject& object)
{
    QString text = object.value(QStringLiteral("text")).toString();
    if (!text.isNull()) {
        return text;
    }

    text = object.value(QStringLiteral("contentText")).toString();
    if (!text.isNull()) {
        return text;
    }

    const QJsonValue contentValue = object.value(QStringLiteral("content"));
    if (contentValue.isString()) {
        return contentValue.toString();
    }
    if (contentValue.isObject()) {
        return contentValue.toObject().value(QStringLiteral("text")).toString();
    }

    return object.value(QStringLiteral("message")).toString();
}

bool aiMessageIsFromUser(const QJsonObject& object)
{
    const QString role = object.value(QStringLiteral("role")).toString(
            object.value(QStringLiteral("senderRole")).toString()).toLower();
    if (!role.isEmpty()) {
        return role == QStringLiteral("user");
    }
    return object.value(QStringLiteral("isFromUser")).toBool(false);
}

QDateTime aiMessageTimeFromObject(const QJsonObject& object)
{
    QDateTime time = dateTimeFromString(object.value(QStringLiteral("createdAt")).toString(
            object.value(QStringLiteral("time")).toString(
                    object.value(QStringLiteral("serverReceivedAt")).toString(
                            object.value(QStringLiteral("updatedAt")).toString()))));
    return time.isValid() ? time : QDateTime::currentDateTime();
}

AiChatMessage aiMessageFromObject(const QJsonObject& object,
                                  const QString& conversationId,
                                  int fallbackIndex)
{
    AiChatMessage message;
    message.messageId = object.value(QStringLiteral("messageId")).toString(
            object.value(QStringLiteral("id")).toString());
    if (message.messageId.isEmpty()) {
        message.messageId = QStringLiteral("remote-ai-message-%1-%2").arg(conversationId).arg(fallbackIndex);
    }
    message.conversationId = object.value(QStringLiteral("conversationId")).toString(conversationId);
    message.text = aiMessageTextFromObject(object);
    message.isFromUser = aiMessageIsFromUser(object);
    message.time = aiMessageTimeFromObject(object);
    return message;
}

AiChatListEntry createRemoteAiConversation(const QString& title)
{
    const QString trimmedTitle = title.trimmed().isEmpty()
            ? QStringLiteral("新对话")
            : title.trimmed();
    QJsonObject body{
            {QStringLiteral("title"), trimmedTitle},
            {QStringLiteral("clientOperationId"), newClientOperationId()}
    };

    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/ai/conversations"),
                                                  body);
    request.maxRetries = 0;
    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return {};
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    AiChatListEntry entry;
    NetworkError failure;
    bool finished = false;

    const QMetaObject::Connection successConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestSucceeded,
            &loop,
            [&](const QString& completedRequestId, const NetworkResponse& response) {
                if (completedRequestId != requestId) {
                    return;
                }

                const QJsonObject object = conversationObjectFromResponse(response.object());
                const QString conversationId = conversationIdFromObject(object);
                if (!isUuid(conversationId)) {
                    failure.code = QStringLiteral("AI_CONVERSATION_ID_INVALID");
                    failure.message = QStringLiteral("AI conversation create response did not include a UUID conversationId.");
                } else {
                    entry.conversationId = conversationId;
                    entry.title = conversationTitleFromObject(object, trimmedTitle);
                    entry.time = conversationTimeFromObject(object);
                    entry.hasUnreadDot = false;
                }
                finished = true;
                loop.quit();
            });
    const QMetaObject::Connection failureConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestFailed,
            &loop,
            [&](const QString& completedRequestId, const NetworkError& error) {
                if (completedRequestId != requestId) {
                    return;
                }

                failure = error;
                finished = true;
                loop.quit();
            });
    const QMetaObject::Connection timeoutConnection = QObject::connect(
            &timeoutTimer,
            &QTimer::timeout,
            &loop,
            [&]() {
                failure.code = QStringLiteral("AI_CONVERSATION_CREATE_TIMEOUT");
                failure.message = QStringLiteral("AI conversation create timed out.");
                loop.quit();
            });

    timeoutTimer.start(kCreateConversationTimeoutMs);
    loop.exec();

    QObject::disconnect(successConnection);
    QObject::disconnect(failureConnection);
    QObject::disconnect(timeoutConnection);

    Q_UNUSED(finished)
    Q_UNUSED(failure)
    return entry;
}

QString patchRemoteAiConversationTitle(const QString& conversationId, const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return {};
    }

    QJsonObject body{
            {QStringLiteral("title"), trimmedTitle},
            {QStringLiteral("clientOperationId"), newClientOperationId()}
    };
    NetworkRequest request = NetworkRequest::json(HttpMethod::Patch,
                                                  QStringLiteral("/ai/conversations/%1").arg(conversationId),
                                                  body);
    request.maxRetries = 0;

    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return {};
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QString appliedTitle;
    const QMetaObject::Connection successConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestSucceeded,
            &loop,
            [&](const QString& completedRequestId, const NetworkResponse& response) {
                if (completedRequestId != requestId) {
                    return;
                }

                const QJsonObject object = conversationObjectFromResponse(response.object());
                appliedTitle = conversationTitleFromObject(object, trimmedTitle);
                loop.quit();
            });
    const QMetaObject::Connection failureConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestFailed,
            &loop,
            [&](const QString& completedRequestId, const NetworkError&) {
                if (completedRequestId == requestId) {
                    loop.quit();
                }
            });
    const QMetaObject::Connection timeoutConnection = QObject::connect(
            &timeoutTimer,
            &QTimer::timeout,
            &loop,
            [&]() { loop.quit(); });

    timeoutTimer.start(kConversationMutationTimeoutMs);
    loop.exec();

    QObject::disconnect(successConnection);
    QObject::disconnect(failureConnection);
    QObject::disconnect(timeoutConnection);

    return appliedTitle;
}

bool deleteRemoteAiConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/ai/conversations/%1").arg(conversationId),
            {},
            {{QStringLiteral("clientOperationId"), newClientOperationId()}});
    request.maxRetries = 0;

    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return false;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    bool deleted = false;
    const QMetaObject::Connection successConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestSucceeded,
            &loop,
            [&](const QString& completedRequestId, const NetworkResponse&) {
                if (completedRequestId != requestId) {
                    return;
                }

                deleted = true;
                loop.quit();
            });
    const QMetaObject::Connection failureConnection = QObject::connect(
            &HttpClient::instance(),
            &HttpClient::requestFailed,
            &loop,
            [&](const QString& completedRequestId, const NetworkError&) {
                if (completedRequestId == requestId) {
                    loop.quit();
                }
            });
    const QMetaObject::Connection timeoutConnection = QObject::connect(
            &timeoutTimer,
            &QTimer::timeout,
            &loop,
            [&]() { loop.quit(); });

    timeoutTimer.start(kConversationMutationTimeoutMs);
    loop.exec();

    QObject::disconnect(successConnection);
    QObject::disconnect(failureConnection);
    QObject::disconnect(timeoutConnection);

    return deleted;
}

} // namespace

AiChatSessionController::AiChatSessionController(QObject* parent)
    : QObject(parent)
    , m_streamClient(new AiChatStreamClient(this))
{
    connect(m_streamClient, &AiChatStreamClient::chunkReceived,
            this, &AiChatSessionController::onAiReplyChunkReceived);
    connect(m_streamClient, &AiChatStreamClient::titleReceived,
            this, &AiChatSessionController::onGeneratedTitleReceived);
    connect(m_streamClient, &AiChatStreamClient::assistantMessageReceived,
            this, &AiChatSessionController::onAssistantMessageReceived);
    connect(m_streamClient, &AiChatStreamClient::cancelled,
            this, &AiChatSessionController::onAiReplyCanceled);
    connect(m_streamClient, &AiChatStreamClient::failed,
            this, &AiChatSessionController::onAiReplyFailed);
    connect(m_streamClient, &AiChatStreamClient::finished,
            this, &AiChatSessionController::onAiReplyFinished);
    connect(&HttpClient::instance(), &HttpClient::requestSucceeded,
            this, &AiChatSessionController::onConversationListRequestSucceeded);
    connect(&HttpClient::instance(), &HttpClient::requestFailed,
            this, &AiChatSessionController::onConversationListRequestFailed);
    connect(&HttpClient::instance(), &HttpClient::requestSucceeded,
            this, &AiChatSessionController::onMessageHistoryRequestSucceeded);
    connect(&HttpClient::instance(), &HttpClient::requestFailed,
            this, &AiChatSessionController::onMessageHistoryRequestFailed);
    connect(&AiChatRepository::instance(), &AiChatRepository::unreadDotStateChanged,
            this, [this]() {
                emit unreadDotStateChanged();
                emit conversationsChanged();
            });
}

QVector<AiChatListEntry> AiChatSessionController::loadConversations(const AiChatListRequest& query) const
{
    return AiChatRepository::instance().requestAiChatList(query);
}

QVector<AiChatMessage> AiChatSessionController::loadMessages(const QString& conversationId) const
{
    return AiChatRepository::instance().requestAiChatMessages(conversationId);
}

AiChatContextUsage AiChatSessionController::loadContextUsage(const AiChatContextUsageRequest& request) const
{
    return AiChatRepository::instance().requestAiChatContextUsage(request);
}

int AiChatSessionController::loadConversationsAsync(const AiChatListRequest& query)
{
    const int requestId = m_nextAsyncRequestId++;

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Get,
            QStringLiteral("/ai/conversations"),
            {},
            {{QStringLiteral("offset"), query.offset}, {QStringLiteral("limit"), query.limit}});
    request.maxRetries = 3;

    const QString httpRequestId = HttpClient::instance().send(request);
    if (httpRequestId.isEmpty()) {
        QTimer::singleShot(0, this, [this, requestId, query]() {
            const QVector<AiChatListEntry> entries = AiChatRepository::instance().requestAiChatList(query);
            emit conversationsLoaded(requestId, query, entries);
        });
        return requestId;
    }

    m_conversationListRequestIds.insert(httpRequestId, requestId);
    m_conversationListQueries.insert(httpRequestId, query);
    return requestId;
}

void AiChatSessionController::onConversationListRequestSucceeded(const QString& httpRequestId,
                                                                 const NetworkResponse& response)
{
    if (!m_conversationListRequestIds.contains(httpRequestId)) {
        return;
    }

    const int requestId = m_conversationListRequestIds.take(httpRequestId);
    const AiChatListRequest query = m_conversationListQueries.take(httpRequestId);
    const QJsonArray conversationItems = arrayFromResponse(response, QStringLiteral("conversations"));

    QVector<AiChatListEntry> entries;
    entries.reserve(conversationItems.size());
    for (const QJsonValue& value : conversationItems) {
        const QJsonObject object = conversationObjectFromResponse(value.toObject());
        if (object.isEmpty()) {
            continue;
        }
        const AiChatListEntry entry = conversationEntryFromObject(object);
        if (!entry.conversationId.isEmpty()) {
            entries.push_back(entry);
        }
    }

    AiChatRepository::instance().setAiChatListPage(query, entries);
    emit conversationsLoaded(requestId, query, entries);
}

void AiChatSessionController::onConversationListRequestFailed(const QString& httpRequestId,
                                                              const NetworkError& error)
{
    Q_UNUSED(error)
    if (!m_conversationListRequestIds.contains(httpRequestId)) {
        return;
    }

    const int requestId = m_conversationListRequestIds.take(httpRequestId);
    const AiChatListRequest query = m_conversationListQueries.take(httpRequestId);
    QTimer::singleShot(0, this, [this, requestId, query]() {
        const QVector<AiChatListEntry> entries = AiChatRepository::instance().requestAiChatList(query);
        emit conversationsLoaded(requestId, query, entries);
    });
}

int AiChatSessionController::loadMessagesAsync(const QString& conversationId)
{
    const int requestId = m_nextAsyncRequestId++;

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Get,
            QStringLiteral("/ai/conversations/%1/messages").arg(conversationId));
    request.maxRetries = 3;

    const QString httpRequestId = HttpClient::instance().send(request);
    if (httpRequestId.isEmpty()) {
        QTimer::singleShot(0, this, [this, requestId, conversationId]() {
            const QVector<AiChatMessage> messages = AiChatRepository::instance().requestAiChatMessages(conversationId);
            emit messagesLoaded(requestId, conversationId, messages);
        });
        return requestId;
    }

    m_messageHistoryRequestIds.insert(httpRequestId, requestId);
    m_messageHistoryConversationIds.insert(httpRequestId, conversationId);
    return requestId;
}

void AiChatSessionController::onMessageHistoryRequestSucceeded(const QString& httpRequestId,
                                                               const NetworkResponse& response)
{
    if (!m_messageHistoryRequestIds.contains(httpRequestId)) {
        return;
    }

    const int requestId = m_messageHistoryRequestIds.take(httpRequestId);
    const QString conversationId = m_messageHistoryConversationIds.take(httpRequestId);
    const QJsonArray messageItems = arrayFromResponse(response, QStringLiteral("messages"));

    QVector<AiChatMessage> messages;
    messages.reserve(messageItems.size() + 1);
    for (int index = 0; index < messageItems.size(); ++index) {
        const QJsonObject object = messageItems.at(index).toObject();
        if (object.isEmpty()) {
            continue;
        }
        messages.push_back(aiMessageFromObject(object, conversationId, index));
    }

    if (conversationId == m_streamConversationId && !m_streamMessageId.isEmpty()) {
        const QVector<AiChatMessage> localMessages = AiChatRepository::instance().requestAiChatMessages(conversationId);
        const auto streamIt = std::find_if(localMessages.cbegin(), localMessages.cend(), [this](const AiChatMessage& message) {
            return message.messageId == m_streamMessageId;
        });
        const bool alreadyIncluded = std::any_of(messages.cbegin(), messages.cend(), [this](const AiChatMessage& message) {
            return message.messageId == m_streamMessageId;
        });
        if (streamIt != localMessages.cend() && !alreadyIncluded) {
            messages.push_back(*streamIt);
        }
    }

    AiChatRepository::instance().setAiChatMessages(conversationId, messages);
    emit messagesLoaded(requestId, conversationId, messages);
}

void AiChatSessionController::onMessageHistoryRequestFailed(const QString& httpRequestId,
                                                            const NetworkError& error)
{
    Q_UNUSED(error)
    if (!m_messageHistoryRequestIds.contains(httpRequestId)) {
        return;
    }

    const int requestId = m_messageHistoryRequestIds.take(httpRequestId);
    const QString conversationId = m_messageHistoryConversationIds.take(httpRequestId);
    QTimer::singleShot(0, this, [this, requestId, conversationId]() {
        const QVector<AiChatMessage> messages = AiChatRepository::instance().requestAiChatMessages(conversationId);
        emit messagesLoaded(requestId, conversationId, messages);
    });
}

int AiChatSessionController::loadContextUsageAsync(const AiChatContextUsageRequest& request)
{
    const int requestId = m_nextAsyncRequestId++;
    QTimer::singleShot(0, this, [this, requestId, request]() {
        const AiChatContextUsage usage = AiChatRepository::instance().requestAiChatContextUsage(request);
        emit contextUsageLoaded(requestId, request, usage);
    });
    return requestId;
}

AiChatListEntry AiChatSessionController::createConversationFromFirstMessage(const QString& firstUserMessage)
{
    const QString title = temporaryTitleFromUserMessage(firstUserMessage);
    AiChatListEntry entry = createRemoteAiConversation(title);
    if (entry.conversationId.isEmpty()) {
        return {};
    }

    const QString conversationId = AiChatRepository::instance().createAiChatConversation(entry.conversationId,
                                                                                        entry.title,
                                                                                        entry.time);
    if (conversationId.isEmpty()) {
        return {};
    }

    if (!conversationId.isEmpty()) {
        emit conversationsChanged();
    }
    return entry;
}

AiChatMessage AiChatSessionController::submitUserMessage(const QString& conversationId, const QString& text)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return {};
    }

    QString resolvedConversationId = conversationId;
    if (!isUuid(resolvedConversationId)) {
        const QString title = temporaryTitleFromUserMessage(text);
        const AiChatListEntry remoteEntry = createRemoteAiConversation(title);
        if (remoteEntry.conversationId.isEmpty()) {
            return {};
        }
        if (!AiChatRepository::instance().replaceAiChatConversationId(conversationId,
                                                                      remoteEntry.conversationId)) {
            return {};
        }

        resolvedConversationId = remoteEntry.conversationId;
        emit conversationIdChanged(conversationId, remoteEntry);
        emit conversationsChanged();
    }

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            resolvedConversationId,
            text,
            true);
    if (message.messageId.isEmpty()) {
        return {};
    }

    startAiReplyStream(resolvedConversationId, text);
    return message;
}

bool AiChatSessionController::regenerateAiReply(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return false;
    }

    if (hasActiveAiReplyStream() &&
            (conversationId != m_streamConversationId || messageId != m_streamMessageId)) {
        return false;
    }

    const QVector<AiChatMessage> messages = AiChatRepository::instance().requestAiChatMessages(conversationId);
    int replyRow = -1;
    for (int row = 0; row < messages.size(); ++row) {
        if (messages.at(row).messageId == messageId) {
            replyRow = row;
            break;
        }
    }

    if (replyRow < 0 ||
            replyRow != messages.size() - 1 ||
            messages.at(replyRow).isFromUser) {
        return false;
    }

    QString prompt;
    for (int row = replyRow - 1; row >= 0; --row) {
        if (messages.at(row).isFromUser) {
            prompt = messages.at(row).text;
            break;
        }
    }
    if (prompt.trimmed().isEmpty()) {
        return false;
    }

    QString resolvedConversationId = conversationId;
    if (!isUuid(resolvedConversationId)) {
        const QString title = temporaryTitleFromUserMessage(prompt);
        const AiChatListEntry remoteEntry = createRemoteAiConversation(title);
        if (remoteEntry.conversationId.isEmpty()) {
            return false;
        }
        if (!AiChatRepository::instance().replaceAiChatConversationId(conversationId,
                                                                      remoteEntry.conversationId)) {
            return false;
        }

        resolvedConversationId = remoteEntry.conversationId;
        emit conversationIdChanged(conversationId, remoteEntry);
        emit conversationsChanged();
    }

    if (resolvedConversationId == m_streamConversationId && messageId == m_streamMessageId) {
        if (m_streamClient) {
            m_streamClient->abort();
        }
        resetActiveAiReplyStream();
    }

    if (!AiChatRepository::instance().removeAiChatMessage(resolvedConversationId, messageId)) {
        return false;
    }

    emit aiReplyMessageRemoved(resolvedConversationId, messageId);
    emit conversationsChanged();
    startAiReplyStream(resolvedConversationId, prompt);
    return true;
}

bool AiChatSessionController::renameConversation(const QString& conversationId, const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return false;
    }

    QString appliedTitle = trimmedTitle;
    if (isUuid(conversationId)) {
        appliedTitle = patchRemoteAiConversationTitle(conversationId, trimmedTitle);
        if (appliedTitle.isEmpty()) {
            return false;
        }
    }

    const bool renamed = AiChatRepository::instance().renameAiChatConversation(conversationId, appliedTitle);
    if (renamed) {
        emit conversationTitleChanged(conversationId, appliedTitle);
        emit conversationsChanged();
    }
    return renamed;
}

bool AiChatSessionController::deleteConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    if (conversationId == m_streamConversationId) {
        const QString messageId = m_streamMessageId;
        if (m_streamClient) {
            m_streamClient->abort();
        }
        resetActiveAiReplyStream();
        emit aiReplyCanceled(conversationId, messageId);
    }

    if (isUuid(conversationId) && !deleteRemoteAiConversation(conversationId)) {
        return false;
    }

    discardMessageHistoryRequestsForConversation(conversationId);
    const bool removed = AiChatRepository::instance().removeAiChatConversation(conversationId);
    if (removed) {
        emit conversationDeleted(conversationId);
        emit conversationsChanged();
    }
    return removed;
}

bool AiChatSessionController::clearConversationUnreadDot(const QString& conversationId)
{
    return AiChatRepository::instance().setConversationUnreadDot(conversationId, false);
}

int AiChatSessionController::unreadConversationDotCount() const
{
    return AiChatRepository::instance().unreadDotCount();
}

bool AiChatSessionController::hasActiveAiReplyStream() const
{
    return !m_streamConversationId.isEmpty() || (m_streamClient && m_streamClient->isRunning());
}

QString AiChatSessionController::activeStreamConversationId() const
{
    return m_streamConversationId;
}

QString AiChatSessionController::activeStreamMessageId() const
{
    return m_streamMessageId;
}

void AiChatSessionController::cancelActiveAiReplyStream()
{
    if (m_streamClient) {
        m_streamClient->cancel();
    }
}

void AiChatSessionController::onAiReplyChunkReceived(const QString& chunk)
{
    if (chunk.isEmpty() || m_streamConversationId.isEmpty()) {
        return;
    }

    m_streamVisibleText += chunk;
    if (m_streamMessageId.isEmpty()) {
        const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
                m_streamConversationId,
                m_streamVisibleText,
                false);
        if (message.messageId.isEmpty()) {
            cancelActiveAiReplyStream();
            return;
        }

        m_streamMessageId = message.messageId;
        emit aiReplyMessageAdded(message);
        return;
    }

    AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                         m_streamMessageId,
                                                         m_streamVisibleText);
    emit aiReplyMessageUpdated(m_streamConversationId, m_streamMessageId, m_streamVisibleText);
}

void AiChatSessionController::onGeneratedTitleReceived(const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (m_streamConversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return;
    }

    if (AiChatRepository::instance().renameAiChatConversation(m_streamConversationId, trimmedTitle)) {
        emit conversationTitleChanged(m_streamConversationId, trimmedTitle);
        emit conversationsChanged();
    }
}

void AiChatSessionController::onAssistantMessageReceived(const QString& messageId,
                                                         const QString& text,
                                                         const QDateTime& time)
{
    if (m_streamConversationId.isEmpty()) {
        return;
    }

    const QString resolvedText = text.isEmpty() ? m_streamVisibleText : text;
    if (resolvedText.isEmpty()) {
        return;
    }

    if (m_streamMessageId.isEmpty()) {
        const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
                m_streamConversationId,
                resolvedText,
                false,
                time.isValid() ? time : QDateTime::currentDateTime());
        if (message.messageId.isEmpty()) {
            cancelActiveAiReplyStream();
            return;
        }

        m_streamMessageId = message.messageId;
        emit aiReplyMessageAdded(message);
    }

    AiChatMessage replacement;
    replacement.messageId = messageId.isEmpty() ? m_streamMessageId : messageId;
    replacement.conversationId = m_streamConversationId;
    replacement.text = resolvedText;
    replacement.isFromUser = false;
    replacement.time = time.isValid() ? time : QDateTime::currentDateTime();

    const QString previousMessageId = m_streamMessageId;
    if (AiChatRepository::instance().replaceAiChatMessage(m_streamConversationId,
                                                          previousMessageId,
                                                          replacement)) {
        m_streamMessageId = replacement.messageId;
        m_streamVisibleText = replacement.text;
        emit aiReplyMessageReplaced(m_streamConversationId, previousMessageId, replacement);
    }
}

void AiChatSessionController::onAiReplyCanceled()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        emit conversationsChanged();
        emit aiReplyCanceled(conversationId, messageId);
    }
}

void AiChatSessionController::onAiReplyFailed(const NetworkError& error)
{
    m_streamFailed = true;
    const QString message = error.message.isEmpty()
            ? QStringLiteral("AI 回复失败，请稍后重试")
            : error.message;
    emit aiReplyFailed(m_streamConversationId, m_streamMessageId, message);
}

void AiChatSessionController::onAiReplyFinished()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    const bool failed = m_streamFailed;
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        if (!failed && !messageId.isEmpty()) {
            AiChatRepository::instance().setConversationUnreadDot(conversationId, true);
        }
        emit conversationsChanged();
        emit aiReplyFinished(conversationId, messageId);
    }
}

void AiChatSessionController::startAiReplyStream(const QString& conversationId, const QString& prompt)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return;
    }

    m_streamConversationId = conversationId;
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    m_streamFailed = false;
    emit aiReplyStarted(conversationId);

    m_streamClient->start(conversationId, prompt);
    if (!m_streamClient->isRunning()) {
        cancelActiveAiReplyStream();
    }
}

void AiChatSessionController::resetActiveAiReplyStream()
{
    m_streamConversationId.clear();
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    m_streamFailed = false;
}

void AiChatSessionController::discardMessageHistoryRequestsForConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    const QList<QString> requestIds = m_messageHistoryConversationIds.keys(conversationId);
    for (const QString& requestId : requestIds) {
        m_messageHistoryConversationIds.remove(requestId);
        m_messageHistoryRequestIds.remove(requestId);
    }
}
