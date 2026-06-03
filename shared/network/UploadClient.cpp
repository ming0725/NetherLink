#include "UploadClient.h"

#include "AuthSession.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

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
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        NetworkError error;
        error.code = QStringLiteral("LOCAL_FILE_OPEN_FAILED");
        error.message = file->errorString();
        file->deleteLater();
        emit uploadFailed(requestId, error);
        return requestId;
    }

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(path).fileName()));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QHttpPart targetPart;
    targetPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"targetType\""));
    targetPart.setBody(targetType.toUtf8());
    multiPart->append(targetPart);

    if (!contentHash.isEmpty()) {
        QHttpPart hashPart;
        hashPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"contentHash\""));
        hashPart.setBody(contentHash.toUtf8());
        multiPart->append(hashPart);
    }

    QNetworkRequest request(m_environment.apiUrl(QStringLiteral("/files"), query));
    request.setRawHeader("Accept", "application/json");
    if (AuthSession::instance().hasAccessToken()) {
        request.setRawHeader("Authorization", "Bearer " + AuthSession::instance().accessToken().toUtf8());
    }

    QNetworkReply* reply = m_manager->post(request, multiPart);
    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::uploadProgress, this, [this, requestId](qint64 sent, qint64 total) {
        emit uploadProgress(requestId, sent, total);
    });
    connect(reply, &QNetworkReply::finished, this, [this, requestId, reply]() {
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
            emit uploadSucceeded(requestId, response);
        } else {
            NetworkError error;
            error.httpStatus = status;
            error.rawBody = body;
            const QJsonObject object = QJsonDocument::fromJson(body).object();
            error.code = object.value(QStringLiteral("code")).toString();
            error.message = object.value(QStringLiteral("message")).toString(reply->errorString());
            error.requestId = object.value(QStringLiteral("requestId")).toString();
            error.details = object.value(QStringLiteral("details")).toObject();
            emit uploadFailed(requestId, error);
        }
        reply->deleteLater();
    });

    emit uploadStarted(requestId, path);
    return requestId;
}
