#pragma once

#include <QCache>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QUrl;

class ImageService : public QObject {
    Q_OBJECT

public:
    enum class LoadState {
        Empty,
        Loading,
        Ready,
        Failed
    };
    Q_ENUM(LoadState)

    static ImageService& instance();

    LoadState loadState(const QString& source) const;
    QPixmap pixmap(const QString& source) const;
    QSize sourceSize(const QString& source) const;

    QPixmap scaled(const QString& source,
                   const QSize& targetSize,
                   Qt::AspectRatioMode aspectMode = Qt::KeepAspectRatio,
                   qreal devicePixelRatio = 1.0) const;

    QPixmap centerCrop(const QString& source,
                       const QSize& targetSize,
                       int radius = 0,
                       qreal devicePixelRatio = 1.0) const;

    QPixmap previewCrop(const QString& source,
                        const QSize& targetSize,
                        int radius = 0,
                        qreal devicePixelRatio = 1.0) const;

    void requestPreviewWarmup(const QString& source,
                              const QSize& targetSize,
                              qreal devicePixelRatio = 1.0);
    void requestOriginalWarmup(const QString& source);

    QPixmap circularAvatar(const QString& source,
                           int size,
                           qreal devicePixelRatio = 1.0) const;

    QPixmap circularAvatarPreview(const QString& source,
                                  int size,
                                  qreal devicePixelRatio = 1.0) const;

    void invalidateSource(const QString& source);

signals:
    void previewReady();
    void resourceChanged(const QString& source);

private:
    ImageService();
    ImageService(const ImageService&) = delete;
    ImageService& operator=(const ImageService&) = delete;

    QImage originalImage(const QString& source) const;
    void requestRemoteOriginalWarmup(const QString& source);
    void startRemoteOriginalRequest(const QString& source,
                                    const QUrl& url,
                                    bool attachAuthorization,
                                    int redirectCount);
    QPixmap transformed(const QString& key,
                        const QString& source,
                        const QSize& targetSize,
                        qreal devicePixelRatio,
                        Qt::AspectRatioMode aspectMode,
                        int radius,
                        bool circular) const;

    mutable QMutex m_mutex;
    mutable QCache<QString, QImage> m_originalCache;
    mutable QCache<QString, QImage> m_previewCache;
    mutable QSet<QString> m_pendingPreviewLoads;
    mutable QSet<QString> m_pendingOriginalLoads;
    mutable QHash<QString, QSize> m_sourceSizes;
    mutable QHash<QString, LoadState> m_loadStates;
    mutable QHash<QString, qint64> m_failedRetryAfterMs;
    QNetworkAccessManager* m_networkManager = nullptr;
};
