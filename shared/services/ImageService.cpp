#include "ImageService.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QUrl>

#include "AvatarSource.h"
#include "shared/data/LocalDataStore.h"
#include "shared/network/AuthSession.h"
#include "shared/network/HttpClient.h"

namespace {

int imageCostKb(const QImage& image)
{
    return qMax(1, (image.bytesPerLine() * image.height()) / 1024);
}

QString variantKey(const char* mode,
                   const QString& source,
                   const QSize& size,
                   qreal dpr,
                   Qt::AspectRatioMode aspectMode,
                   int radius)
{
    return QString("imgsvc|%1|%2|%3x%4|%5|%6|%7")
            .arg(QString::fromLatin1(mode),
                 source,
                 QString::number(size.width()),
                 QString::number(size.height()),
                 QString::number(dpr, 'f', 2),
                 QString::number(static_cast<int>(aspectMode)),
                 QString::number(radius));
}

QPixmap renderPixmap(const QImage& source,
                     const QSize& logicalSize,
                     qreal dpr,
                     Qt::AspectRatioMode aspectMode,
                     int radius,
                     bool circular)
{
    if (source.isNull() || !logicalSize.isValid()) {
        return {};
    }

    const QSize pixelSize = QSize(qMax(1, qRound(logicalSize.width() * dpr)),
                                  qMax(1, qRound(logicalSize.height() * dpr)));

    QImage canvas(pixelSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);

    QPainterPath clipPath;
    if (circular) {
        clipPath.addEllipse(QRect(QPoint(0, 0), pixelSize));
    } else if (radius > 0) {
        clipPath.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(pixelSize)),
                                radius * dpr,
                                radius * dpr);
    }
    if (!clipPath.isEmpty()) {
        painter.setClipPath(clipPath);
    }

    const QImage scaled = source.scaled(pixelSize,
                                        aspectMode,
                                        Qt::SmoothTransformation);

    QRect sourceRect(QPoint(0, 0), scaled.size());
    if (aspectMode == Qt::KeepAspectRatioByExpanding) {
        sourceRect.moveLeft(qMax(0, (scaled.width() - pixelSize.width()) / 2));
        sourceRect.moveTop(qMax(0, (scaled.height() - pixelSize.height()) / 2));
        sourceRect.setSize(pixelSize);
    }

    painter.drawImage(QRect(QPoint(0, 0), pixelSize), scaled, sourceRect);

    QPixmap pixmap = QPixmap::fromImage(canvas);
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

QImage readScaledPreview(const QString& source, const QSize& logicalSize, qreal dpr)
{
    if (source.isEmpty() || !logicalSize.isValid()) {
        return {};
    }

    QImageReader reader(AvatarSource::cleanForIo(source));
    reader.setAutoTransform(true);

    const QSize sourceSize = reader.size();
    const QSize targetPixels(qMax(1, qRound(logicalSize.width() * dpr)),
                             qMax(1, qRound(logicalSize.height() * dpr)));
    if (sourceSize.isValid() && sourceSize.width() > 0 && sourceSize.height() > 0) {
        const QSize decodeSize = sourceSize.scaled(targetPixels, Qt::KeepAspectRatioByExpanding);
        if (decodeSize.isValid()) {
            reader.setScaledSize(decodeSize);
        }
    }

    return reader.read();
}

QString previewSourceKey(const QString& source, const QSize& targetSize, qreal dpr)
{
    return QString("imgsvc-src|%1|%2x%3|%4")
            .arg(source,
                 QString::number(targetSize.width()),
                 QString::number(targetSize.height()),
                 QString::number(dpr, 'f', 2));
}

QString ioSource(const QString& source)
{
    return AvatarSource::cleanForIo(source);
}

bool isExistingLocalFile(const QString& source)
{
    const QString cleanSource = ioSource(source);
    return cleanSource.startsWith(QLatin1Char('/')) && QFileInfo::exists(cleanSource);
}

bool isApiRelativeSource(const QString& source)
{
    return source.startsWith(QLatin1Char('/')) && !isExistingLocalFile(source);
}

bool isRemoteSource(const QString& source)
{
    if (isApiRelativeSource(source)) {
        return true;
    }

    const QUrl url(ioSource(source));
    return url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https");
}

