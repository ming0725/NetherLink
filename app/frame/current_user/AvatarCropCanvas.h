#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <functional>

class QEnterEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class AvatarCropCanvas final : public QWidget
{
public:
    explicit AvatarCropCanvas(const QImage& image, QWidget* parent = nullptr);

    QImage croppedImage() const;
    int zoomValue() const;
    void setZoomValue(int value);
    void zoomIn();
    void zoomOut();

    std::function<void(int)> zoomValueChanged;

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    QRectF cropRect() const;
    QRectF imageTargetRect() const;
    void resetTransform();
    void setScale(qreal scale, const QPointF& anchor);
    void clampOffset();
    void notifyZoomChanged();

    QImage m_image;
    qreal m_minScale = 1.0;
    qreal m_maxScale = 1.0;
    qreal m_scale = 1.0;
    QPointF m_offset;
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QPointF m_dragStartOffset;
};
