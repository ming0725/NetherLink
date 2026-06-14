#include "UploadClient.h"

#include "AuthSession.h"
#include "NetworkLog.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSharedPointer>
#include <QUuid>

namespace {

QString mimeTypeForFile(const QString& path)
{
    QMimeDatabase database;
    QString mimeType = database.mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    if (mimeType.isEmpty() || mimeType == QStringLiteral("application/octet-stream")) {
        mimeType = database.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    }
    if (!mimeType.isEmpty() && mimeType != QStringLiteral("application/octet-stream")) {
        return mimeType;
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("png")) {
        return QStringLiteral("image/png");
    }
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        return QStringLiteral("image/jpeg");
    }
    if (suffix == QStringLiteral("gif")) {
        return QStringLiteral("image/gif");
    }
    if (suffix == QStringLiteral("webp")) {
        return QStringLiteral("image/webp");
    }
    if (suffix == QStringLiteral("bmp")) {
        return QStringLiteral("image/bmp");
    }
    return QStringLiteral("application/octet-stream");
}

QString uploadMultipart(QNetworkAccessManager* manager,
                        const BackendEnvironment& environment,
                        QObject* owner,
                        const QString& path,
                        const QString& endpoint,
                        const QString& targetType,
                        const QString& contentHash,
                        const QVariantMap& query)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* file = new QFile(path);
    auto* client = qobject_cast<UploadClient*>(owner);
    if (!file->open(QIODevice::ReadOnly)) {
        NetworkError error;
        error.code = QStringLiteral("LOCAL_FILE_OPEN_FAILED");
        error.message = file->errorString();
        file->deleteLater();
        if (client) {
            emit client->uploadFailed(requestId, error);
        }
        return requestId;
    }

    const QFileInfo fileInfo(path);
    const QString fileName = fileInfo.fileName();
    const QString mimeType = mimeTypeForFile(path);
    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    if (!targetType.isEmpty()) {
        QHttpPart targetPart;
        targetPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"targetType\""));
        targetPart.setBody(targetType.toUtf8());
        multiPart->append(targetPart);
    }

    if (!contentHash.isEmpty()) {
        QHttpPart hashPart;
        hashPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"contentHash\""));
        hashPart.setBody(contentHash.toUtf8());
        multiPart->append(hashPart);
    }

    QNetworkRequest request(environment.apiUrl(endpoint, query));
    request.setRawHeader("Accept", "application/json");
    if (AuthSession::instance().hasAccessToken()) {
        request.setRawHeader("Authorization", "Bearer " + AuthSession::instance().accessToken().toUtf8());
    }

    auto timer = QSharedPointer<QElapsedTimer>::create();
    timer->start();
    NetworkLog::uploadRequest(requestId, request.url(), fileName, mimeType, fileInfo.size(), query);

    QNetworkReply* reply = manager->post(request, multiPart);
    multiPart->setParent(reply);
    QObject::connect(reply, &QNetworkReply::uploadProgress, owner, [client, requestId](qint64 sent, qint64 total) {
        if (client) {
            emit client->uploadProgress(requestId, sent, total);
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, owner, [client, requestId, reply, timer]() {
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            NetworkResponse response;
            response.httpStatus = status;
            response.rawBody = body;
            response.body = QJsonDocument::fromJson(body);
            for (const QByteArray& header : reply->rawHeaderList()) {
                response.headers.insert(header, reply->rawHeader(header));
            }
            response.etag = QString::fromUtf8(reply->rawHeader("ETag"));
            response.requestId = QString::fromUtf8(reply->rawHeader("X-Request-Id"));
            NetworkLog::uploadResponse(requestId, response, timer->elapsed());
            if (client) {
                emit client->uploadSucceeded(requestId, response);
            }
        } else {
            NetworkError error;
            error.httpStatus = status;
            error.rawBody = body;
            const QJsonObject object = QJsonDocument::fromJson(body).object();
            error.code = object.value(QStringLiteral("code")).toString();
            error.message = object.value(QStringLiteral("message")).toString(reply->errorString());
            error.requestId = object.value(QStringLiteral("requestId")).toString(
                    QString::fromUtf8(reply->rawHeader("X-Request-Id")));
            error.details = object.value(QStringLiteral("details")).toObject();
            NetworkLog::uploadError(requestId, error, timer->elapsed());
            if (client) {
                emit client->uploadFailed(requestId, error);
            }
        }
        reply->deleteLater();
    });

    if (client) {
        emit client->uploadStarted(requestId, path);
    }
    return requestId;
}

} // namespace

UploadClient& UploadClient::instance()
{
    static UploadClient client;
    return client;
}

UploadClient::UploadClient(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

BackendEnvironment UploadClient::environment() const
{
    return m_environment;
}

void UploadClient::setEnvironment(const BackendEnvironment& environment)
{
    m_environment = environment;
}

QString UploadClient::uploadFile(const QString& path,
                                 const QString& targetType,
                                 const QString& contentHash,
                                 const QVariantMap& query)
{
    return uploadMultipart(m_manager,
                           m_environment,
                           this,
                           path,
                           QStringLiteral("/files"),
                           targetType,
                           contentHash,
                           query);
}

QString UploadClient::uploadAvatar(const QString& path,
                                   int expectedVersion,
                                   const QString& contentHash)
{
    QVariantMap query;
    if (expectedVersion > 0) {
        query.insert(QStringLiteral("expectedVersion"), expectedVersion);
    }
    return uploadMultipart(m_manager,
                           m_environment,
                           this,
                           path,
                           QStringLiteral("/me/avatar"),
                           {},
                           contentHash,
                           query);
}
