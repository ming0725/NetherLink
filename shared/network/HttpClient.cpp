#include "HttpClient.h"

#include "AuthSession.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
#include <QtGlobal>

struct HttpClient::Operation {
    QString id;
    NetworkRequest request;
    int attempts = 0;
    bool replayedAfterRefresh = false;
};

namespace {

QByteArray headerValue(QNetworkReply* reply, const QByteArray& name)
{
    return reply->rawHeader(name);
}

int retryDelayMs(int attempts)
{
    static const int baseDelays[] = {300, 800, 2000};
    const int index = qBound(0, attempts - 1, 2);
    return baseDelays[index] + static_cast<int>(QRandomGenerator::global()->bounded(0, 180));
}

} // namespace

HttpClient& HttpClient::instance()
{
    static HttpClient client;
    return client;
}

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    qRegisterMetaType<NetworkResponse>("NetworkResponse");
    qRegisterMetaType<NetworkError>("NetworkError");
    qRegisterMetaType<NetworkRequest>("NetworkRequest");
}

BackendEnvironment HttpClient::environment() const
{
    return m_environment;
}

void HttpClient::setEnvironment(const BackendEnvironment& environment)
{
    m_environment = environment;
}

QString HttpClient::send(const NetworkRequest& request)
{
    auto operation = QSharedPointer<Operation>::create();
    operation->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    operation->request = request;
    m_operations.insert(operation->id, operation);
    emit requestStarted(operation->id, request);
    startOperation(operation);
    return operation->id;
}

void HttpClient::refreshAccessToken()
{
    if (m_refreshing) {
        return;
    }
    if (!AuthSession::instance().hasRefreshToken()) {
        NetworkError error;
        error.httpStatus = 401;
        error.code = QStringLiteral("TOKEN_MISSING");
        error.message = QStringLiteral("Refresh token is missing.");
        emit authRefreshFailed(error);
        flushRefreshQueue(false, error);
        return;
    }

    m_refreshing = true;
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/auth/refresh"),
            {
                    {QStringLiteral("refreshToken"), AuthSession::instance().refreshToken()},
                    {QStringLiteral("deviceId"), AuthSession::instance().deviceId()}
            });
    request.requiresAuth = false;
    request.maxRetries = 0;

    QNetworkRequest networkRequest = buildNetworkRequest(request);
    QNetworkReply* reply = m_manager->post(networkRequest, request.body.toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            const QJsonObject object = QJsonDocument::fromJson(body).object();
            const QString accessToken = object.value(QStringLiteral("accessToken")).toString();
            const QString refreshToken = object.value(QStringLiteral("refreshToken")).toString(AuthSession::instance().refreshToken());
            const int expiresIn = object.value(QStringLiteral("expiresIn")).toInt(900);
            AuthSession::instance().setTokens(accessToken, refreshToken, expiresIn);
            m_refreshing = false;
            emit authRefreshSucceeded();
            flushRefreshQueue(true);
        } else {
            NetworkError error = errorFromReply(reply, body);
            if (error.code.isEmpty()) {
                error.code = QStringLiteral("TOKEN_REFRESH_FAILED");
            }
            m_refreshing = false;
            AuthSession::instance().clear();
            emit authRefreshFailed(error);
            flushRefreshQueue(false, error);
        }
        reply->deleteLater();
    });
}

void HttpClient::startOperation(const QSharedPointer<Operation>& operation)
{
    if (!operation) {
        return;
    }

    ++operation->attempts;
    QNetworkRequest networkRequest = buildNetworkRequest(operation->request);
    QByteArray body;
    if (operation->request.hasJsonBody) {
        body = operation->request.body.toJson(QJsonDocument::Compact);
    } else {
        body = operation->request.rawBody;
    }

    QNetworkReply* reply = nullptr;
    switch (operation->request.method) {
    case HttpMethod::Get:
        reply = m_manager->get(networkRequest);
        break;
    case HttpMethod::Post:
        reply = m_manager->post(networkRequest, body);
        break;
    case HttpMethod::Patch:
        reply = m_manager->sendCustomRequest(networkRequest, "PATCH", body);
        break;
    case HttpMethod::Delete:
        reply = body.isEmpty()
                ? m_manager->deleteResource(networkRequest)
                : m_manager->sendCustomRequest(networkRequest, "DELETE", body);
        break;
    }

    connect(reply, &QNetworkReply::finished, this, [this, operation, reply]() {
        handleReply(operation, reply);
        reply->deleteLater();
    });
}

