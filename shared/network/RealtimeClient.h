#pragma once

#include "NetworkTypes.h"

#include <QObject>
#include <QTimer>
#include <QUrl>

class QWebSocket;

class RealtimeClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Connecting,
        Ready,
        Stale,
        Reconnecting,
        Closed
    };
    Q_ENUM(State)

    static RealtimeClient& instance();

    State state() const;
    BackendEnvironment environment() const;
    void setEnvironment(const BackendEnvironment& environment);
    bool isConnected() const;
    QString currentSessionId() const;

public slots:
    void connectToServer();
    void disconnectFromServer();
    void sendPing();
    void sendResume();
    void sendJson(const QJsonObject& message);
    void sendPresenceUpdate(const QString& status);

signals:
    void stateChanged(RealtimeClient::State state);
    void connected();
    void disconnected();
    void eventReceived(const RealtimeEvent& event);
    void connectionError(const QString& message);
    void sessionRevoked(const QString& sessionId, const QString& message);

private:
    explicit RealtimeClient(QObject* parent = nullptr);
    Q_DISABLE_COPY(RealtimeClient)

    void setState(State state);
    void scheduleReconnect(bool immediate = false);
    void resetHeartbeat();
    void handleTextMessage(const QString& message);
    void handleRealtimeReady(const RealtimeEvent& event);
    bool handleSessionRevoked(const RealtimeEvent& event);
    void handleClientError(const RealtimeEvent& event);
    int reconnectDelayMs() const;

    QWebSocket* m_socket = nullptr;
    BackendEnvironment m_environment;
    QString m_currentSessionId;
    State m_state = State::Idle;
    QTimer m_pingTimer;
    QTimer m_staleTimer;
    QTimer m_reconnectTimer;
    QUrl m_currentUrl;
    int m_lastHandshakeHttpStatus = 0;
    bool m_suppressNextReconnect = false;
    int m_reconnectAttempt = 0;
    bool m_userClosed = false;
    bool m_sessionRevoked = false;
};