QUrl resolvedRemoteUrl(const QString& source)
{
    const QString cleanSource = ioSource(source);
    if (isApiRelativeSource(cleanSource)) {
        const BackendEnvironment environment = HttpClient::instance().environment();
        const QString prefix = environment.apiPrefix.startsWith(QLatin1Char('/'))
                ? environment.apiPrefix
                : QStringLiteral("/") + environment.apiPrefix;
        const QUrl relative(cleanSource);
        QString path = relative.path();
        if (!path.startsWith(prefix + QLatin1Char('/')) && path != prefix) {
            path = prefix + (path.startsWith(QLatin1Char('/')) ? path : QStringLiteral("/") + path);
        }

        QUrl url(environment.baseUrl);
        url.setPath(path);
        url.setQuery(relative.query());
        return url;
    }
    return QUrl(cleanSource);
}

bool shouldAttachAuthorization(const QString& source, const QUrl& url)
{
    const QUrl baseUrl = HttpClient::instance().environment().baseUrl;
    const bool sameBackendOrigin = url.scheme() == baseUrl.scheme()
            && url.host() == baseUrl.host()
            && url.port() == baseUrl.port();
    return sameBackendOrigin && (isApiRelativeSource(source) || !source.isEmpty());
}

QImage imageFromBytes(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return {};
    }

    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    return reader.read();
}

