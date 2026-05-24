#include "app/frame/current_user/AvatarCropCanvas.h"

#include "shared/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr int kAvatarCropPreviewSize = 320;
constexpr int kAvatarCropOutputSize = 512;

} // namespace

AvatarCropCanvas::AvatarCropCanvas(const QImage& image, QWidget* parent)
    : QWidget(parent)
    , m_image(image)
{
    setFixedSize(kAvatarCropPreviewSize, kAvatarCropPreviewSize);
    setMouseTracking(true);
    setCursor(Qt::SizeAllCursor);
    resetTransform();
}

QImage AvatarCropCanvas::croppedImage() const
{
    if (m_image.isNull()) {
        return {};
    }

    QImage output(QSize(kAvatarCropOutputSize, kAvatarCropOutputSize),
                  QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    const QRectF sourceRect(-m_offset.x() / m_scale,
                            -m_offset.y() / m_scale,
                            width() / m_scale,
                            height() / m_scale);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(QPointF(0, 0), QSizeF(output.size())),
                      m_image,
                      sourceRect);
    return output;
}

int AvatarCropCanvas::zoomValue() const
{
    if (qFuzzyCompare(m_minScale, m_maxScale)) {
        return 0;
    }
    const qreal normalized = std::log(m_scale / m_minScale) / std::log(m_maxScale / m_minScale);
    return qBound(0, qRound(normalized * 1000.0), 1000);
}

void AvatarCropCanvas::setZoomValue(int value)
{
    const qreal normalized = qBound(0.0, value / 1000.0, 1.0);
    setScale(m_minScale * std::pow(m_maxScale / m_minScale, normalized),
             QRectF(rect()).center());
}

void AvatarCropCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

    if (!m_image.isNull()) {
        painter.drawImage(QRectF(m_offset,
                                 QSizeF(m_image.width() * m_scale,
                                        m_image.height() * m_scale)),
                          m_image);
    }

    QPainterPath outer;
    outer.addRect(rect());
    QPainterPath cropCircle;
    cropCircle.addEllipse(QRectF(rect()));
    painter.fillPath(outer.subtracted(cropCircle),
                     QColor(0, 0, 0, ThemeManager::instance().isDark() ? 118 : 92));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 230), 2));
    painter.drawEllipse(rect().adjusted(1, 1, -1, -1));
}

void AvatarCropCanvas::wheelEvent(QWheelEvent* event)
{
    const int delta = event->pixelDelta().isNull()
            ? event->angleDelta().y()
            : event->pixelDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    const qreal factor = std::pow(1.0018, delta);
    setScale(m_scale * factor, event->position());
    event->accept();
}

void AvatarCropCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        m_dragging = true;
        m_dragStartPos = event->pos();
        m_dragStartOffset = m_offset;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AvatarCropCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_offset = m_dragStartOffset + QPointF(event->pos() - m_dragStartPos);
        clampOffset();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void AvatarCropCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AvatarCropCanvas::resetTransform()
{
    if (m_image.isNull()) {
        return;
    }
    m_minScale = qMax(width() / qreal(m_image.width()),
                      height() / qreal(m_image.height()));
    m_maxScale = qMax(m_minScale, m_minScale * 4.5);
    m_scale = m_minScale;
    m_offset = QPointF((width() - m_image.width() * m_scale) / 2.0,
                       (height() - m_image.height() * m_scale) / 2.0);
    clampOffset();
}

void AvatarCropCanvas::setScale(qreal scale, const QPointF& anchor)
{
    if (m_image.isNull()) {
        return;
    }
    const qreal nextScale = qBound(m_minScale, scale, m_maxScale);
    if (qFuzzyCompare(m_scale, nextScale)) {
        return;
    }
    const QPointF imageAnchor = (anchor - m_offset) / m_scale;
    m_scale = nextScale;
    m_offset = anchor - imageAnchor * m_scale;
    clampOffset();
    update();
}

void AvatarCropCanvas::clampOffset()
{
    const QSizeF scaledSize(m_image.width() * m_scale, m_image.height() * m_scale);
    m_offset.setX(scaledSize.width() <= width()
                  ? (width() - scaledSize.width()) / 2.0
                  : qBound(width() - scaledSize.width(), m_offset.x(), 0.0));
    m_offset.setY(scaledSize.height() <= height()
                  ? (height() - scaledSize.height()) / 2.0
                  : qBound(height() - scaledSize.height(), m_offset.y(), 0.0));
}
