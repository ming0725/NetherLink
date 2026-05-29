#include "PostOverlay.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/effects/FastGaussianBlur.h"

#include <QFutureWatcher>
#include <QPainter>
#include <QPixmap>
#include <QtConcurrent/QtConcurrentRun>

namespace {

constexpr qreal kPostOverlayBlurRadius = 12.0;
constexpr qreal kPostOverlayBlurRenderScale = 0.58;

QImage renderBlurredOverlaySnapshot(QImage scaledSource, qreal blurRadius, qreal devicePixelRatio)
{
    if (scaledSource.isNull()) {
        return {};
    }

    FastGaussianBlur blur;
    QImage blurred = blur.blur(scaledSource, blurRadius);
    blurred.setDevicePixelRatio(devicePixelRatio);
    return blurred;
}

} // namespace

PostOverlay::PostOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void PostOverlay::captureBlurredBackground(QWidget* sourceWidget)
{
    QWidget* source = sourceWidget ? sourceWidget : parentWidget();
    if (!source || !source->size().isValid()) {
        clearBlurredBackground();
        return;
    }

    ++m_blurGeneration;
    const int generation = m_blurGeneration;
    m_snapshot = {};
    m_blurredSnapshot = {};

    const QPixmap snapshot = source->grab();
    QImage sourceImage = snapshot.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (sourceImage.isNull()) {
        update();
        return;
    }

    const qreal devicePixelRatio = qMax<qreal>(1.0, snapshot.devicePixelRatio());
    QColor baseColor = ThemeManager::instance().color(ThemeColor::WindowBackground);
    baseColor.setAlpha(255);

    QImage opaqueSource(sourceImage.size(), QImage::Format_ARGB32_Premultiplied);
    opaqueSource.fill(baseColor);
    {
        QPainter sourcePainter(&opaqueSource);
        sourcePainter.drawImage(opaqueSource.rect(), sourceImage, sourceImage.rect());
    }
    opaqueSource.setDevicePixelRatio(devicePixelRatio);
    m_snapshot = opaqueSource;

    const qreal scale = qBound<qreal>(0.2, kPostOverlayBlurRenderScale, 1.0);
    const QSize scaledSize(qMax(1, qRound(opaqueSource.width() * scale)),
                           qMax(1, qRound(opaqueSource.height() * scale)));
    QImage scaledSource = opaqueSource.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaledSource.setDevicePixelRatio(devicePixelRatio * scale);

    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, generation]() {
        if (generation == m_blurGeneration) {
            m_blurredSnapshot = watcher->result();
            update();
        }
        watcher->deleteLater();
    });

    const qreal blurRadius = kPostOverlayBlurRadius * scale * devicePixelRatio;
    const qreal blurredDevicePixelRatio = devicePixelRatio * scale;
    watcher->setFuture(QtConcurrent::run(renderBlurredOverlaySnapshot,
                                         scaledSource,
                                         blurRadius,
                                         blurredDevicePixelRatio));
    update();
}

void PostOverlay::clearBlurredBackground()
{
    ++m_blurGeneration;
    m_snapshot = {};
    m_blurredSnapshot = {};
    update();
}

void PostOverlay::setOverlayOpacity(qreal opacity)
{
    const qreal bounded = qBound(0.0, opacity, 1.0);
    if (!qFuzzyCompare(m_opacity, bounded)) {
        m_opacity = bounded;
        update();
    }
}

void PostOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_opacity > 0.0) {
        const QImage& background = !m_blurredSnapshot.isNull() ? m_blurredSnapshot : m_snapshot;
        if (!background.isNull()) {
            painter.save();
            painter.setOpacity(m_opacity);
            painter.drawImage(rect(), background);
            painter.restore();
        }
    }

    QColor color = ThemeManager::instance().color(ThemeColor::PostOverlay);
    color.setAlphaF(color.alphaF() * m_opacity);
    painter.fillRect(rect(), color);
}
