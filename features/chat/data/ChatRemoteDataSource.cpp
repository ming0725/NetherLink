#include "ChatRemoteDataSource.h"

#include "shared/network/HttpClient.h"
#include "shared/network/AppEventBus.h"
#include "shared/network/ReferenceDataResolver.h"
#include "shared/network/UploadClient.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

namespace {

QString newClientMessageId()
{
    return QStringLiteral("msg_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QJsonObject fileObjectFromResponse(QJsonObject object)
{
    if (object.value(QStringLiteral("file")).isObject()) {
        object = object.value(QStringLiteral("file")).toObject();
    }
    return object;
}

QString fileIdFromResponse(const QJsonObject& object)
{
    return object.value(QStringLiteral("fileId")).toString(
            object.value(QStringLiteral("id")).toString());
}

QJsonObject baseMessageBody(const QString& clientMessageId,
                            const QString& clientSentAt,
                            const QString& type,
                            const QString& referencedMessageId)
{
    QJsonObject body{
            {QStringLiteral("clientMessageId"), clientMessageId},
            {QStringLiteral("clientSentAt"), clientSentAt},
            {QStringLiteral("type"), type}
    };
    body.insert(QStringLiteral("referencedMessageId"),
                referencedMessageId.isEmpty()
                        ? QJsonValue(QJsonValue::Null)
                        : QJsonValue(referencedMessageId));
    return body;
}

} // namespace

ChatRemoteDataSource& ChatRemoteDataSource::instance()
{
    static ChatRemoteDataSource dataSource;
    return dataSource;
}

ChatRemoteDataSource::ChatRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&UploadClient::instance(),
            &UploadClient::uploadSucceeded,
            this,
            &ChatRemoteDataSource::handleUploadSucceeded);
    connect(&UploadClient::instance(),
            &UploadClient::uploadFailed,
            this,
            &ChatRemoteDataSource::handleUploadFailed);
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &ChatRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &ChatRemoteDataSource::handleRequestFailed);
    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (type != QStringLiteral("client.error") ||
                    payload.value(QStringLiteral("requestType")).toString() != QStringLiteral("chat.message.send")) {
                    return;
                }

                NetworkError error;
                error.code = payload.value(QStringLiteral("code")).toString();
                error.message = payload.value(QStringLiteral("message")).toString();
                error.requestId = payload.value(QStringLiteral("requestId")).toString();
                error.details = payload;

                const QString clientMessageId = payload.value(QStringLiteral("clientMessageId")).toString(
                        payload.value(QStringLiteral("client_message_id")).toString());
                if (!clientMessageId.isEmpty()) {
                    emit messageSendFailed(clientMessageId, error);
                    return;
                }
                emit messageSendBlocked(error);
            });
}

QString ChatRemoteDataSource::sendTextMessage(const QString& conversationId,
                                              const QString& text,
                                              const QString& referencedMessageId,
                                              const QString& clientMessageId)
{
    const QString trimmedText = text.trimmed();
    if (conversationId.isEmpty() || trimmedText.isEmpty()) {
        return {};
    }

    const QString resolvedClientMessageId = clientMessageId.isEmpty() ? newClientMessageId() : clientMessageId;
    QJsonObject content{
            {QStringLiteral("text"), trimmedText},
            {QStringLiteral("json"), QJsonValue(QJsonValue::Null)}
    };
    QJsonObject body = baseMessageBody(resolvedClientMessageId,
                                       utcNow(),
                                       QStringLiteral("text"),
                                       referencedMessageId);
    body.insert(QStringLiteral("content"), content);
    body.insert(QStringLiteral("attachments"), QJsonArray{});
    return sendMessageRequest(conversationId, body, resolvedClientMessageId);
}

QString ChatRemoteDataSource::sendImageMessage(const QString& conversationId,
                                               const QString& imagePath,
                                               const QString& referencedMessageId,
                                               const QString& clientMessageId)
{
    if (conversationId.isEmpty() || imagePath.isEmpty()) {
        return {};
    }
    if (!QFileInfo(imagePath).isReadable()) {
        NetworkError error;
        error.code = QStringLiteral("LOCAL_FILE_OPEN_FAILED");
        error.message = QStringLiteral("Image file is not readable.");
        const QString resolvedClientMessageId = clientMessageId.isEmpty() ? newClientMessageId() : clientMessageId;
        emit imageUploadFailed(resolvedClientMessageId, error);
        return {};
    }

    QImageReader reader(imagePath);
    const QSize size = reader.size();
    if (!size.isValid()) {
        NetworkError error;
        error.code = QStringLiteral("LOCAL_IMAGE_INVALID");
        error.message = QStringLiteral("Image file is invalid.");
        const QString resolvedClientMessageId = clientMessageId.isEmpty() ? newClientMessageId() : clientMessageId;
        emit imageUploadFailed(resolvedClientMessageId, error);
        return {};
    }

    const QString resolvedClientMessageId = clientMessageId.isEmpty() ? newClientMessageId() : clientMessageId;
    const auto uploadedIt = m_uploadedImagesByClientMessageId.constFind(resolvedClientMessageId);
    if (uploadedIt != m_uploadedImagesByClientMessageId.cend()) {
        return sendUploadedImageMessage(conversationId,
                                        uploadedIt.value(),
                                        referencedMessageId,
                                        resolvedClientMessageId,
                                        utcNow());
    }

    const QString uploadRequestId = UploadClient::instance().uploadFile(imagePath, QStringLiteral("chat_image"));
    if (uploadRequestId.isEmpty()) {
        NetworkError error;
        error.code = QStringLiteral("UPLOAD_REQUEST_FAILED");
        error.message = QStringLiteral("Unable to start image upload.");
        emit imageUploadFailed(resolvedClientMessageId, error);
        return {};
    }
    PendingImage pending;
    pending.conversationId = conversationId;
    pending.imagePath = imagePath;
    pending.referencedMessageId = referencedMessageId;
    pending.clientMessageId = resolvedClientMessageId;
    pending.clientSentAt = utcNow();
    pending.width = size.width();
    pending.height = size.height();
    m_pendingImagesByUploadRequest.insert(uploadRequestId, pending);
    return resolvedClientMessageId;
}