void HttpClient::handleReply(const QSharedPointer<Operation>& operation, QNetworkReply* reply)
{
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 200 && status < 300) {
        NetworkResponse response = responseFromReply(reply);
        response.rawBody = body;
        response.body = QJsonDocument::fromJson(body);
        m_operations.remove(operation->id);
        emit requestSucceeded(operation->id, response);
        return;
    }

    NetworkError error = errorFromReply(reply, body);
    if (operation->request.requiresAuth &&
        error.isTokenExpired() &&
        !operation->replayedAfterRefresh &&
        !operation->request.path.contains(QStringLiteral("/auth/refresh"))) {
        queueForTokenRefresh(operation);
        return;
    }

    if (shouldRetry(*operation, error)) {
        retryLater(operation);
        return;
    }

    m_operations.remove(operation->id);
    emit requestFailed(operation->id, error);
}

void HttpClient::retryLater(const QSharedPointer<Operation>& operation)
{
    QTimer::singleShot(retryDelayMs(operation->attempts), this, [this, operation]() {
        if (m_operations.contains(operation->id)) {
            startOperation(operation);
        }
    });
}

void HttpClient::queueForTokenRefresh(const QSharedPointer<Operation>& operation)
{
    operation->replayedAfterRefresh = true;
    m_refreshQueue.push_back(operation);
    refreshAccessToken();
}

void HttpClient::flushRefreshQueue(bool success, const NetworkError& error)
{
    const QVector<QSharedPointer<Operation>> queued = m_refreshQueue;
    m_refreshQueue.clear();
    for (const QSharedPointer<Operation>& operation : queued) {
        if (!m_operations.contains(operation->id)) {
            continue;
        }
        if (success) {
            startOperation(operation);
        } else {
            m_operations.remove(operation->id);
            emit requestFailed(operation->id, error);
        }
    }
}

bool HttpClient::shouldRetry(const Operation& operation, const NetworkError& error) const
{
    if (operation.request.maxRetries <= 0 || operation.attempts > operation.request.maxRetries) {
        return false;
    }

    const bool idempotent = operation.request.method == HttpMethod::Get || operation.request.hasIdempotencyHint();
    if (!idempotent) {
        return false;
    }

    if (error.code == QStringLiteral("VERSION_CONFLICT")) {
        return false;
    }
    if (error.code == QStringLiteral("OPERATION_IN_PROGRESS")) {
        return operation.request.hasIdempotencyHint();
    }
    if (error.isRetryableServiceError()) {
        return true;
    }
    return error.httpStatus == 0;
}

QNetworkRequest HttpClient::buildNetworkRequest(const NetworkRequest& request) const
{
    QNetworkRequest networkRequest(m_environment.apiUrl(request.path, request.query));
    networkRequest.setRawHeader("Accept", request.headers.value("Accept", "application/json"));
    if (request.hasJsonBody) {
        networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    } else if (!request.contentType.isEmpty()) {
        networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromUtf8(request.contentType));
    }
    if (request.requiresAuth && AuthSession::instance().hasAccessToken()) {
        networkRequest.setRawHeader("Authorization", "Bearer " + AuthSession::instance().accessToken().toUtf8());
    }
    for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it) {
        networkRequest.setRawHeader(it.key(), it.value());
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    networkRequest.setTransferTimeout(request.timeoutMs);
#endif
    return networkRequest;
}

NetworkResponse HttpClient::responseFromReply(QNetworkReply* reply) const
{
    NetworkResponse response;
    response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    for (const QByteArray& header : reply->rawHeaderList()) {
        response.headers.insert(header, reply->rawHeader(header));
    }
    response.etag = QString::fromUtf8(headerValue(reply, "ETag"));
    response.requestId = QString::fromUtf8(headerValue(reply, "X-Request-Id"));
    return response;
}

NetworkError HttpClient::errorFromReply(QNetworkReply* reply, const QByteArray& body) const
{
    NetworkError error;
    error.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    error.rawBody = body;
    const QJsonObject object = QJsonDocument::fromJson(body).object();
    error.code = object.value(QStringLiteral("code")).toString();
    error.message = object.value(QStringLiteral("message")).toString();
    error.requestId = object.value(QStringLiteral("requestId")).toString(QString::fromUtf8(headerValue(reply, "X-Request-Id")));
    error.details = object.value(QStringLiteral("details")).toObject();
    if (error.message.isEmpty()) {
        error.message = reply->errorString();
    }
    return error;
}
