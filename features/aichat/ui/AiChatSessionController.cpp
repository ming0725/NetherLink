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
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {

constexpr int kCreateConversationTimeoutMs = 20000;
constexpr int kConversationMutationTimeoutMs = 15000;
constexpr int kTemporaryTitleMaxLength = 80;
constexpr int kStreamCharacterIntervalMs = 24;
constexpr int kProgressCharacterIntervalMs = 24;

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

QStringList graphemeClusters(const QString& text)
{
    QStringList clusters;
    if (text.isEmpty()) {
        return clusters;
    }

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.toStart();
    int start = 0;
    while (true) {
        const int end = finder.toNextBoundary();
        if (end < 0) {
            break;
        }
        if (end > start) {
            clusters.push_back(text.mid(start, end - start));
        }
        start = end;
    }
    if (start < text.size()) {
        clusters.push_back(text.mid(start));
    }
    return clusters;
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

QVector<QString> stringsFromArray(const QJsonValue& value)
{
    QVector<QString> strings;
    for (const QJsonValue& item : value.toArray()) {
        QString text;
        if (item.isString()) {
            text = item.toString();
        } else if (item.isObject()) {
            const QJsonObject object = item.toObject();
            text = object.value(QStringLiteral("url")).toString(
                    object.value(QStringLiteral("text")).toString());
        }
        if (!text.isEmpty() && !strings.contains(text)) {
            strings.push_back(text);
        }
    }
    return strings;
}

AiChatTrace aiTraceFromObject(const QJsonObject& object)
{
    AiChatTrace trace;
    if (object.isEmpty()) {
        return trace;
    }

    trace.traceId = object.value(QStringLiteral("traceId")).toString();
    trace.status = object.value(QStringLiteral("status")).toString();
    trace.summary = object.value(QStringLiteral("summary")).toString();
    trace.sourceRefs = stringsFromArray(object.value(QStringLiteral("sourceRefs")));
    trace.createdAt = dateTimeFromString(object.value(QStringLiteral("createdAt")).toString());
    trace.updatedAt = dateTimeFromString(object.value(QStringLiteral("updatedAt")).toString());
    for (const QJsonValue& value : object.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject stepObject = value.toObject();
        AiChatTraceStep step;
        step.sequence = stepObject.value(QStringLiteral("sequence")).toInt();
        step.stepId = stepObject.value(QStringLiteral("stepId")).toString();
        step.phase = stepObject.value(QStringLiteral("phase")).toString();
        step.status = stepObject.value(QStringLiteral("status")).toString();
        step.text = stepObject.value(QStringLiteral("text")).toString();
        step.tool = stepObject.value(QStringLiteral("tool")).toString();
        step.query = stepObject.value(QStringLiteral("query")).toString();
        step.urls = stringsFromArray(stepObject.value(QStringLiteral("urls")));
        trace.steps.push_back(step);
    }
    return trace;
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
    message.trace = aiTraceFromObject(object.value(QStringLiteral("trace")).toObject());
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
    connect(m_streamClient, &AiChatStreamClient::streamStarted,
            this, &AiChatSessionController::onAiReplyStreamStarted);
    connect(m_streamClient, &AiChatStreamClient::chunkReceived,
            this, &AiChatSessionController::onAiReplyChunkReceived);
    connect(m_streamClient, &AiChatStreamClient::thinkingStateChanged,
            this, &AiChatSessionController::onAiReplyThinkingChanged);
    connect(m_streamClient, &AiChatStreamClient::streamStatusChanged,
            this, &AiChatSessionController::onAiReplyStreamStatusChanged);
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
            });

    m_streamCharacterTimer.setInterval(kStreamCharacterIntervalMs);
    connect(&m_streamCharacterTimer,
            &QTimer::timeout,
            this,
            &AiChatSessionController::revealNextStreamCharacter);
    m_streamProgressCharacterTimer.setInterval(kProgressCharacterIntervalMs);
    connect(&m_streamProgressCharacterTimer,
            &QTimer::timeout,
            this,
            &AiChatSessionController::revealNextProgressCharacter);
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
        emit conversationEntryChanged(entry);
    }
    return entry;
}

AiChatMessage AiChatSessionController::submitUserMessage(const QString& conversationId,
                                                         const QString& text,
                                                         const AiChatRequestOptions& options)
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
        emit conversationEntryChanged(remoteEntry);
    }

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            resolvedConversationId,
            text,
            true);
    if (message.messageId.isEmpty()) {
        return {};
    }

    emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(resolvedConversationId));
    startAiReplyStream(resolvedConversationId, text, options);
    return message;
}

