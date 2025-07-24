
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QNetworkAccessManager>
#include <QWebSocket>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NetworkManager : public QObject {
    Q_OBJECT

    public:
        static NetworkManager& instance();

        // HTTP
        void post(const QUrl& url, const QByteArray& payload);

        void get(const QUrl& url, const QByteArray& payload);

        // WSS
        void connectWss(const QUrl& wsUrl);

        void sendWsMessage(const QString& msg);

        void closeWss();

        // Contacts
        void reloadContacts();

    signals:
        // HTTP
        void httpFinished(const QByteArray& data);

        void httpError(const QString& errorString);

        // WSS
        void wssConnected();

        void wssDisconnected();

        void wssMessageReceived(const QString& message);

        void wssError(const QString& errorString);

        // Friend Request
        void friendRequestResponse(bool success, const QString& message);

        void friendRequestReceived(int requestId, const QString& fromUid, const QString& fromName, const QString& fromAvatar, const QString& message, const QString& createdAt);

        void friendRequestAccepted();

    private slots:
        void onHttpFinished(QNetworkReply* reply);

        void onWssConnected();

        void onWssTextMessage(const QString& msg);

        void onWssErrorOccurred(QAbstractSocket::SocketError error);

        void onSslErrors(const QList <QSslError>& errors);

    private:
        explicit NetworkManager(QObject* parent = nullptr);

        ~NetworkManager();

        QNetworkAccessManager* m_http;
        QWebSocket*            m_wss;
};
