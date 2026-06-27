#include "AiChatStreamClient.h"

#include "shared/network/AppEventBus.h"
#include "shared/network/HttpClient.h"
#include "shared/network/RealtimeClient.h"
#include "shared/network/SseClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
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

QString stringFromJsonValue(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        QString text = object.value(QStringLiteral("url")).toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
        text = object.value(QStringLiteral("href")).toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
        return object.value(QStringLiteral("text")).toString().trimmed();
    }

    return {};
}

QVector<QString> stringVectorFromJsonValue(const QJsonValue& value)
{
    QVector<QString> strings;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        strings.reserve(array.size());
        for (const QJsonValue& item : array) {
            const QString text = stringFromJsonValue(item);
            if (!text.isEmpty() && !strings.contains(text)) {
                strings.push_back(text);
            }
        }
        return strings;
    }

    const QString text = stringFromJsonValue(value);
    if (!text.isEmpty()) {
        strings.push_back(text);
    }
    return strings;
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
                               const QString& clientMessageId,
                               const AiChatRequestOptions& options)
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
            {QStringLiteral("aiFileIds"), QJsonArray{}},
            {QStringLiteral("model"), options.model.isEmpty()
                    ? QStringLiteral("deepseek-v4-pro")
                    : options.model},
            {QStringLiteral("thinkingType"), options.thinkingType.isEmpty()
                    ? QStringLiteral("enabled")
                    : options.thinkingType},
            {QStringLiteral("reasoningEffort"), options.reasoningEffort.isEmpty()
                    ? QStringLiteral("high")
                    : options.reasoningEffort}
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
        emit streamStarted(m_streamId,
                           data.value(QStringLiteral("conversationId")).toString(),
                           data.value(QStringLiteral("clientMessageId")).toString(m_clientMessageId));
        return;
    }

    if (!matchesActiveStreamEvent(data)) {
        return;
    }

    if (type == QStringLiteral("ai.stream.chunk") ||
            type == QStringLiteral("ai.stream.chunk.delta")) {
        if (m_waitingForTerminalEvent) {
            return;
        }
        processStreamChunk(data);
        return;
    }

    if (type == QStringLiteral("ai.stream.done") ||
            type == QStringLiteral("ai.stream.done.assistantMessage")) {
        processTerminalEvent(data, false);
        return;
    }

    if (type == QStringLiteral("ai.stream.cancelled")) {
        processTerminalEvent(data, true);
        return;
    }

    if (type == QStringLiteral("ai.stream.thinking")) {
        if (!m_waitingForTerminalEvent) {
            const AiChatStreamStatus status = streamStatusFromObject(data);
            m_streamStatusActive = status.active;
            emit streamStatusChanged(status);
            emit thinkingStateChanged(status.active);
        }
        return;
    }

    if (type == QStringLiteral("client.error")) {
        const NetworkError error = errorFromObject(data);
        emitInactiveStreamStatus();
        clearActiveRequest();
        emit failed(error);
        emit finished();
    }
}