bool AiChatSessionController::regenerateAiReply(const QString& conversationId,
                                                const QString& messageId,
                                                const AiChatRequestOptions& options)
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
        emit conversationEntryChanged(remoteEntry);
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
    emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(resolvedConversationId));
    startAiReplyStream(resolvedConversationId, prompt, options);
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
        emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(conversationId));
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
    const bool changed = AiChatRepository::instance().setConversationUnreadDot(conversationId, false);
    if (changed) {
        emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(conversationId));
    }
    return changed;
}

int AiChatSessionController::unreadConversationDotCount() const
{
    return AiChatRepository::instance().unreadDotCount();
}

void AiChatSessionController::setCurrentConversationId(const QString& conversationId)
{
    m_currentConversationId = conversationId;
    if (!m_currentConversationId.isEmpty()) {
        clearConversationUnreadDot(m_currentConversationId);
    }
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

void AiChatSessionController::onAiReplyStreamStarted(const QString& streamId,
                                                     const QString& conversationId,
                                                     const QString& clientMessageId)
{
    Q_UNUSED(clientMessageId)
    if (!conversationId.isEmpty() && conversationId != m_streamConversationId) {
        return;
    }
    if (!streamId.isEmpty() && streamId == m_streamId) {
        return;
    }

    m_streamId = streamId;
    clearStreamCharacterQueue();
    clearProgressCharacterQueue();
    m_streamVisibleText.clear();
    m_streamProgressSegments.clear();
    m_legacyToolStepIds.clear();
    m_latestToolStepId.clear();
    m_nextLegacyToolStepId = 1;
    m_streamAnswerStarted = false;
}

void AiChatSessionController::onAiReplyChunkReceived(const AiChatStreamChunk& chunk)
{
    if (m_streamConversationId.isEmpty()) {
        return;
    }

    if (chunk.kind == QStringLiteral("progress") || chunk.transient) {
        QString stepId = chunk.stepId;
        if (stepId.isEmpty()) {
            stepId = m_latestToolStepId;
        }
        if (stepId.isEmpty()) {
            stepId = QStringLiteral("progress-step-%1").arg(m_nextLegacyToolStepId++);
            m_latestToolStepId = stepId;
        }

        QString segmentId = chunk.segmentId;
        if (segmentId.isEmpty()) {
            segmentId = QStringLiteral("%1-progress").arg(stepId);
        }

        AiChatProgressSegment& progress = m_streamProgressSegments[segmentId];
        if (chunk.segmentStart || progress.segmentId.isEmpty()) {
            progress = {};
            progress.stepId = stepId;
            progress.segmentId = segmentId;
            emit aiReplyProgressChanged(m_streamConversationId, progress);
        }
        progress.stepId = stepId;
        const QStringList clusters = graphemeClusters(chunk.delta);
        for (const QString& cluster : clusters) {
            m_streamProgressCharacterQueue.enqueue(
                    {stepId, segmentId, cluster, false});
        }
        if (chunk.segmentEnd) {
            m_streamProgressCharacterQueue.enqueue(
                    {stepId, segmentId, {}, true});
        }
        if (!m_streamProgressCharacterQueue.isEmpty() &&
                !m_streamProgressCharacterTimer.isActive()) {
            m_streamProgressCharacterTimer.start();
        }
        return;
    }

    if (chunk.delta.isEmpty()) {
        return;
    }

    if (!m_streamAnswerStarted) {
        m_streamAnswerStarted = true;
        emit aiReplyThinkingChanged(m_streamConversationId, false);
    }
    const QStringList clusters = graphemeClusters(chunk.delta);
    for (const QString& cluster : clusters) {
        m_streamCharacterQueue.enqueue({cluster});
    }
    if (m_streamProgressCharacterQueue.isEmpty() &&
            !m_streamProgressCharacterTimer.isActive() &&
            !m_streamCharacterQueue.isEmpty() &&
            !m_streamCharacterTimer.isActive()) {
        m_streamCharacterTimer.start();
    }
}

void AiChatSessionController::revealNextStreamCharacter()
{
    if (m_streamConversationId.isEmpty() ||
            !m_streamProgressCharacterQueue.isEmpty() ||
            m_streamProgressCharacterTimer.isActive() ||
            m_streamCharacterQueue.isEmpty()) {
        m_streamCharacterTimer.stop();
        return;
    }

    const QueuedStreamText queued = m_streamCharacterQueue.dequeue();
    appendVisibleStreamText(queued.text);
    if (m_streamCharacterQueue.isEmpty()) {
        m_streamCharacterTimer.stop();
    }
}

void AiChatSessionController::revealNextProgressCharacter()
{
    if (m_streamConversationId.isEmpty() ||
            m_streamProgressCharacterQueue.isEmpty()) {
        m_streamProgressCharacterTimer.stop();
        if (!m_streamCharacterQueue.isEmpty() &&
                !m_streamCharacterTimer.isActive()) {
            m_streamCharacterTimer.start();
        }
        return;
    }

    const QueuedProgressText queued = m_streamProgressCharacterQueue.dequeue();
    AiChatProgressSegment& progress =
            m_streamProgressSegments[queued.segmentId];
    if (progress.segmentId.isEmpty()) {
        progress.stepId = queued.stepId;
        progress.segmentId = queued.segmentId;
    }
    if (queued.segmentEnd) {
        progress.complete = true;
    } else {
        progress.text += queued.text;
    }
    emit aiReplyProgressChanged(m_streamConversationId, progress);

    if (m_streamProgressCharacterQueue.isEmpty()) {
        m_streamProgressCharacterTimer.stop();
        if (!m_streamCharacterQueue.isEmpty() &&
                !m_streamCharacterTimer.isActive()) {
            m_streamCharacterTimer.start();
        }
    }
}

void AiChatSessionController::appendVisibleStreamText(const QString& text)
{
    if (text.isEmpty() || m_streamConversationId.isEmpty()) {
        return;
    }

    m_streamVisibleText += text;
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
        emit aiReplyMessageAdded(message, false);
        return;
    }

    AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                         m_streamMessageId,
                                                         m_streamVisibleText);
    emit aiReplyMessageUpdated(m_streamConversationId,
                               m_streamMessageId,
                               m_streamVisibleText,
                               false);
}

