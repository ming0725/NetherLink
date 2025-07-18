#include "AiChatWebSocket.h"
#include "CurrentUser.h"
#include "NetworkConfig.h"
#include "NotificationManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUrlQuery>

AiChatWebSocket::AiChatWebSocket(QObject*parent) : QObject(parent), m_isProcessing(false), m_networkManager(new QNetworkAccessManager(this)) {
    QString serverIP = NetworkConfig::instance().getServerIP();
    QString httpPort = QString::number(NetworkConfig::instance().getHttpPort());

    m_wsUrl = QString("ws://%1:%2/ws/ai").arg(serverIP, httpPort);
    connect(&m_webSocket, &QWebSocket::connected, this, &AiChatWebSocket::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &AiChatWebSocket::onDisconnected);
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, &AiChatWebSocket::onError);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &AiChatWebSocket::onTextMessageReceived);
}

void AiChatWebSocket::connectToServer() {
    QUrl url(m_wsUrl);  // 直接使用WebSocket URL
    QNetworkRequest request(url);
    QString token = CurrentUser::instance().getToken();

    // 设置WebSocket升级所需的请求头
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    request.setRawHeader("Connection", "Upgrade");
    request.setRawHeader("Upgrade", "websocket");
    request.setRawHeader("Sec-WebSocket-Version", "13");

    // 直接打开WebSocket连接
    m_webSocket.open(request);
}

void AiChatWebSocket::sendMessage(const QString& message, const QString& conversationId) {
    if (m_isProcessing) {
        emit requestError(tr("当前正在处理其他请求，请稍后再试"));

        return;
    }

    QJsonObject jsonObj;

    jsonObj["conversation_id"] = conversationId;
    jsonObj["message"] = message;

    QJsonDocument doc(jsonObj);
    QString jsonMessage = doc.toJson(QJsonDocument::Compact);

    qDebug() << "发送消息:" << jsonMessage;
    m_webSocket.sendTextMessage(jsonMessage);
    m_isProcessing = true;
}

void AiChatWebSocket::onConnected() {
    qDebug() << "WebSocket连接成功";

    emit connectionEstablished();
}

void AiChatWebSocket::onDisconnected() {
    qDebug() << "WebSocket disconnected";
    m_isProcessing = false;
}

void AiChatWebSocket::onError(QAbstractSocket::SocketError error) {
    qDebug() << "WebSocket错误:" << error << m_webSocket.errorString();

    emit connectionError(m_webSocket.errorString());

    m_isProcessing = false;
}

void AiChatWebSocket::onTextMessageReceived(const QString& message) {
    qDebug() << "收到消息:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());

    if (doc.isNull()) {
        qDebug() << "解析JSON失败";

        return;
    }

    QJsonObject jsonObj = doc.object();
    QString type = jsonObj["type"].toString();

    if (type == "error") {
        m_isProcessing = false;

        emit requestError(jsonObj["content"].toString());
    } else if (type == "start") {
        QString conversationId = jsonObj["data"].toObject()["conversation_id"].toString();
        emit conversationStarted(conversationId);
    } else if (type == "message") {
        QString content = jsonObj["content"].toString();
        emit messageContent(content);
    } else if (type == "end") {
        m_isProcessing = false;

        emit messageEnded();
    }
}