#include "AiChatStreamClient.h"

#include "shared/network/HttpClient.h"
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
}

bool AiChatStreamClient::isRunning() const
{
    return m_running || (m_sseClient && m_sseClient->isRunning());
}

void AiChatStreamClient::start(const QString& conversationId,
                               const QString& prompt,
                               const QString& clientMessageId)
{
    cancel();

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
        return;
    }

    if (type == QStringLiteral("ai.stream.chunk")) {
        const QString delta = data.value(QStringLiteral("delta")).toString(
                data.value(QStringLiteral("content")).toString());
        if (!delta.isEmpty()) {
            emit chunkReceived(delta);
        }
        return;
    }

    if (type == QStringLiteral("ai.stream.done")) {
        const QJsonObject message = assistantMessageObject(data);
        const QString text = assistantMessageText(message);
        if (!text.isEmpty()) {
            emit assistantMessageReceived(assistantMessageId(message),
                                          text,
                                          assistantMessageTime(message));
        }
        clearActiveRequest();
        emit finished();
        return;
    }

    if (type == QStringLiteral("ai.stream.cancelled")) {
        clearActiveRequest();
        emit finished();
        return;
    }

    if (type == QStringLiteral("client.error")) {
        const NetworkError error = errorFromObject(data);
        clearActiveRequest();
        emit failed(error);
        emit finished();
    }
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

bool AiChatStreamClient::matchesActiveRequest(const QString& requestId) const
{
    return m_running && !m_requestId.isEmpty() && requestId == m_requestId;
}

void AiChatStreamClient::clearActiveRequest()
{
    m_requestId.clear();
    m_clientMessageId.clear();
    m_running = false;
}
