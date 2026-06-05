#include "AiChatStreamClient.h"

#include "shared/network/AppEventBus.h"
#include "shared/network/HttpClient.h"
#include "shared/network/RealtimeClient.h"
#include "shared/network/SseClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

namespace {

QString normalizedEventName(const QString& eventName, const QJsonObject& data)
{
    if (!eventName.isEmpty()) {
        return eventName;
    }
    return data.value(QStringLiteral("type")).toString();
}

QDateTime dateTimeFromString(const QString& value)
{
    QDateTime time = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(value, Qt::ISODate);
    }
    return time.isValid() ? time : QDateTime::currentDateTime();
}

NetworkError errorFromObject(const QJsonObject& object)
{
    NetworkError error;
    error.httpStatus = object.value(QStringLiteral("httpStatus")).toInt();
    error.code = object.value(QStringLiteral("code")).toString(QStringLiteral("AI_STREAM_ERROR"));
    error.message = object.value(QStringLiteral("message")).toString(QStringLiteral("AI stream failed."));
    error.requestId = object.value(QStringLiteral("requestId")).toString();
    error.details = object.value(QStringLiteral("details")).toObject();
    return error;
}

QString clientMessageIdFromPayload(const QJsonObject& payload)
{
    QString clientMessageId = payload.value(QStringLiteral("clientMessageId")).toString();
    if (!clientMessageId.isEmpty()) {
        return clientMessageId;
    }

    const QJsonObject userMessage = payload.value(QStringLiteral("userMessage")).toObject();
    clientMessageId = userMessage.value(QStringLiteral("clientMessageId")).toString();
    if (!clientMessageId.isEmpty()) {
        return clientMessageId;
    }

    const QJsonObject message = payload.value(QStringLiteral("message")).toObject();
    clientMessageId = message.value(QStringLiteral("clientMessageId")).toString();
    if (!clientMessageId.isEmpty()) {
        return clientMessageId;
    }

    return payload.value(QStringLiteral("userClientMessageId")).toString();
}

} // namespace

AiChatStreamClient::AiChatStreamClient(QObject* parent)
    : QObject(parent)
    , m_sseClient(new SseClient(this))
{
    connect(m_sseClient,
            &SseClient::eventReceived,
            this,
            &AiChatStreamClient::handleStreamEvent);
    connect(m_sseClient,
            &SseClient::streamFinished,
            this,
            &AiChatStreamClient::handleStreamFinished);
    connect(m_sseClient,
            &SseClient::streamFailed,
            this,
            &AiChatStreamClient::handleStreamFailed);
    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                handleRealtimeEvent(type, payload);
            });

    m_cancelTimeoutTimer.setSingleShot(true);
    m_cancelTimeoutTimer.setInterval(3000);
    connect(&m_cancelTimeoutTimer,
            &QTimer::timeout,
            this,
            &AiChatStreamClient::handleCancelTimeout);
}

bool AiChatStreamClient::isRunning() const
{
    return m_running || (m_sseClient && m_sseClient->isRunning());
}

void AiChatStreamClient::start(const QString& conversationId,
                               const QString& prompt,
                               const QString& clientMessageId)
{
    abort();

    const QString trimmedPrompt = prompt.trimmed();
    if (conversationId.isEmpty() || trimmedPrompt.isEmpty() || !m_sseClient) {
        return;
    }

    m_clientMessageId = clientMessageId.isEmpty() ? newClientMessageId() : clientMessageId;
    QJsonObject body{
            {QStringLiteral("message"), trimmedPrompt},
            {QStringLiteral("clientMessageId"), m_clientMessageId},
            {QStringLiteral("aiFileIds"), QJsonArray{}}
    };

    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/ai/conversations/%1/messages").arg(conversationId),
                                                  body,
                                                  {{QStringLiteral("stream"), true}});
    request.headers.insert("Accept", "text/event-stream");
    request.maxRetries = 0;

    m_sseClient->setEnvironment(HttpClient::instance().environment());
    m_running = true;
    m_requestId = m_sseClient->start(request);
}

void AiChatStreamClient::cancel()
{
    if (!isRunning()) {
        clearActiveRequest();
        return;
    }

    sendCancelCommand();
    m_waitingForTerminalEvent = true;
    m_cancelTimeoutTimer.start();

    if (m_streamId.isEmpty() && m_sseClient) {
        m_sseClient->cancel();
        m_requestId.clear();
    }
}

void AiChatStreamClient::abort()
{
    if (m_sseClient) {
        m_sseClient->cancel();
    }
    clearActiveRequest();
}

