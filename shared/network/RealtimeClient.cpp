#include "RealtimeClient.h"

#include "AuthSession.h"
#include "EventCursorStore.h"
#include "HttpClient.h"
#include "RealtimeEventDispatcher.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>

namespace {

QString stateName(RealtimeClient::State state)
{
    switch (state) {
    case RealtimeClient::State::Idle:
        return QStringLiteral("idle");
    case RealtimeClient::State::Connecting:
        return QStringLiteral("connecting");
    case RealtimeClient::State::Ready:
        return QStringLiteral("ready");
    case RealtimeClient::State::Stale:
        return QStringLiteral("stale");
    case RealtimeClient::State::Reconnecting:
        return QStringLiteral("reconnecting");
    case RealtimeClient::State::Closed:
        return QStringLiteral("closed");
    }
    return QStringLiteral("unknown");
}

QString redactedUrl(QUrl url)
{
    QUrlQuery query(url);
    if (query.hasQueryItem(QStringLiteral("accessToken"))) {
        query.removeAllQueryItems(QStringLiteral("accessToken"));
        query.addQueryItem(QStringLiteral("accessToken"), QStringLiteral("<redacted>"));
        url.setQuery(query);
    }
    return url.toString(QUrl::FullyEncoded);
}

QString closeReasonOrPlaceholder(const QString& closeReason)
{
    return closeReason.isEmpty() ? QStringLiteral("<empty>") : closeReason;
}

QString socketErrorOrPlaceholder(const QString& errorString)
{
    return errorString.isEmpty() ? QStringLiteral("<empty>") : errorString;
}

int handshakeHttpStatusFromError(const QString& errorString)
{
    static const QRegularExpression pattern(QStringLiteral("status code:\\s*(\\d+)"));
    const QRegularExpressionMatch match = pattern.match(errorString);
    if (!match.hasMatch()) {
        return 0;
    }
    return match.captured(1).toInt();
}

} // namespace

RealtimeClient& RealtimeClient::instance()
{
    static RealtimeClient client;
    return client;
}

