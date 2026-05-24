#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

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

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void resetTransform();
    void setScale(qreal scale, const QPointF& anchor);
    void clampOffset();

    QImage m_image;
    qreal m_minScale = 1.0;
    qreal m_maxScale = 1.0;
    qreal m_scale = 1.0;
    QPointF m_offset;
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QPointF m_dragStartOffset;
};