QString remoteDiskCachePath(const QString& source)
{
    if (source.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString dirPath = LocalDataStore::instance().dataRootPath()
                            + QStringLiteral("/cache/images");
    QDir().mkpath(dirPath);
    return QDir(dirPath).filePath(QString::fromLatin1(digest) + QStringLiteral(".img"));
}

QString remoteDiskCacheEtagPath(const QString& source)
{
    const QString imagePath = remoteDiskCachePath(source);
    return imagePath.isEmpty() ? QString() : imagePath + QStringLiteral(".etag");
}

QString readRemoteDiskCacheEtag(const QString& source)
{
    const QString path = remoteDiskCacheEtagPath(source);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

void writeRemoteDiskCacheEtag(const QString& source, const QString& etag)
{
    if (source.isEmpty()) {
        return;
    }

    const QString path = remoteDiskCacheEtagPath(source);
    if (path.isEmpty()) {
        return;
    }
    if (etag.isEmpty()) {
        QFile::remove(path);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(etag.toUtf8());
}

QImage readRemoteDiskCache(const QString& source)
{
    const QString path = remoteDiskCachePath(source);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QFile::remove(path);
    }
    return image;
}

void writeRemoteDiskCache(const QString& source, const QByteArray& body)
{
    if (body.isEmpty()) {
        return;
    }

    const QString path = remoteDiskCachePath(source);
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(body);
}

} // namespace

ImageService& ImageService::instance()
{
    static ImageService service;
    return service;
}

ImageService::ImageService()
    : QObject(nullptr)
    , m_originalCache(64 * 1024)
    , m_previewCache(32 * 1024)
    , m_networkManager(new QNetworkAccessManager(this))
{
    if (QPixmapCache::cacheLimit() < 65536) {
        QPixmapCache::setCacheLimit(65536);
    }
}

ImageService::LoadState ImageService::loadState(const QString& source) const
{
    if (source.isEmpty()) {
        return LoadState::Empty;
    }

    QMutexLocker locker(&m_mutex);
    if (m_originalCache.object(source)) {
        return LoadState::Ready;
    }
    return m_loadStates.value(source, LoadState::Empty);
}

QImage ImageService::originalImage(const QString& source) const
{
    if (source.isEmpty()) {
        return {};
    }

    {
        QMutexLocker locker(&m_mutex);
        if (QImage* cached = m_originalCache.object(source)) {
            return *cached;
        }
    }

    QImage image;
    if (isRemoteSource(source)) {
        image = readRemoteDiskCache(source);
    } else {
        QImageReader reader(ioSource(source));
        reader.setAutoTransform(true);
        image = reader.read();
    }
    if (image.isNull()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    m_originalCache.insert(source, new QImage(image), imageCostKb(image));
    m_sourceSizes.insert(source, image.size());
    m_loadStates.insert(source, LoadState::Ready);
    return image;
}

QPixmap ImageService::pixmap(const QString& source) const
{
    if (source.isEmpty()) {
        return {};
    }

    const QString key = QStringLiteral("imgsvc|pixmap|%1").arg(source);
    QPixmap cachedPixmap;
    if (QPixmapCache::find(key, &cachedPixmap)) {
        return cachedPixmap;
    }

    const QImage image = originalImage(source);
    if (image.isNull()) {
        const_cast<ImageService*>(this)->requestOriginalWarmup(source);
        return {};
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    if (!pixmap.isNull()) {
        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}

QSize ImageService::sourceSize(const QString& source) const
{
    if (source.isEmpty()) {
        return {};
    }

    {
        QMutexLocker locker(&m_mutex);
        if (const QImage* cached = m_originalCache.object(source)) {
            return cached->size();
        }
        const QSize cachedSize = m_sourceSizes.value(source);
        if (cachedSize.isValid()) {
            return cachedSize;
        }
    }

    if (isRemoteSource(source)) {
        const_cast<ImageService*>(this)->requestOriginalWarmup(source);
        return {};
    }

    QImageReader reader(ioSource(source));
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) {
        QMutexLocker locker(&m_mutex);
        m_sourceSizes.insert(source, size);
    }
    return size;
}

QPixmap ImageService::transformed(const QString& key,
                                  const QString& source,
                                  const QSize& targetSize,
                                  qreal devicePixelRatio,
                                  Qt::AspectRatioMode aspectMode,
                                  int radius,
                                  bool circular) const
{
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }

    const QImage image = originalImage(source);
    if (image.isNull()) {
        const_cast<ImageService*>(this)->requestOriginalWarmup(source);
        return {};
    }

    pixmap = renderPixmap(image,
                          targetSize,
                          devicePixelRatio,
                          aspectMode,
                          radius,
                          circular);
    if (!pixmap.isNull()) {
        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}

QPixmap ImageService::scaled(const QString& source,
                             const QSize& targetSize,
                             Qt::AspectRatioMode aspectMode,
                             qreal devicePixelRatio) const
{
    if (targetSize.isEmpty()) {
        return {};
    }

    return transformed(variantKey("scaled",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  aspectMode,
                                  0),
                       source,
                       targetSize,
                       devicePixelRatio,
                       aspectMode,
                       0,
                       false);
}

QPixmap ImageService::centerCrop(const QString& source,
                                 const QSize& targetSize,
                                 int radius,
                                 qreal devicePixelRatio) const
{
    if (targetSize.isEmpty()) {
        return {};
    }

    return transformed(variantKey("crop",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  Qt::KeepAspectRatioByExpanding,
                                  radius),
                       source,
                       targetSize,
                       devicePixelRatio,
                       Qt::KeepAspectRatioByExpanding,
                       radius,
                       false);
}

QPixmap ImageService::previewCrop(const QString& source,
                                  const QSize& targetSize,
                                  int radius,
                                  qreal devicePixelRatio) const
{
    if (targetSize.isEmpty()) {
        return {};
    }

    const QString key = variantKey("preview",
                                   source,
                                   targetSize,
                                   devicePixelRatio,
                                   Qt::KeepAspectRatioByExpanding,
                                   radius);
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }

    const QString sourceKey = previewSourceKey(source, targetSize, devicePixelRatio);
    QImage image;
    {
        QMutexLocker locker(&m_mutex);
        if (QImage* cached = m_previewCache.object(sourceKey)) {
            image = *cached;
        }
    }
    if (image.isNull()) {
        const_cast<ImageService*>(this)->requestPreviewWarmup(source, targetSize, devicePixelRatio);
        return centerCrop(source, targetSize, radius, devicePixelRatio);
    }

    pixmap = renderPixmap(image,
                          targetSize,
                          devicePixelRatio,
                          Qt::KeepAspectRatioByExpanding,
                          radius,
                          false);
    if (!pixmap.isNull()) {
        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}

void ImageService::requestPreviewWarmup(const QString& source,
                                        const QSize& targetSize,
                                        qreal devicePixelRatio)
{
    if (source.isEmpty() || targetSize.isEmpty()) {
        return;
    }

    if (isRemoteSource(source)) {
        requestOriginalWarmup(source);
        return;
    }

    const QString key = previewSourceKey(source, targetSize, devicePixelRatio);
    {
        QMutexLocker locker(&m_mutex);
        if (m_previewCache.object(key) || m_pendingPreviewLoads.contains(key)) {
            return;
        }
        m_pendingPreviewLoads.insert(key);
    }

    QThreadPool::globalInstance()->start(QRunnable::create([this, key, source, targetSize, devicePixelRatio]() {
        const QImage image = readScaledPreview(source, targetSize, devicePixelRatio);
        QMetaObject::invokeMethod(this, [this, key, source, targetSize, image]() {
            {
                QMutexLocker locker(&m_mutex);
                m_pendingPreviewLoads.remove(key);
                if (!image.isNull()) {
                    m_previewCache.insert(key, new QImage(image), imageCostKb(image));
                }
            }
            emit previewReady();
        }, Qt::QueuedConnection);
    }));
}

void ImageService::requestOriginalWarmup(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }

    if (isRemoteSource(source)) {
        requestRemoteOriginalWarmup(source);
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_originalCache.object(source) || m_pendingOriginalLoads.contains(source)) {
            return;
        }
        m_pendingOriginalLoads.insert(source);
    }

    QThreadPool::globalInstance()->start(QRunnable::create([this, source]() {
        const QImage image = originalImage(source);
        QMetaObject::invokeMethod(this, [this, source, image]() {
            Q_UNUSED(image);
            QMutexLocker locker(&m_mutex);
            m_pendingOriginalLoads.remove(source);
        }, Qt::QueuedConnection);
    }));
}

void ImageService::requestRemoteOriginalWarmup(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, source]() {
            requestRemoteOriginalWarmup(source);
        }, Qt::QueuedConnection);
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_originalCache.object(source) || m_pendingOriginalLoads.contains(source)) {
            return;
        }
        if (m_loadStates.value(source) == LoadState::Failed) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 retryAfter = m_failedRetryAfterMs.value(source);
            if (retryAfter > now) {
                return;
            }
        }
        m_pendingOriginalLoads.insert(source);
        m_loadStates.insert(source, LoadState::Loading);
    }

    const QUrl url = resolvedRemoteUrl(source);
    if (!url.isValid() || url.scheme().isEmpty()) {
        {
            QMutexLocker locker(&m_mutex);
            m_pendingOriginalLoads.remove(source);
            m_loadStates.insert(source, LoadState::Failed);
            m_failedRetryAfterMs.insert(source, QDateTime::currentMSecsSinceEpoch() + 30000);
        }
        emit previewReady();
        emit resourceChanged(source);
        return;
    }

    startRemoteOriginalRequest(source,
                               url,
                               shouldAttachAuthorization(source, url) && AuthSession::instance().hasAccessToken(),
                               0);
}

