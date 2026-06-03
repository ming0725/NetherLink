#include "SseClient.h"

#include "AuthSession.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

SseClient::SseClient(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

void SseClient::setEnvironment(const BackendEnvironment& environment)
{
    m_environment = environment;
}

QString SseClient::start(const NetworkRequest& request)
{
    cancel();
    m_requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QNetworkRequest networkRequest(m_environment.apiUrl(request.path, request.query));
    networkRequest.setRawHeader("Accept", "text/event-stream");
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (request.requiresAuth && AuthSession::instance().hasAccessToken()) {
        networkRequest.setRawHeader("Authorization", "Bearer " + AuthSession::instance().accessToken().toUtf8());
    }
    for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it) {
        networkRequest.setRawHeader(it.key(), it.value());
    }

    const QByteArray body = request.hasJsonBody ? request.body.toJson(QJsonDocument::Compact) : request.rawBody;
    m_reply = m_manager->post(networkRequest, body);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        m_buffer += m_reply->readAll();
        parseBufferedEvents();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            if (!m_buffer.trimmed().isEmpty()) {
                emitEventBlock(m_buffer);
            }
            emit streamFinished(m_requestId);
        } else {
            NetworkError error;
            error.httpStatus = status;
            error.rawBody = m_reply->readAll();
            const QJsonObject object = QJsonDocument::fromJson(error.rawBody).object();
            error.code = object.value(QStringLiteral("code")).toString();
            error.message = object.value(QStringLiteral("message")).toString(m_reply->errorString());
            error.requestId = object.value(QStringLiteral("requestId")).toString();
            error.details = object.value(QStringLiteral("details")).toObject();
            emit streamFailed(m_requestId, error);
        }
        m_reply->deleteLater();
        m_reply = nullptr;
        m_buffer.clear();
    });

    emit streamStarted(m_requestId);
    return m_requestId;
}

bool SseClient::isRunning() const
{
    return m_reply != nullptr;
}

void SseClient::cancel()
{
    if (!m_reply) {
        return;
    }
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
    m_buffer.clear();
}

void SseClient::parseBufferedEvents()
{
    while (true) {
        int split = m_buffer.indexOf("\n\n");
        int delimiterLength = 2;
        if (split < 0) {
            split = m_buffer.indexOf("\r\n\r\n");
            delimiterLength = 4;
        }
        if (split < 0) {
            return;
        }
        const QByteArray block = m_buffer.left(split);
        m_buffer.remove(0, split + delimiterLength);
        emitEventBlock(block);
    }
}

void SseClient::emitEventBlock(const QByteArray& block)
{
    QString eventName;
    QByteArray data;
    const QList<QByteArray> lines = block.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.startsWith("event:")) {
            eventName = QString::fromUtf8(line.mid(6).trimmed());
        } else if (line.startsWith("data:")) {
            if (!data.isEmpty()) {
                data += '\n';
            }
            data += line.mid(5).trimmed();
        }
    }
    if (eventName.isEmpty() && data.isEmpty()) {
        return;
    }
    emit eventReceived(m_requestId, eventName, QJsonDocument::fromJson(data).object());
}
