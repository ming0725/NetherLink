/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_AI_CHAT_AI_CHAT_WEB_SOCKET
#define INCLUDE_VIEW_AI_CHAT_AI_CHAT_WEB_SOCKET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QNetworkAccessManager>
#include <QWebSocket>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatWebSocket : public QObject {
    Q_OBJECT

    public:
        explicit AiChatWebSocket(QObject*parent = nullptr);

        void connectToServer();

        void sendMessage(const QString& message, const QString& conversationId = QString());

    signals:
        void messageContent(const QString& content);

        void conversationStarted(const QString& conversationId);

        void messageEnded();

        void connectionEstablished();

        void connectionError(const QString& error);

        void requestError(const QString& errorMessage);

    private slots:
        void onConnected();

        void onDisconnected();

        void onTextMessageReceived(const QString& message);

        void onError(QAbstractSocket::SocketError error);

    private:
        QWebSocket m_webSocket;
        QString m_wsUrl;
        bool m_isProcessing = false;
        QNetworkAccessManager* m_networkManager = nullptr;
};

#endif /* INCLUDE_VIEW_AI_CHAT_AI_CHAT_WEB_SOCKET */