QString AiChatStreamClient::newClientMessageId() const
{
    return QStringLiteral("ai_msg_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void AiChatStreamClient::handleStreamEvent(const QString& requestId,
                                           const QString& eventName,
                                           const QJsonObject& data)
{
    if (!matchesActiveRequest(requestId)) {
        return;
    }

    const QString type = normalizedEventName(eventName, data);
    if (type == QStringLiteral("ai.stream.started")) {
        m_streamId = data.value(QStringLiteral("streamId")).toString(m_streamId);
        return;
    }

    if (type == QStringLiteral("ai.stream.chunk")) {
        if (m_waitingForTerminalEvent) {
            return;
        }
        const QString delta = data.value(QStringLiteral("delta")).toString(
                data.value(QStringLiteral("content")).toString());
        if (!delta.isEmpty()) {
            emit chunkReceived(delta);
        }
        return;
    }

    if (type == QStringLiteral("ai.stream.done")) {
        processTerminalEvent(data, false);
        return;
    }

    if (type == QStringLiteral("ai.stream.cancelled")) {
        processTerminalEvent(data, true);
        return;
    }

    if (type == QStringLiteral("client.error")) {
        const NetworkError error = errorFromObject(data);
        clearActiveRequest();
        emit failed(error);
        emit finished();
    }
}

void AiChatStreamClient::handleRealtimeEvent(const QString& type, const QJsonObject& payload)
{
    if (type != QStringLiteral("ai.stream.done") &&
            type != QStringLiteral("ai.stream.cancelled")) {
        return;
    }

    if (!matchesActiveRealtimeEvent(payload)) {
        return;
    }

    processTerminalEvent(payload, type == QStringLiteral("ai.stream.cancelled"));
}

void AiChatStreamClient::handleStreamFinished(const QString& requestId)
{
    if (!matchesActiveRequest(requestId)) {
        return;
    }

    clearActiveRequest();
    emit finished();
}

void AiChatStreamClient::handleStreamFailed(const QString& requestId, const NetworkError& error)
{
    if (!matchesActiveRequest(requestId)) {
        return;
    }

    clearActiveRequest();
    emit failed(error);
    emit finished();
}

void AiChatStreamClient::handleCancelTimeout()
{
    if (!m_running || !m_waitingForTerminalEvent) {
        return;
    }

    if (m_sseClient) {
        m_sseClient->cancel();
    }
    clearActiveRequest();
    emit cancelled();
}

void AiChatStreamClient::processTerminalEvent(const QJsonObject& data, bool isCancelled)
{
    const QString title = streamTitle(data);
    if (!title.isEmpty()) {
        emit titleReceived(title);
    }

    const QJsonObject message = assistantMessageObject(data);
    const QString text = assistantMessageText(message);
    const QString messageId = assistantMessageId(message);
    if (!message.isEmpty() && (!text.isEmpty() || !messageId.isEmpty())) {
        emit assistantMessageReceived(messageId,
                                      text,
                                      assistantMessageTime(message));
    }

    if (m_sseClient) {
        m_sseClient->cancel();
    }
    clearActiveRequest();
    if (isCancelled) {
        emit cancelled();
    } else {
        emit finished();
    }
}

void AiChatStreamClient::sendCancelCommand()
{
    if (m_streamId.isEmpty() || !RealtimeClient::instance().isConnected()) {
        return;
    }

    RealtimeClient::instance().sendJson({
            {QStringLiteral("type"), QStringLiteral("ai.stream.cancel")},
            {QStringLiteral("payload"), QJsonObject{{QStringLiteral("streamId"), m_streamId}}}
    });
}

QJsonObject AiChatStreamClient::assistantMessageObject(QJsonObject data)
{
    if (data.value(QStringLiteral("assistantMessage")).isObject()) {
        return data.value(QStringLiteral("assistantMessage")).toObject();
    }
    if (data.value(QStringLiteral("message")).isObject()) {
        return data.value(QStringLiteral("message")).toObject();
    }
    return data;
}

QString AiChatStreamClient::assistantMessageId(const QJsonObject& message)
{
    return message.value(QStringLiteral("messageId")).toString(
            message.value(QStringLiteral("id")).toString(
                    message.value(QStringLiteral("assistantMessageId")).toString()));
}

QString AiChatStreamClient::assistantMessageText(const QJsonObject& message)
{
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QString contentText = content.value(QStringLiteral("text")).toString();
    if (!contentText.isEmpty()) {
        return contentText;
    }
    return message.value(QStringLiteral("text")).toString(
            message.value(QStringLiteral("message")).toString());
}

QDateTime AiChatStreamClient::assistantMessageTime(const QJsonObject& message)
{
    return dateTimeFromString(message.value(QStringLiteral("createdAt")).toString(
            message.value(QStringLiteral("updatedAt")).toString(
                    message.value(QStringLiteral("time")).toString())));
}

QString AiChatStreamClient::streamTitle(const QJsonObject& data)
{
    QString title = data.value(QStringLiteral("title")).toString().trimmed();
    if (!title.isEmpty()) {
        return title;
    }

    const QJsonObject conversation = data.value(QStringLiteral("conversation")).toObject();
    title = conversation.value(QStringLiteral("title")).toString().trimmed();
    if (!title.isEmpty()) {
        return title;
    }

    return data.value(QStringLiteral("conversationTitle")).toString().trimmed();
}

bool AiChatStreamClient::matchesActiveRequest(const QString& requestId) const
{
    return m_running && !m_requestId.isEmpty() && requestId == m_requestId;
}

bool AiChatStreamClient::matchesActiveRealtimeEvent(const QJsonObject& payload) const
{
    if (!m_running) {
        return false;
    }

    const QString streamId = payload.value(QStringLiteral("streamId")).toString();
    if (!m_streamId.isEmpty() && !streamId.isEmpty()) {
        return streamId == m_streamId;
    }

    const QString clientMessageId = clientMessageIdFromPayload(payload);
    return !m_clientMessageId.isEmpty() && clientMessageId == m_clientMessageId;
}

void AiChatStreamClient::clearActiveRequest()
{
    m_cancelTimeoutTimer.stop();
    m_requestId.clear();
    m_streamId.clear();
    m_clientMessageId.clear();
    m_running = false;
    m_waitingForTerminalEvent = false;
}