QString ChatRemoteDataSource::recallMessage(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return {};
    }

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/conversations/%1/messages/%2/recall").arg(conversationId, messageId),
            {});
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return {};
    }

    m_pendingRecallsByRequest.insert(requestId, PendingRecall{conversationId, messageId});
    return requestId;
}

QString ChatRemoteDataSource::sendMessageRequest(const QString& conversationId,
                                                 const QJsonObject& body,
                                                 const QString& clientMessageId)
{
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/conversations/%1/messages").arg(conversationId),
                                                  body);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    if (!requestId.isEmpty()) {
        m_clientMessageIdsByRequest.insert(requestId, clientMessageId);
    }
    return clientMessageId;
}

QString ChatRemoteDataSource::sendUploadedImageMessage(const QString& conversationId,
                                                       const UploadedImage& image,
                                                       const QString& referencedMessageId,
                                                       const QString& clientMessageId,
                                                       const QString& clientSentAt)
{
    if (conversationId.isEmpty() || image.fileId.isEmpty() || clientMessageId.isEmpty()) {
        return {};
    }

    QJsonObject attachment{
            {QStringLiteral("fileId"), image.fileId},
            {QStringLiteral("displayName"), image.displayName},
            {QStringLiteral("width"), image.width},
            {QStringLiteral("height"), image.height}
    };
    QJsonArray attachments;
    attachments.append(attachment);

    QJsonObject content{
            {QStringLiteral("text"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("json"), QJsonValue(QJsonValue::Null)}
    };
    QJsonObject body = baseMessageBody(clientMessageId,
                                       clientSentAt,
                                       QStringLiteral("image"),
                                       referencedMessageId);
    body.insert(QStringLiteral("content"), content);
    body.insert(QStringLiteral("attachments"), attachments);
    return sendMessageRequest(conversationId, body, clientMessageId);
}

void ChatRemoteDataSource::handleUploadSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingImagesByUploadRequest.contains(requestId)) {
        return;
    }

    const PendingImage pending = m_pendingImagesByUploadRequest.take(requestId);
    const QJsonObject file = fileObjectFromResponse(response.object());
    const QString fileId = fileIdFromResponse(file);
    if (fileId.isEmpty()) {
        NetworkError error;
        error.code = QStringLiteral("FILE_ID_MISSING");
        error.message = QStringLiteral("Uploaded file response did not include fileId.");
        emit imageUploadFailed(pending.clientMessageId, error);
        return;
    }

    UploadedImage uploaded;
    uploaded.fileId = fileId;
    uploaded.displayName = QFileInfo(pending.imagePath).fileName();
    uploaded.width = pending.width;
    uploaded.height = pending.height;
    m_uploadedImagesByClientMessageId.insert(pending.clientMessageId, uploaded);
    emit imageUploadSucceeded(pending.clientMessageId);

    sendUploadedImageMessage(pending.conversationId,
                             uploaded,
                             pending.referencedMessageId,
                             pending.clientMessageId,
                             pending.clientSentAt);
}

void ChatRemoteDataSource::handleUploadFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingImagesByUploadRequest.contains(requestId)) {
        return;
    }

    const PendingImage pending = m_pendingImagesByUploadRequest.take(requestId);
    emit imageUploadFailed(pending.clientMessageId, error);
}

void ChatRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    ReferenceDataResolver::instance().consumePayload(response.object());

    if (m_clientMessageIdsByRequest.contains(requestId)) {
        const QString clientMessageId = m_clientMessageIdsByRequest.take(requestId);
        emit messageSendSucceeded(clientMessageId);
        return;
    }

    if (!m_pendingRecallsByRequest.contains(requestId)) {
        return;
    }

    const PendingRecall pending = m_pendingRecallsByRequest.take(requestId);
    QJsonObject message = response.object().value(QStringLiteral("message")).toObject();
    if (message.isEmpty()) {
        message = response.object().value(QStringLiteral("replacementMessage")).toObject();
    }
    if (message.isEmpty()) {
        message = response.object();
    }
    if (!message.contains(QStringLiteral("conversationId"))) {
        message.insert(QStringLiteral("conversationId"), pending.conversationId);
    }
    if (!message.contains(QStringLiteral("messageId"))) {
        message.insert(QStringLiteral("messageId"), pending.messageId);
    }
    emit messageRecallSucceeded(requestId, pending.conversationId, message);
}

void ChatRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (m_clientMessageIdsByRequest.contains(requestId)) {
        const QString clientMessageId = m_clientMessageIdsByRequest.take(requestId);
        emit messageSendFailed(clientMessageId, error);
        return;
    }

    if (!m_pendingRecallsByRequest.contains(requestId)) {
        return;
    }

    const PendingRecall pending = m_pendingRecallsByRequest.take(requestId);
    emit messageRecallFailed(requestId, pending.conversationId, pending.messageId, error);
}