void ImageService::startRemoteOriginalRequest(const QString& source,
                                              const QUrl& url,
                                              bool attachAuthorization,
                                              int redirectCount)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "image/*,*/*;q=0.8");
    const QString etag = readRemoteDiskCacheEtag(source);
    if (!etag.isEmpty() && QFileInfo::exists(remoteDiskCachePath(source))) {
        request.setRawHeader("If-None-Match", etag.toUtf8());
    }
    if (attachAuthorization) {
        request.setRawHeader("Authorization",
                             "Bearer " + AuthSession::instance().accessToken().toUtf8());
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, source, url, redirectCount, reply]() {
        const QByteArray body = reply->readAll();
        const bool networkOk = reply->error() == QNetworkReply::NoError;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (status >= 300 && status < 400 && redirectTarget.isValid()) {
            const QUrl nextUrl = url.resolved(redirectTarget);
            reply->deleteLater();
            if (redirectCount >= 5) {
                {
                    QMutexLocker locker(&m_mutex);
                    m_pendingOriginalLoads.remove(source);
                    m_loadStates.insert(source, LoadState::Failed);
                    m_failedRetryAfterMs.insert(source, QDateTime::currentMSecsSinceEpoch() + 30000);
                }
                emit previewReady();
                emit resourceChanged(source);
                return;
            }

            startRemoteOriginalRequest(source,
                                       nextUrl,
                                       shouldAttachAuthorization(source, nextUrl) && AuthSession::instance().hasAccessToken(),
                                       redirectCount + 1);
            return;
        }

        if (status == 304) {
            const QImage image = readRemoteDiskCache(source);
            {
                QMutexLocker locker(&m_mutex);
                m_pendingOriginalLoads.remove(source);
                if (!image.isNull()) {
                    m_originalCache.insert(source, new QImage(image), imageCostKb(image));
                    m_sourceSizes.insert(source, image.size());
                    m_loadStates.insert(source, LoadState::Ready);
                    m_failedRetryAfterMs.remove(source);
                } else {
                    m_loadStates.insert(source, LoadState::Failed);
                    m_failedRetryAfterMs.insert(source, QDateTime::currentMSecsSinceEpoch() + 30000);
                }
            }
            reply->deleteLater();
            emit previewReady();
            emit resourceChanged(source);
            return;
        }

        const QImage image = networkOk ? imageFromBytes(body) : QImage();
        {
            QMutexLocker locker(&m_mutex);
            m_pendingOriginalLoads.remove(source);
            if (!image.isNull()) {
                writeRemoteDiskCache(source, body);
                writeRemoteDiskCacheEtag(source, QString::fromUtf8(reply->rawHeader("ETag")));
                m_originalCache.insert(source, new QImage(image), imageCostKb(image));
                m_sourceSizes.insert(source, image.size());
                m_loadStates.insert(source, LoadState::Ready);
                m_failedRetryAfterMs.remove(source);
            } else {
                m_loadStates.insert(source, LoadState::Failed);
                m_failedRetryAfterMs.insert(source, QDateTime::currentMSecsSinceEpoch() + 30000);
            }
        }
        reply->deleteLater();
        emit previewReady();
        emit resourceChanged(source);
    });
}

