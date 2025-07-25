/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Data/AvatarLoader.h"

#include <QDir>

#include <QNetworkReply>

#include <QPainter>

#include <QPainterPath>

#include <QStandardPaths>

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// AvatarLabel 实现
AvatarLabel::AvatarLabel(QWidget* parent) : QLabel(parent) {
    setMinimumSize(avatarSize, avatarSize);
    setMaximumSize(avatarSize, avatarSize);
    setScaledContents(true);

    // 在构造函数中建立持久的信号槽连接
    connect(&AvatarLoader::instance(), &AvatarLoader::avatarLoadStatusChanged, this, &AvatarLabel::handleAvatarLoadStatus);

    // 设置默认头像
    setPixmap(AvatarLoader::instance().getAvatar(""));
}

void AvatarLabel::handleAvatarLoadStatus(AvatarLoadEvent* event) {
    if (event->id != currentId) {
        return;
    }

    switch (event->status) {
        case AvatarLoadEvent::Status::Success:
            setPixmap(event->avatar);
            break;
        case AvatarLoadEvent::Status::Failed:
        case AvatarLoadEvent::Status::NoAvatar:
            setPixmap(AvatarLoader::instance().getAvatar(""));
            break;
        case AvatarLoadEvent::Status::Loading:
            // 可以在这里添加加载中的动画效果
            break;
    }
    update();  // 强制更新显示
}

void AvatarLabel::setAvatarSize(int size) {
    avatarSize = size;
    setMinimumSize(size, size);
    setMaximumSize(size, size);
    update();
}

void AvatarLabel::loadAvatar(const QString& id, const QUrl& url, bool isGroup) {
    // 保存当前ID
    currentId = id;

    // 先设置默认头像
    setPixmap(AvatarLoader::instance().getAvatar(""));
    update();  // 强制更新显示
    // 请求加载头像
    AvatarLoader::instance().loadAvatar(id, url, isGroup);
}

// AvatarLoader 实现
AvatarLoader::AvatarLoader(QObject* parent) : QObject(parent) {
    // 设置默认缓存配置
    setCacheConfig();

    // 创建默认头像
    defaultAvatar = QPixmap(DEFAULT_AVATAR_SIZE, DEFAULT_AVATAR_SIZE);
    defaultAvatar.fill(Qt::transparent);

    QPainter p(&defaultAvatar);

    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::lightGray);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, DEFAULT_AVATAR_SIZE, DEFAULT_AVATAR_SIZE);
}

AvatarLoader& AvatarLoader::instance() {
    static AvatarLoader loader;

    return (loader);
}

void AvatarLoader::setCacheConfig(int maxMemoryItems, int maxDiskItems) {
    this->maxMemoryItems = maxMemoryItems;
    this->maxDiskItems = maxDiskItems;
    cleanupCache();
}

void AvatarLoader::loadAvatar(const QString& id, const QUrl& url, bool isGroup) {
    // 检查内存缓存
    if (avatarCache.contains(id)) {
        auto& item = avatarCache[id];

        if (item.isValid) {
            item.lastAccess = QDateTime::currentDateTime();

            AvatarLoadEvent* event = new AvatarLoadEvent(id, url, AvatarLoadEvent::Status::Success);

            event->avatar = item.pixmap;

            emit avatarLoadStatusChanged(event);

            event->deleteLater();

            return;
        }
    }

    // 如果正在加载，直接返回
    if (loadingAvatars.contains(id)) {
        return;
    }

    // 发送加载中状态
    AvatarLoadEvent* loadingEvent = new AvatarLoadEvent(id, url);
    emit avatarLoadStatusChanged(loadingEvent);

    loadingEvent->deleteLater();

    // 检查磁盘缓存
    QPixmap diskAvatar = loadFromDisk(id, url, isGroup);

    if (!diskAvatar.isNull()) {
        // 添加到内存缓存
        AvatarCacheItem item;

        item.pixmap = diskAvatar;
        item.url = getAvatarPath(id, isGroup);
        item.lastAccess = QDateTime::currentDateTime();
        item.isValid = true;
        avatarCache[id] = item;
        cleanupCache();

        // 发送加载成功状态
        AvatarLoadEvent* successEvent = new AvatarLoadEvent(id, url, AvatarLoadEvent::Status::Success);

        successEvent->avatar = diskAvatar;

        emit avatarLoadStatusChanged(successEvent);

        successEvent->deleteLater();

        return;
    }

    // 从网络加载
    loadingAvatars.insert(id);
    downloadAvatar(id, url, isGroup);
}

QPixmap AvatarLoader::getAvatar(const QString& id) {
    if (avatarCache.contains(id)) {
        auto& item = avatarCache[id];

        if (item.isValid) {
            item.lastAccess = QDateTime::currentDateTime();

            return (item.pixmap);
        }
    }
    return (defaultAvatar);
}

void AvatarLoader::clearAvatar(const QString& id) {
    avatarCache.remove(id);

    // 删除磁盘文件
    QString userPath = getAvatarPath(id, false);
    QString groupPath = getAvatarPath(id, true);

    QFile::remove(userPath);
    QFile::remove(groupPath);
}

void AvatarLoader::clearAllAvatars() {
    avatarCache.clear();

    // 删除所有磁盘缓存
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars";
    QDir dir(cacheDir);

    dir.removeRecursively();
}