void AiChatStreamClient::handleRealtimeEvent(const QString& type, const QJsonObject& payload)
{
    if (type == QStringLiteral("ai.stream.started")) {
        if (!m_running) {
            return;
        }
        const QString clientMessageId = payload.value(QStringLiteral("clientMessageId")).toString();
        if (!m_clientMessageId.isEmpty() &&
                !clientMessageId.isEmpty() &&
                clientMessageId != m_clientMessageId) {
            return;
        }
        m_streamId = payload.value(QStringLiteral("streamId")).toString(m_streamId);
        emit streamStarted(m_streamId,
                           payload.value(QStringLiteral("conversationId")).toString(),
                           clientMessageId);
        return;
    }

    if (type == QStringLiteral("ai.stream.chunk") ||
            type == QStringLiteral("ai.stream.chunk.delta")) {
        if (matchesActiveRealtimeEvent(payload) && !m_waitingForTerminalEvent) {
            processStreamChunk(payload);
        }
        return;
    }

    if (type == QStringLiteral("ai.stream.thinking")) {
        if (matchesActiveRealtimeEvent(payload) && !m_waitingForTerminalEvent) {
            const AiChatStreamStatus status = streamStatusFromObject(payload);
            m_streamStatusActive = status.active;
            emit streamStatusChanged(status);
            emit thinkingStateChanged(status.active);
        }
        return;
    }

    if (type != QStringLiteral("ai.stream.done") &&
            type != QStringLiteral("ai.stream.done.assistantMessage") &&
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

    emitInactiveStreamStatus();
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
    emitInactiveStreamStatus();
    clearActiveRequest();
    emit cancelled();
}

void AiChatStreamClient::processStreamChunk(const QJsonObject& data)
{
    const AiChatStreamChunk chunk = streamChunkFromObject(data);
    if (chunk.delta.isEmpty() && !chunk.segmentStart && !chunk.segmentEnd) {
        return;
    }

    if (chunk.kind == QStringLiteral("answer") && m_streamStatusActive) {
        emitInactiveStreamStatus();
    }
    emit chunkReceived(chunk);
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
        AiChatMessage assistantMessage;
        assistantMessage.messageId = messageId;
        assistantMessage.conversationId =
                message.value(QStringLiteral("conversationId")).toString();
        assistantMessage.text = text;
        assistantMessage.isFromUser = false;
        assistantMessage.time = assistantMessageTime(message);
        assistantMessage.trace = traceFromObject(
                message.value(QStringLiteral("trace")).toObject());
        emit assistantMessageReceived(assistantMessage);
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

void AiChatStreamClient::emitInactiveStreamStatus()
{
    m_streamStatusActive = false;
    AiChatStreamStatus status;
    status.active = false;
    emit streamStatusChanged(status);
    emit thinkingStateChanged(false);
}

AiChatStreamChunk AiChatStreamClient::streamChunkFromObject(const QJsonObject& object)
{
    AiChatStreamChunk chunk;
    chunk.streamId = object.value(QStringLiteral("streamId")).toString();
    chunk.delta = object.value(QStringLiteral("delta")).toString(
            object.value(QStringLiteral("content")).toString());
    chunk.transient = object.value(QStringLiteral("transient")).toBool(false);
    chunk.kind = object.value(QStringLiteral("kind")).toString().trimmed().toLower();
    if (chunk.kind != QStringLiteral("progress") &&
            chunk.kind != QStringLiteral("answer")) {
        chunk.kind = chunk.transient
                ? QStringLiteral("progress")
                : QStringLiteral("answer");
    }
    chunk.stepId = object.value(QStringLiteral("stepId")).toString();
    chunk.segmentId = object.value(QStringLiteral("segmentId")).toString();
    chunk.segmentStart = object.value(QStringLiteral("segmentStart")).toBool(false);
    chunk.segmentEnd = object.value(QStringLiteral("segmentEnd")).toBool(false);
    return chunk;
}

AiChatStreamStatus AiChatStreamClient::streamStatusFromObject(const QJsonObject& object)
{
    AiChatStreamStatus status;
    status.active = object.value(QStringLiteral("active")).toBool(true);
    status.userVisible = object.value(QStringLiteral("userVisible")).toBool(false);
    status.stepId = object.value(QStringLiteral("stepId")).toString();
    status.sequence = object.value(QStringLiteral("sequence")).toInt();
    status.phase = object.value(QStringLiteral("phase")).toString().trimmed();
    status.status = object.value(QStringLiteral("status")).toString().trimmed();
    status.tool = object.value(QStringLiteral("tool")).toString().trimmed();
    status.query = object.value(QStringLiteral("query")).toString().trimmed();
    status.urls = stringVectorFromJsonValue(object.value(QStringLiteral("urls")));

    const QString url = object.value(QStringLiteral("url")).toString().trimmed();
    if (!url.isEmpty() && !status.urls.contains(url)) {
        status.urls.push_back(url);
    }

    status.message = object.value(QStringLiteral("message")).toString().trimmed();
    return status;
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

AiChatTrace AiChatStreamClient::traceFromObject(const QJsonObject& object)
{
    AiChatTrace trace;
    if (object.isEmpty()) {
        return trace;
    }

    trace.traceId = object.value(QStringLiteral("traceId")).toString();
    trace.status = object.value(QStringLiteral("status")).toString();
    trace.summary = object.value(QStringLiteral("summary")).toString();
    trace.createdAt = dateTimeFromString(object.value(QStringLiteral("createdAt")).toString());
    trace.updatedAt = dateTimeFromString(object.value(QStringLiteral("updatedAt")).toString());
    trace.sourceRefs = stringVectorFromJsonValue(object.value(QStringLiteral("sourceRefs")));

    for (const QJsonValue& value : object.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject stepObject = value.toObject();
        if (stepObject.isEmpty()) {
            continue;
        }
        AiChatTraceStep step;
        step.sequence = stepObject.value(QStringLiteral("sequence")).toInt();
        step.stepId = stepObject.value(QStringLiteral("stepId")).toString();
        step.phase = stepObject.value(QStringLiteral("phase")).toString();
        step.status = stepObject.value(QStringLiteral("status")).toString();
        step.text = stepObject.value(QStringLiteral("text")).toString();
        step.tool = stepObject.value(QStringLiteral("tool")).toString();
        step.query = stepObject.value(QStringLiteral("query")).toString();
        step.urls = stringVectorFromJsonValue(stepObject.value(QStringLiteral("urls")));
        trace.steps.push_back(step);
    }
    return trace;
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

bool AiChatStreamClient::matchesActiveStreamEvent(const QJsonObject& payload) const
{
    const QString streamId = payload.value(QStringLiteral("streamId")).toString();
    return m_streamId.isEmpty() || streamId.isEmpty() || streamId == m_streamId;
}

void AiChatStreamClient::clearActiveRequest()
{
    m_cancelTimeoutTimer.stop();
    m_requestId.clear();
    m_streamId.clear();
    m_clientMessageId.clear();
    m_running = false;
    m_streamStatusActive = false;
    m_waitingForTerminalEvent = false;
}