void AiChatSessionController::clearStreamCharacterQueue()
{
    m_streamCharacterTimer.stop();
    m_streamCharacterQueue.clear();
}

void AiChatSessionController::clearProgressCharacterQueue()
{
    m_streamProgressCharacterTimer.stop();
    m_streamProgressCharacterQueue.clear();
}

void AiChatSessionController::finishProgressAnimations()
{
    m_streamProgressCharacterTimer.stop();
    QSet<QString> changedSegments;
    while (!m_streamProgressCharacterQueue.isEmpty()) {
        const QueuedProgressText queued =
                m_streamProgressCharacterQueue.dequeue();
        AiChatProgressSegment& progress =
                m_streamProgressSegments[queued.segmentId];
        if (progress.segmentId.isEmpty()) {
            progress.stepId = queued.stepId;
            progress.segmentId = queued.segmentId;
        }
        if (queued.segmentEnd) {
            progress.complete = true;
        } else {
            progress.text += queued.text;
        }
        changedSegments.insert(queued.segmentId);
    }
    for (const QString& segmentId : changedSegments) {
        emit aiReplyProgressChanged(m_streamConversationId,
                                    m_streamProgressSegments.value(segmentId));
    }
}

void AiChatSessionController::onAiReplyThinkingChanged(bool active)
{
    if (!m_streamConversationId.isEmpty()) {
        emit aiReplyThinkingChanged(m_streamConversationId, active);
    }
}

void AiChatSessionController::onAiReplyStreamStatusChanged(const AiChatStreamStatus& status)
{
    if (!m_streamConversationId.isEmpty()) {
        AiChatStreamStatus resolvedStatus = status;
        const QString phase = status.phase.trimmed().toLower();
        if (phase == QStringLiteral("searching") ||
                phase == QStringLiteral("reading") ||
                phase == QStringLiteral("tool")) {
            resolvedStatus.stepId = resolvedToolStepId(status);
            m_latestToolStepId = resolvedStatus.stepId;
        }
        emit aiReplyStreamStatusChanged(m_streamConversationId, resolvedStatus);
    }
}

