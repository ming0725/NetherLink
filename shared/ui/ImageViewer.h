#pragma once

#include <QDialog>
#include <QImage>
#include <QPointF>
#include <QPixmap>

class QLabel;
class QMouseEvent;
class QPushButton;
class QTimer;
class QVariantAnimation;
class QWheelEvent;
class SmoothScrollBar;

class ImageCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    bool setImageSource(const QString& source);
    bool setImagePixmap(const QPixmap& pixmap, const QString& source = QString());
    bool replaceImage(const QImage& image, const QString& source = QString());
    QString imageSource() const { return m_source; }
    int zoomPercent() const;
    QImage renderedImage() const;

public slots:
    void zoomIn();
    void zoomOut();
    void rotateClockwise();
    void fitToView();

signals:
    void zoomPercentChanged(int percent);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QSizeF rotatedSourceSize() const;
    QSizeF contentSize() const;
    QPointF contentTopLeft() const;
    void animateScaleTo(qreal scale, const QPointF& anchor);
    void animateRotationTo(qreal rotation);
    void applyScale(qreal scale, const QPointF& anchor, bool showIndicator);
    void applyRotation(qreal rotation, const QPointF& centerRatio);
    void setScale(qreal scale, const QPointF& anchor);
    void setScrollPosition(const QPointF& position);
    void stopTransformAnimations();
    void updateScrollBars();
    void updateScrollBarGeometry();
    void showScrollBars();
    void showZoomIndicator();
    qreal fitScaleForViewport() const;

    QString m_source;
    QPixmap m_pixmap;
    qreal m_scale = 1.0;
    qreal m_rotation = 0.0;
    QPointF m_scrollPosition;
    QPointF m_scaleAnimationAnchor;
    QPointF m_rotationAnimationCenterRatio = QPointF(0.5, 0.5);
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QPointF m_dragStartScroll;
    bool m_fitPending = false;
    bool m_showZoomIndicator = false;
    SmoothScrollBar* m_verticalScrollBar = nullptr;
    SmoothScrollBar* m_horizontalScrollBar = nullptr;
    QTimer* m_zoomIndicatorTimer = nullptr;
    QVariantAnimation* m_scaleAnimation = nullptr;
    QVariantAnimation* m_rotationAnimation = nullptr;
};

class ImageViewer : public QDialog
{
    Q_OBJECT

public:
    explicit ImageViewer(const QString& imageSource, QWidget* parent = nullptr);
    explicit ImageViewer(const QPixmap& initialPixmap,
                         const QString& imageSource,
                         QWidget* parent = nullptr);

    bool replaceImage(const QImage& image, const QString& source = QString());

private:
    class IconButton;

    void setupViewerUi(const QSize& initialSize);
    void updatePercentLabel(int percent);
    void saveAs();

    ImageCanvas* m_canvas = nullptr;
    QLabel* m_percentLabel = nullptr;
};
