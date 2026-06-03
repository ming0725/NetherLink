#include "RealtimeClient.h"

#include "AuthSession.h"
#include "EventCursorStore.h"
#include "HttpClient.h"
#include "RealtimeEventDispatcher.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrl>
#include <QWebSocket>

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
        setState(State::Stale);
        m_socket->close();
        scheduleReconnect();
    });

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RealtimeClient::connectToServer);

    connect(m_socket, &QWebSocket::connected, this, [this]() {
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
            setState(State::Closed);
            return;
        }
        scheduleReconnect();
    });
    connect(m_socket, &QWebSocket::textMessageReceived, this, &RealtimeClient::handleTextMessage);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket->errorString());
    });
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket->errorString());
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
    setState(m_reconnectAttempt > 0 ? State::Reconnecting : State::Connecting);

    const qint64 lastSeq = EventCursorStore::instance().lastEventSeq();
    const QString lastEventId = EventCursorStore::instance().lastEventId();
    const QString accessToken = AuthSession::instance().accessToken();

    QNetworkRequest request(m_environment.realtimeUrl(accessToken, lastSeq, lastEventId));
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
    emit stateChanged(m_state);
}

void RealtimeClient::scheduleReconnect(bool immediate)
{
    if (m_userClosed) {
        return;
    }
    ++m_reconnectAttempt;
    setState(State::Reconnecting);
    m_reconnectTimer.start(immediate ? 0 : reconnectDelayMs());
}

void RealtimeClient::resetHeartbeat()
{
    m_pingTimer.start();
    m_staleTimer.start();
}

void RealtimeClient::handleTextMessage(const QString& message)
{
    resetHeartbeat();
    const QJsonObject object = QJsonDocument::fromJson(message.toUtf8()).object();
    if (object.isEmpty()) {
        return;
    }

    const RealtimeEvent event = RealtimeEvent::fromJson(object);
    if (event.type == QStringLiteral("client.error")) {
        handleClientError(event);
    }

    emit eventReceived(event);
    RealtimeEventDispatcher::instance().dispatch(event);
}

void RealtimeClient::handleClientError(const RealtimeEvent& event)
{
    const QString code = event.payload.value(QStringLiteral("code")).toString(event.raw.value(QStringLiteral("code")).toString());
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
