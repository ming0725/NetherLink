#pragma once

#include "shared/network/NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

class ChatRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static ChatRemoteDataSource& instance();

    QString sendTextMessage(const QString& conversationId,
                            const QString& text,
                            const QString& referencedMessageId = {},
                            const QString& clientMessageId = {});
    QString sendImageMessage(const QString& conversationId,
                             const QString& imagePath,
                             const QString& referencedMessageId = {},
                             const QString& clientMessageId = {});

signals:
    void messageSendSucceeded(const QString& clientMessageId);
    void messageSendFailed(const QString& clientMessageId, const NetworkError& error);
    void imageUploadFailed(const QString& clientMessageId, const NetworkError& error);

private:
    struct PendingImage {
        QString conversationId;
        QString imagePath;
        QString referencedMessageId;
        QString clientMessageId;
        QString clientSentAt;
        int width = 0;
        int height = 0;
    };

    explicit ChatRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(ChatRemoteDataSource)

    QString sendMessageRequest(const QString& conversationId,
                               const QJsonObject& body,
                               const QString& clientMessageId);
    void handleUploadSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleUploadFailed(const QString& requestId, const NetworkError& error);
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, PendingImage> m_pendingImagesByUploadRequest;
    QHash<QString, QString> m_clientMessageIdsByRequest;
};
