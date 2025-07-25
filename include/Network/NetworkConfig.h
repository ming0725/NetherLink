/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_NETWORK_NETWORK_CONFIG
#define INCLUDE_NETWORK_NETWORK_CONFIG

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QObject>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NetworkConfig : public QObject {
    Q_OBJECT

    public:
        static NetworkConfig& instance();

        // 获取服务器配置
        QString getServerIP() const {
            return (m_serverIP);
        }

        int getHttpPort() const {
            return (m_httpPort);
        }

        int getWebSocketPort() const {
            return (m_webSocketPort);
        }

        // 获取完整的服务器地址
        QString getHttpAddress() const;

        QString getWebSocketAddress() const;

        // 重新加载配置
        bool reloadConfig();

    private:
        explicit NetworkConfig(QObject* parent = nullptr);

        ~NetworkConfig() = default;

        NetworkConfig(const NetworkConfig&) = delete;

        NetworkConfig& operator=(const NetworkConfig&) = delete;

        // 加载配置文件
        bool loadConfig();

        // 配置数据
        QString m_serverIP;
        int m_httpPort;
        int m_webSocketPort;
};

#endif /* INCLUDE_NETWORK_NETWORK_CONFIG */
