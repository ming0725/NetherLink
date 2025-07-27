/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSslSocket>

#include "Data/AvatarLoader.h"
#include "Data/CurrentUser.h"
#include "Data/GroupRepository.h"
#include "Data/UserRepository.h"
#include "Network/NetworkConfig.h"
#include "Network/NetworkManager.h"
#include "View/Chat/MessageHandler.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

NetworkManager& NetworkManager::instance() {
    static NetworkManager mgr;

    return (mgr);
}

NetworkManager::NetworkManager(QObject* parent) : QObject(parent), m_http(new QNetworkAccessManager(this)), m_wss(new QWebSocket()) {
    // HTTP
    connect(m_http, &QNetworkAccessManager::finished, this, &NetworkManager::onHttpFinished, Qt::QueuedConnection);

    // WSS 信号（都在主线程）
    connect(m_wss, &QWebSocket::connected, this, &NetworkManager::onWssConnected, Qt::QueuedConnection);
    connect(m_wss, &QWebSocket::disconnected, this, &NetworkManager::wssDisconnected, Qt::QueuedConnection);
    connect(m_wss, &QWebSocket::textMessageReceived, this, &NetworkManager::onWssTextMessage, Qt::QueuedConnection);
    connect(m_wss, &QWebSocket::errorOccurred, this, &NetworkManager::onWssErrorOccurred, Qt::QueuedConnection);
    connect(m_wss, QOverload <const QList <QSslError>&>::of(&QWebSocket::sslErrors), this, &NetworkManager::onSslErrors, Qt::QueuedConnection);
}

NetworkManager::~NetworkManager() {
    m_wss->close();
}

// —— HTTP ——//
void NetworkManager::get(const QUrl& url, const QByteArray& /* payload */) {
    QNetworkRequest req(url);

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_http->get(req);
}

void NetworkManager::post(const QUrl& url, const QByteArray& payload) {
    QNetworkRequest req(url);

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_http->post(req, payload);
}

void NetworkManager::onHttpFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        emit httpFinished(reply->readAll());
    } else {
        emit httpError(reply->errorString());
    }
    reply->deleteLater();
}

// —— WSS ——//
void NetworkManager::connectWss(const QUrl& wsUrl) {
    // 忽略 SSL 验证（自签或测试环境）
    QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();

    cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_wss->setSslConfiguration(cfg);
    m_wss->open(wsUrl);
}

void NetworkManager::sendWsMessage(const QString& msg) {
    qDebug() << "WSS发送消息 >>" << msg;
    m_wss->sendTextMessage(msg);
}

void NetworkManager::closeWss() {
    m_wss->close();
}

void NetworkManager::onWssConnected() {
    emit wssConnected();
}