RealtimeClient::RealtimeClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    qRegisterMetaType<RealtimeEvent>("RealtimeEvent");

    m_pingTimer.setInterval(25000);
    connect(&m_pingTimer, &QTimer::timeout, this, &RealtimeClient::sendPing);

    m_staleTimer.setInterval(60000);
    m_staleTimer.setSingleShot(true);
    connect(&m_staleTimer, &QTimer::timeout, this, [this]() {
        qWarning().noquote() << "Realtime WebSocket stale, closing for reconnect"
                             << "url=" << redactedUrl(m_currentUrl);
        setState(State::Stale);
        m_socket->close();
        scheduleReconnect();
    });

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RealtimeClient::connectToServer);

    connect(m_socket, &QWebSocket::connected, this, [this]() {
        qInfo().noquote() << "Realtime WebSocket connected"
                          << "url=" << redactedUrl(m_currentUrl);
        m_reconnectAttempt = 0;
        setState(State::Ready);
        resetHeartbeat();
        sendResume();
        emit connected();
    });
    connect(m_socket, &QWebSocket::disconnected, this, [this]() {
        m_pingTimer.stop();
        m_staleTimer.stop();
        emit disconnected();
        if (m_userClosed) {
            qInfo().noquote() << "Realtime WebSocket closed by client"
                              << "code=" << static_cast<int>(m_socket->closeCode())
                              << "reason=" << closeReasonOrPlaceholder(m_socket->closeReason())
                              << "url=" << redactedUrl(m_currentUrl);
            setState(State::Closed);
            return;
        }
        if (m_suppressNextReconnect) {
            qWarning().noquote() << "Realtime WebSocket disconnected without reconnect"
                                 << "httpStatus=" << m_lastHandshakeHttpStatus
                                 << "code=" << static_cast<int>(m_socket->closeCode())
                                 << "reason=" << closeReasonOrPlaceholder(m_socket->closeReason())
                                 << "socketError=" << socketErrorOrPlaceholder(m_socket->errorString())
                                 << "url=" << redactedUrl(m_currentUrl);
            m_suppressNextReconnect = false;
            setState(State::Closed);
            return;
        }
        qWarning().noquote() << "Realtime WebSocket disconnected, scheduling reconnect"
                             << "code=" << static_cast<int>(m_socket->closeCode())
                             << "reason=" << closeReasonOrPlaceholder(m_socket->closeReason())
                             << "socketError=" << socketErrorOrPlaceholder(m_socket->errorString())
                             << "url=" << redactedUrl(m_currentUrl);
        scheduleReconnect();
    });
    connect(m_socket, &QWebSocket::textMessageReceived, this, &RealtimeClient::handleTextMessage);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        const QString errorString = m_socket->errorString();
        m_lastHandshakeHttpStatus = handshakeHttpStatusFromError(errorString);
        if (m_lastHandshakeHttpStatus == 401 || m_lastHandshakeHttpStatus == 403) {
            m_suppressNextReconnect = true;
        }
        qWarning().noquote() << "Realtime WebSocket error"
                             << "error=" << static_cast<int>(error)
                             << "httpStatus=" << m_lastHandshakeHttpStatus
                             << "message=" << socketErrorOrPlaceholder(errorString)
                             << "url=" << redactedUrl(m_currentUrl);
        if (m_lastHandshakeHttpStatus == 401) {
            HttpClient::instance().refreshAccessToken();
        }
        emit connectionError(errorString);
    });
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error) {
        const QString errorString = m_socket->errorString();
        m_lastHandshakeHttpStatus = handshakeHttpStatusFromError(errorString);
        if (m_lastHandshakeHttpStatus == 401 || m_lastHandshakeHttpStatus == 403) {
            m_suppressNextReconnect = true;
        }
        qWarning().noquote() << "Realtime WebSocket error"
                             << "error=" << static_cast<int>(error)
                             << "httpStatus=" << m_lastHandshakeHttpStatus
                             << "message=" << socketErrorOrPlaceholder(errorString)
                             << "url=" << redactedUrl(m_currentUrl);
        if (m_lastHandshakeHttpStatus == 401) {
            HttpClient::instance().refreshAccessToken();
        }
        emit connectionError(errorString);
    });
#endif

    connect(&HttpClient::instance(), &HttpClient::authRefreshSucceeded, this, [this]() {
        if (!m_userClosed && (m_state == State::Reconnecting || m_state == State::Stale || m_state == State::Closed)) {
            scheduleReconnect(true);
        }
    });
}

RealtimeClient::State RealtimeClient::state() const
{
    return m_state;
}

BackendEnvironment RealtimeClient::environment() const
{
    return m_environment;
}

void RealtimeClient::setEnvironment(const BackendEnvironment& environment)
{
    m_environment = environment;
}

bool RealtimeClient::isConnected() const
{
    return m_state == State::Ready;
}

void RealtimeClient::connectToServer()
{
    if (m_state == State::Connecting || m_state == State::Ready) {
        return;
    }
    m_userClosed = false;
    m_lastHandshakeHttpStatus = 0;
    setState(m_reconnectAttempt > 0 ? State::Reconnecting : State::Connecting);

    const qint64 lastSeq = EventCursorStore::instance().lastEventSeq();
    const QString lastEventId = EventCursorStore::instance().lastEventId();
    const QString accessToken = AuthSession::instance().accessToken();

    m_currentUrl = m_environment.realtimeUrl(accessToken, lastSeq, lastEventId);
    qInfo().noquote() << "Realtime WebSocket opening"
                      << "state=" << stateName(m_state)
                      << "attempt=" << m_reconnectAttempt
                      << "lastEventSeq=" << lastSeq
                      << "lastEventId=" << (lastEventId.isEmpty() ? QStringLiteral("<empty>") : lastEventId)
                      << "url=" << redactedUrl(m_currentUrl);

    QNetworkRequest request(m_currentUrl);
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    }
    m_socket->open(request);
}

void RealtimeClient::disconnectFromServer()
{
    m_userClosed = true;
    m_reconnectTimer.stop();
    m_pingTimer.stop();
    m_staleTimer.stop();
    m_socket->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("client closed"));
    setState(State::Closed);
}

