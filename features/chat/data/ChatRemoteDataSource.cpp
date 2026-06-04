#include "ChatRemoteDataSource.h"

#include "shared/network/HttpClient.h"
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
    const QString uploadRequestId = UploadClient::instance().uploadFile(imagePath, QStringLiteral("chat_image"));
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

QString ChatRemoteDataSource::sendMessageRequest(const QString& conversationId,
                                                 const QJsonObject& body,
                                                 const QString& clientMessageId)
{
    NetworkRequest request = NetworkRequest::json(HttpMethod::Post,
                                                  QStringLiteral("/conversations/%1/messages").arg(conversationId),
                                                  body);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_clientMessageIdsByRequest.insert(requestId, clientMessageId);
    return clientMessageId;
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

    QJsonObject attachment{
            {QStringLiteral("fileId"), fileId},
            {QStringLiteral("displayName"), QFileInfo(pending.imagePath).fileName()},
            {QStringLiteral("width"), pending.width},
            {QStringLiteral("height"), pending.height}
    };
    QJsonArray attachments;
    attachments.append(attachment);

    QJsonObject content{
            {QStringLiteral("text"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("json"), QJsonValue(QJsonValue::Null)}
    };
    QJsonObject body = baseMessageBody(pending.clientMessageId,
                                       pending.clientSentAt,
                                       QStringLiteral("image"),
                                       pending.referencedMessageId);
    body.insert(QStringLiteral("content"), content);
    body.insert(QStringLiteral("attachments"), attachments);
    sendMessageRequest(pending.conversationId, body, pending.clientMessageId);
}

void ChatRemoteDataSource::handleUploadFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingImagesByUploadRequest.contains(requestId)) {
        return;
    }

    const PendingImage pending = m_pendingImagesByUploadRequest.take(requestId);
    emit imageUploadFailed(pending.clientMessageId, error);
}

void ChatRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse&)
{
    if (!m_clientMessageIdsByRequest.contains(requestId)) {
        return;
    }

    emit messageSendSucceeded(m_clientMessageIdsByRequest.take(requestId));
}

void ChatRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_clientMessageIdsByRequest.contains(requestId)) {
        return;
    }

    emit messageSendFailed(m_clientMessageIdsByRequest.take(requestId), error);
}