QString AvatarLoader::getAvatarPath(const QString& id, bool isGroup) const {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars";

    if (isGroup) {
        cacheDir += "/group";
    } else {
        cacheDir += "/user";
    }
    QDir().mkpath(cacheDir);

    return (cacheDir + QString("/%1.jpg").arg(id));
}

QPixmap AvatarLoader::loadFromDisk(const QString& id, const QUrl& /* url */, bool isGroup) {
    QString path = getAvatarPath(id, isGroup);
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        return (QPixmap());
    }

    QPixmap originalAvatar;

    if (originalAvatar.load(path)) {
        // 统一在这里处理圆形裁剪，确保所有来源的头像都经过相同的处理
        QPixmap circularAvatar = createCircularAvatar(originalAvatar, DEFAULT_AVATAR_SIZE);

        return (circularAvatar);
    }
    return (QPixmap());
}

void AvatarLoader::downloadAvatar(const QString& id, const QUrl& url, bool isGroup) {
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager.get(request);

    connect(reply, &QNetworkReply::finished, this, [=] () {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            processDownloadedAvatar(id, data, isGroup);
        } else {
            qWarning() << "Failed to download avatar for" << id << ":" << reply->errorString();
            loadingAvatars.remove(id);

            // 发送失败状态
            AvatarLoadEvent* event = new AvatarLoadEvent(id, url, AvatarLoadEvent::Status::Failed);
            emit avatarLoadStatusChanged(event);
            event->deleteLater();
        }
        reply->deleteLater();
    });
    connect(reply, &QNetworkReply::sslErrors, this, [=] (const QList <QSslError>& errors) {
        qWarning() << "SSL errors while downloading avatar for" << id << ":";

        for (const QSslError& error : errors) {
            qWarning() << error.errorString();
        }
        reply->ignoreSslErrors();
    });
}

void AvatarLoader::processDownloadedAvatar(const QString& id, const QByteArray& data, bool isGroup) {
    QPixmap originalAvatar;

    if (!originalAvatar.loadFromData(data)) {
        qWarning() << "Failed to load avatar data for" << id;
        loadingAvatars.remove(id);

        // 发送失败状态
        AvatarLoadEvent* event = new AvatarLoadEvent(id, QUrl(), AvatarLoadEvent::Status::Failed);
        emit avatarLoadStatusChanged(event);

        event->deleteLater();

        return;
    }

    // 保存原始图片到磁盘
    QString path = getAvatarPath(id, isGroup);

    if (saveToDisk(path, originalAvatar)) {
        // 从磁盘读取并处理（统一使用磁盘缓存的处理逻辑）
        QPixmap diskAvatar = loadFromDisk(id, QUrl(), isGroup);

        if (!diskAvatar.isNull()) {
            // 更新内存缓存
            AvatarCacheItem item;

            item.pixmap = diskAvatar;
            item.url = path;
            item.lastAccess = QDateTime::currentDateTime();
            item.isValid = true;
            avatarCache[id] = item;
            cleanupCache();

            // 发送加载成功事件
            AvatarLoadEvent* event = new AvatarLoadEvent(id, QUrl::fromLocalFile(path), AvatarLoadEvent::Status::Success);

            event->avatar = diskAvatar;

            emit avatarLoadStatusChanged(event);

            event->deleteLater();
        }
    } else {
        qWarning() << "Failed to save avatar to disk for" << id;

        AvatarLoadEvent* event = new AvatarLoadEvent(id, QUrl(), AvatarLoadEvent::Status::Failed);
        emit avatarLoadStatusChanged(event);

        event->deleteLater();
    }
    loadingAvatars.remove(id);
}

QPixmap AvatarLoader::createCircularAvatar(const QPixmap& source, int size) const {
    QPixmap result(size, size);

    result.fill(Qt::transparent);

    QPainter painter(&result);

    painter.setRenderHint(QPainter::Antialiasing);

    // 创建圆形裁剪路径
    QPainterPath path;

    path.addEllipse(0, 0, size, size);
    painter.setClipPath(path);

    // 缩放并绘制图片，保持宽高比
    QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 计算居中位置
    int x = (size - scaled.width()) / 2;
    int y = (size - scaled.height()) / 2;

    painter.drawPixmap(x, y, scaled);

    return (result); // 返回圆形裁切后的图片
}

bool AvatarLoader::saveToDisk(const QString& path, const QPixmap& pixmap) const {
    // 确保保存原始尺寸的图片到磁盘
    return (pixmap.save(path, "PNG", 100)); // 使用PNG格式以保持更好的质量
}

void AvatarLoader::cleanupCache() {
    // 清理内存缓存
    while (avatarCache.size() > maxMemoryItems) {
        QString oldestId;
        QDateTime oldestTime = QDateTime::currentDateTime();

        // 找到最旧的缓存项
        for (auto it = avatarCache.begin(); it != avatarCache.end(); ++it) {
            if (it.value().lastAccess < oldestTime) {
                oldestTime = it.value().lastAccess;
                oldestId = it.key();
            }
        }

        if (!oldestId.isEmpty()) {
            avatarCache.remove(oldestId);
        }
    }

    // 清理磁盘缓存
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars";
    QDir dir(cacheDir);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);

    while (files.size() > maxDiskItems) {
        QFile::remove(files.takeFirst().absoluteFilePath());
    }
}
