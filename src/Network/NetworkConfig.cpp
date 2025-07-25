/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "Network/NetworkConfig.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

NetworkConfig::NetworkConfig(QObject* parent) : QObject(parent), m_serverIP("127.0.0.1") // 默认值
    , m_httpPort(8080) // 默认值
    , m_webSocketPort(8081) // 默认值
{
    loadConfig();
}

NetworkConfig& NetworkConfig::instance() {
    static NetworkConfig instance;

    return (instance);
}

bool NetworkConfig::loadConfig() {
    QFile configFile;

    // 首先尝试从可执行文件目录加载配置
    QString exeConfigPath = QCoreApplication::applicationDirPath() + "/network_config.json";

    if (QFile::exists(exeConfigPath)) {
        configFile.setFileName(exeConfigPath);
    } else {
        // 如果可执行文件目录没有配置文件，则从资源加载
        configFile.setFileName(":/network_config.json");
    }

    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开网络配置文件:" << configFile.fileName();

        return (false);
    }

    QByteArray configData = configFile.readAll();

    configFile.close();

    QJsonDocument doc = QJsonDocument::fromJson(configData);

    if (doc.isNull()) {
        qWarning() << "网络配置文件格式无效";

        return (false);
    }

    QJsonObject config = doc.object();
    QJsonObject serverConfig = config["server"].toObject();

    // 读取配置
    if (serverConfig.contains("ip")) {
        m_serverIP = serverConfig["ip"].toString();
    }

    if (serverConfig.contains("http_port")) {
        m_httpPort = serverConfig["http_port"].toInt();
    }

    if (serverConfig.contains("websocket_port")) {
        m_webSocketPort = serverConfig["websocket_port"].toInt();
    }
    return (true);
}

bool NetworkConfig::reloadConfig() {
    return (loadConfig());
}

QString NetworkConfig::getHttpAddress() const {
    return (QString("http://%1:%2").arg(m_serverIP).arg(m_httpPort));
}

QString NetworkConfig::getWebSocketAddress() const {
    return (QString("ws://%1:%2").arg(m_serverIP).arg(m_webSocketPort));
}