QPixmap ImageService::circularAvatar(const QString& source,
                                     int size,
                                     qreal devicePixelRatio) const
{
    if (size <= 0) {
        return {};
    }

    const QSize targetSize(size, size);
    return transformed(variantKey("avatar",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  Qt::KeepAspectRatioByExpanding,
                                  size / 2),
                       source,
                       targetSize,
                       devicePixelRatio,
                       Qt::KeepAspectRatioByExpanding,
                       size / 2,
                       true);
}

QPixmap ImageService::circularAvatarPreview(const QString& source,
                                            int size,
                                            qreal devicePixelRatio) const
{
    if (size <= 0) {
        return {};
    }

    if (isRemoteSource(source)) {
        return circularAvatar(source, size, devicePixelRatio);
    }

    const QSize targetSize(size, size);
    const QString key = variantKey("avatar-preview",
                                   source,
                                   targetSize,
                                   devicePixelRatio,
                                   Qt::KeepAspectRatioByExpanding,
                                   size / 2);
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }

    const QString sourceKey = previewSourceKey(source, targetSize, devicePixelRatio);
    QImage image;
    {
        QMutexLocker locker(&m_mutex);
        if (QImage* cached = m_previewCache.object(sourceKey)) {
            image = *cached;
        }
    }
    if (image.isNull()) {
        const_cast<ImageService*>(this)->requestPreviewWarmup(source, targetSize, devicePixelRatio);
        return circularAvatar(source, size, devicePixelRatio);
    }

    pixmap = renderPixmap(image,
                          targetSize,
                          devicePixelRatio,
                          Qt::KeepAspectRatioByExpanding,
                          size / 2,
                          true);
    if (!pixmap.isNull()) {
        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}

void ImageService::invalidateSource(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_originalCache.remove(source);
        m_previewCache.clear();
        m_sourceSizes.remove(source);
        m_loadStates.remove(source);
        m_failedRetryAfterMs.remove(source);
        QPixmapCache::clear();
        const auto pendingKeys = m_pendingPreviewLoads.values();
        for (const QString& key : pendingKeys) {
            if (key.contains(source)) {
                m_pendingPreviewLoads.remove(key);
            }
        }
        m_pendingOriginalLoads.remove(source);
    }

    QFile::remove(remoteDiskCachePath(source));
    QFile::remove(remoteDiskCacheEtagPath(source));
    emit previewReady();
    emit resourceChanged(source);
}
