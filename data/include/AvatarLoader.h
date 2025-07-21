
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QNetworkAccessManager>

#include <QLabel>

/* struct ----------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// 头像缓存项
struct AvatarCacheItem {
    QPixmap pixmap;
    QString url;
    QDateTime lastAccess;
    bool isValid;
};

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

// 头像加载事件
class AvatarLoadEvent : public QObject {
    Q_OBJECT

    public:
        enum class Status {
            Loading, // 正在加载
            Success, // 加载成功
            Failed, // 加载失败
            NoAvatar // 无头像（使用默认头像）
        };

        explicit AvatarLoadEvent(const QString& id, const QUrl& url, Status status = Status::Loading) : id(id), url(url), status(status) {}

        QString id;  // 用户ID或群组ID
        QUrl url; // 头像URL
        Status status; // 加载状态
        QPixmap avatar; // 加载的头像
};

// 头像标签组件
class AvatarLabel : public QLabel {
    Q_OBJECT

    public:
        explicit AvatarLabel(QWidget* parent = nullptr);

        void setAvatarSize(int size);

        void loadAvatar(const QString& id, const QUrl& url, bool isGroup = false);

    private slots:
        void handleAvatarLoadStatus(AvatarLoadEvent* event);

    private:
        int avatarSize = 48;
        QString currentId;
        bool isCircular = true;
};

// 头像加载服务
class AvatarLoader : public QObject {
    Q_OBJECT

    public:
        static AvatarLoader& instance();

        // 异步加载头像
        void loadAvatar(const QString& id, const QUrl& url, bool isGroup = false);

        // 获取头像（如果有）
        QPixmap getAvatar(const QString& id);

        // 清除指定ID的头像
        void clearAvatar(const QString& id);

        // 清除所有头像
        void clearAllAvatars();

        // 设置缓存配置
        void setCacheConfig(int maxMemoryItems = 100, int maxDiskItems = 500);

    signals:
        // 头像加载状态变化信号
        void avatarLoadStatusChanged(AvatarLoadEvent* event);

    private:
        explicit AvatarLoader(QObject* parent = nullptr);

        Q_DISABLE_COPY(AvatarLoader)

        // 内部方法
        QString getAvatarPath(const QString& id, bool isGroup) const;

        QPixmap loadFromDisk(const QString& id, const QUrl& url, bool isGroup);

        void downloadAvatar(const QString& id, const QUrl& url, bool isGroup);

        void processDownloadedAvatar(const QString& id, const QByteArray& data, bool isGroup);

        QPixmap createCircularAvatar(const QPixmap& source, int size) const;

        bool saveToDisk(const QString& path, const QPixmap& pixmap) const;

        void cleanupCache();

    private:
        QNetworkAccessManager networkManager;
        QMap <QString, AvatarCacheItem> avatarCache; // 内存缓存
        QSet <QString> loadingAvatars; // 正在加载的头像ID集合
        QPixmap defaultAvatar; // 默认头像
        int maxMemoryItems; // 最大内存缓存项数
        int maxDiskItems; // 最大磁盘缓存项数
        static const int DEFAULT_AVATAR_SIZE = 48; // 默认头像尺寸
};