void NetworkManager::reloadContacts() {
    // 发送获取联系人信息的请求
    QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
    QNetworkRequest request(QUrl(baseHttpUrl + "/api/contacts"));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 添加token认证
    QString token = CurrentUser::instance().getToken();

    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    // 发送GET请求
    m_http->get(request);

    // 连接一次性的响应处理
    connect(m_http, &QNetworkAccessManager::finished, this, [this] (QNetworkReply* reply) {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "获取联系人信息失败:" << reply->errorString();

            return;
        }

        // 读取响应数据
        QByteArray data = reply->readAll();

        // 解析JSON响应
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);

        if ((err.error != QJsonParseError::NoError) || !doc.isObject()) {
            qWarning() << "联系人信息解析失败";

            return;
        }
        QJsonObject obj = doc.object();
        qDebug() << "获取到联系人数据，开始处理...";

        // 加载联系人信息和头像
        UserRepository::instance().loadUserAndContacts(obj["user"].toObject(), obj["friends"].toArray());
        QJsonArray friends = obj["friends"].toArray();

        for (const QJsonValue& friendVal : friends) {
            QJsonObject friendObj = friendVal.toObject();
            QString friendId = friendObj["uid"].toString();
            QString friendAvatarUrl = friendObj["avatar_url"].toString();

            if (!friendAvatarUrl.isEmpty()) {
                AvatarLoader::instance().loadAvatar(friendId, QUrl(friendAvatarUrl), false);
            }
        }

        // 加载群组信息、成员和头像
        QJsonArray groups = obj["groups"].toArray();
        GroupRepository::instance().loadGroupsAndMembers(groups);

        for (const QJsonValue& groupVal : groups) {
            QJsonObject groupObj = groupVal.toObject();
            QString groupId = QString::number(groupObj["gid"].toInt());
            QString groupAvatarUrl = groupObj["avatar_url"].toString();

            if (!groupAvatarUrl.isEmpty()) {
                AvatarLoader::instance().loadAvatar(groupId, QUrl(groupAvatarUrl), true);
            }

            // 加载群成员头像
            QJsonArray members = groupObj["members"].toArray();

            for (const QJsonValue& memberVal : members) {
                QJsonObject memberObj = memberVal.toObject();
                QString memberId = memberObj["uid"].toString();
                QString memberAvatarUrl = memberObj["avatar_url"].toString();

                if (!memberAvatarUrl.isEmpty()) {
                    AvatarLoader::instance().loadAvatar(memberId, QUrl(memberAvatarUrl), false);
                }
            }
        }

        // 数据更新完成后，发出信号通知UI更新
        emit friendRequestAccepted();
    }, Qt::SingleShotConnection);
}

void NetworkManager::onWssTextMessage(const QString& msg) {
    qDebug() << "WSS接收消息 <<" << msg;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);

    if ((err.error != QJsonParseError::NoError) || !doc.isObject()) {
        qWarning() << "WebSocket消息解析失败:" << err.errorString();

        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "ping") {
        // 收到ping消息，回应pong
        QJsonObject pongMsg;

        pongMsg["type"] = "pong";

        QString jsonString = QJsonDocument(pongMsg).toJson(QJsonDocument::Compact);

        qDebug() << "WSS发送消息 >>" << jsonString;
        m_wss->sendTextMessage(jsonString);

        return;
    } else if (type == "chat") {
        // 使用MessageHandler处理聊天消息
        MessageHandler::instance().handleReceivedMessage(obj);
    } else if (type == "friend_request_response") {
        // 处理好友请求响应
        QJsonObject payload = obj["payload"].toObject();
        bool success = payload["success"].toBool();
        QString message = payload["message"].toString();
        emit friendRequestResponse(success, message);

        // 如果是同意好友请求的响应，也重新加载联系人
        if (success && message.contains("已接受")) {
            reloadContacts();
        }
    } else if (type == "friend_request_result") {
        QJsonObject payload = obj["payload"].toObject();
        QString action = payload["action"].toString();

        if (action == "accept") {
            reloadContacts();
        }
    } else if (type == "friend_request_received") {
        // 处理收到的好友请求
        QJsonObject payload = obj["payload"].toObject();
        int requestId = payload["request_id"].toInt();
        QString fromUid = payload["from_uid"].toString();
        QString fromName = payload["from_name"].toString();
        QString fromAvatar = payload["from_avatar"].toString();
        QString message = payload["message"].toString();
        QString createdAt = payload["created_at"].toString();
        QDateTime dateTime = QDateTime::fromString(createdAt, Qt::ISODate);
        QString dateOnly = dateTime.date().toString("yyyy-MM-dd");
        emit friendRequestReceived(requestId, fromUid, fromName, fromAvatar, message, dateOnly);
    } else if (type == "error") {
        // 处理错误消息
        QJsonObject payload = obj["payload"].toObject();
        QString message = payload["message"].toString();
        emit friendRequestResponse(false, message);  // 发送失败响应
    }
}

void NetworkManager::onWssErrorOccurred(QAbstractSocket::SocketError) {
    emit wssError(m_wss->errorString());
}

void NetworkManager::onSslErrors(const QList <QSslError>& errors) {
    Q_UNUSED(errors);
    m_wss->ignoreSslErrors();  // 完全忽略
}