void RealtimeClient::sendPing()
{
    sendJson({{QStringLiteral("type"), QStringLiteral("ping")},
              {QStringLiteral("payload"), QJsonObject{}}});
}

void RealtimeClient::sendResume()
{
    const qint64 lastSeq = EventCursorStore::instance().lastEventSeq();
    if (lastSeq <= 0 || m_state != State::Ready) {
        return;
    }
    sendJson({{QStringLiteral("type"), QStringLiteral("resume")},
              {QStringLiteral("payload"), QJsonObject{{QStringLiteral("lastEventSeq"), static_cast<double>(lastSeq)}}}});
}

void RealtimeClient::sendJson(const QJsonObject& message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning().noquote() << "Realtime WebSocket send skipped because socket is not connected"
                             << "type=" << message.value(QStringLiteral("type")).toString(QStringLiteral("<empty>"))
                             << "socketState=" << static_cast<int>(m_socket->state())
                             << "url=" << redactedUrl(m_currentUrl);
        return;
    }
    m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void RealtimeClient::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    qInfo().noquote() << "Realtime WebSocket state changed"
                      << "state=" << stateName(m_state);
    emit stateChanged(m_state);
}

void RealtimeClient::scheduleReconnect(bool immediate)
{
    if (m_userClosed) {
        return;
    }
    ++m_reconnectAttempt;
    setState(State::Reconnecting);
    const int delayMs = immediate ? 0 : reconnectDelayMs();
    qWarning().noquote() << "Realtime WebSocket reconnect scheduled"
                         << "attempt=" << m_reconnectAttempt
                         << "delayMs=" << delayMs;
    m_reconnectTimer.start(delayMs);
}

void RealtimeClient::resetHeartbeat()
{
    m_pingTimer.start();
    m_staleTimer.start();
}

void RealtimeClient::handleTextMessage(const QString& message)
{
    resetHeartbeat();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning().noquote() << "Realtime WebSocket received invalid JSON"
                             << "error=" << parseError.errorString()
                             << "messageSize=" << message.size();
        return;
    }

    const QJsonObject object = document.object();
    if (object.isEmpty()) {
        qWarning().noquote() << "Realtime WebSocket received empty JSON object";
        return;
    }

    const RealtimeEvent event = RealtimeEvent::fromJson(object);
    if (event.type.isEmpty()) {
        qWarning().noquote() << "Realtime WebSocket received event without type"
                             << "raw=" << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    } else if (event.isControlEvent()) {
        qInfo().noquote() << "Realtime WebSocket received control event"
                          << "type=" << event.type
                          << "eventSeq=" << event.eventSeq
                          << "eventId=" << (event.eventId.isEmpty() ? QStringLiteral("<empty>") : event.eventId);
    }

    if (event.type == QStringLiteral("client.error")) {
        handleClientError(event);
    }

    emit eventReceived(event);
    RealtimeEventDispatcher::instance().dispatch(event);
}

void RealtimeClient::handleClientError(const RealtimeEvent& event)
{
    const QString code = event.payload.value(QStringLiteral("code")).toString(event.raw.value(QStringLiteral("code")).toString());
    const QString message = event.payload.value(QStringLiteral("message")).toString(event.raw.value(QStringLiteral("message")).toString());
    qWarning().noquote() << "Realtime WebSocket client.error received"
                         << "code=" << (code.isEmpty() ? QStringLiteral("<empty>") : code)
                         << "message=" << (message.isEmpty() ? QStringLiteral("<empty>") : message);
    if (code == QStringLiteral("TOKEN_EXPIRED")) {
        m_socket->close();
        HttpClient::instance().refreshAccessToken();
    }
}

int RealtimeClient::reconnectDelayMs() const
{
    static const int delays[] = {0, 1000, 2000, 5000, 10000, 30000};
    const int index = qBound(0, m_reconnectAttempt, 5);
    return delays[index] + static_cast<int>(QRandomGenerator::global()->bounded(0, 350));
}