QString AiChatSessionController::resolvedToolStepId(const AiChatStreamStatus& status)
{
    if (!status.stepId.isEmpty()) {
        return status.stepId;
    }

    const QString signature = QStringLiteral("%1\x1f%2\x1f%3")
            .arg(status.phase.trimmed().toLower(),
                 status.tool.trimmed().toLower(),
                 status.query.trimmed());
    auto it = m_legacyToolStepIds.find(signature);
    if (it != m_legacyToolStepIds.end()) {
        return it.value();
    }

    const QString stepId = QStringLiteral("legacy-step-%1").arg(m_nextLegacyToolStepId++);
    m_legacyToolStepIds.insert(signature, stepId);
    return stepId;
}

void AiChatSessionController::onGeneratedTitleReceived(const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (m_streamConversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return;
    }

    if (AiChatRepository::instance().renameAiChatConversation(m_streamConversationId, trimmedTitle)) {
        emit conversationTitleChanged(m_streamConversationId, trimmedTitle);
        emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(m_streamConversationId));
    }
}

void AiChatSessionController::onAssistantMessageReceived(const AiChatMessage& serverMessage)
{
    if (m_streamConversationId.isEmpty()) {
        return;
    }

    clearStreamCharacterQueue();
    finishProgressAnimations();
    const QString resolvedText = serverMessage.text.isEmpty()
            ? m_streamVisibleText
            : serverMessage.text;
    if (resolvedText.isEmpty()) {
        return;
    }

    if (m_streamMessageId.isEmpty()) {
        const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
                m_streamConversationId,
                resolvedText,
                false,
                serverMessage.time.isValid()
                        ? serverMessage.time
                        : QDateTime::currentDateTime());
        if (message.messageId.isEmpty()) {
            cancelActiveAiReplyStream();
            return;
        }

        m_streamMessageId = message.messageId;
        emit aiReplyMessageAdded(message, false);
    }

    AiChatMessage replacement = serverMessage;
    replacement.messageId = serverMessage.messageId.isEmpty()
            ? m_streamMessageId
            : serverMessage.messageId;
    replacement.conversationId = m_streamConversationId;
    replacement.text = resolvedText;
    replacement.isFromUser = false;
    replacement.time = serverMessage.time.isValid()
            ? serverMessage.time
            : QDateTime::currentDateTime();

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
        emit aiReplyThinkingChanged(conversationId, false);
        emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(conversationId));
        emit aiReplyCanceled(conversationId, messageId);
    }
}

void AiChatSessionController::onAiReplyFailed(const NetworkError& error)
{
    clearStreamCharacterQueue();
    clearProgressCharacterQueue();
    m_streamFailed = true;
    const QString message = error.message.isEmpty()
            ? QStringLiteral("AI 回复失败，请稍后重试")
            : error.message;
    emit aiReplyThinkingChanged(m_streamConversationId, false);
    emit aiReplyFailed(m_streamConversationId, m_streamMessageId, message);
}

void AiChatSessionController::onAiReplyFinished()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    const bool failed = m_streamFailed;
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        emit aiReplyThinkingChanged(conversationId, false);
        if (!failed && !messageId.isEmpty() && conversationId != m_currentConversationId) {
            AiChatRepository::instance().setConversationUnreadDot(conversationId, true);
        }
        emit conversationEntryChanged(AiChatRepository::instance().requestAiChatConversation(conversationId));
        emit aiReplyFinished(conversationId, messageId);
    }
}

void AiChatSessionController::startAiReplyStream(const QString& conversationId,
                                                 const QString& prompt,
                                                 const AiChatRequestOptions& options)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return;
    }

    m_streamConversationId = conversationId;
    m_streamId.clear();
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    clearStreamCharacterQueue();
    clearProgressCharacterQueue();
    m_streamProgressSegments.clear();
    m_legacyToolStepIds.clear();
    m_latestToolStepId.clear();
    m_nextLegacyToolStepId = 1;
    m_streamAnswerStarted = false;
    m_streamFailed = false;
    emit aiReplyStarted(conversationId);

    m_streamClient->start(conversationId, prompt, {}, options);
    if (!m_streamClient->isRunning()) {
        cancelActiveAiReplyStream();
    }
}

void AiChatSessionController::resetActiveAiReplyStream()
{
    clearStreamCharacterQueue();
    clearProgressCharacterQueue();
    m_streamConversationId.clear();
    m_streamId.clear();
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    m_streamProgressSegments.clear();
    m_legacyToolStepIds.clear();
    m_latestToolStepId.clear();
    m_nextLegacyToolStepId = 1;
    m_streamAnswerStarted = false;
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
