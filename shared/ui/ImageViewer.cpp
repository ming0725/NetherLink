#include "ImageViewer.h"

#include <cmath>

#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QAbstractAnimation>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QEasingCurve>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

#ifdef Q_OS_MACOS
#include <QNativeGestureEvent>
#endif

#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/SmoothScrollBar.h"

namespace {

constexpr qreal kMinScale = 0.05;
constexpr qreal kMaxScale = 8.0;
constexpr int kBottomBarHeight = 58;
constexpr int kToolbarButtonSize = 38;
constexpr int kScrollBarEdgeInset = 3;
constexpr int kScrollBarThickness = 8;
constexpr int kZoomAnimationDurationMs = 180;
constexpr int kRotationAnimationDurationMs = 260;
constexpr int kZoomIndicatorDurationMs = 900;
constexpr qreal kAngleWheelPanStep = 80.0;
constexpr int kMinViewerWidth = 360;
constexpr int kMinCanvasHeight = 260;
constexpr int kPrimaryInitialPadding = 96;
constexpr int kSecondaryInitialPadding = 32;
constexpr qreal kMaxScreenWidthRatio = 0.82;
constexpr qreal kMaxScreenHeightRatio = 0.82;

qreal zoomStep(int wheelDelta)
{
    const qreal steps = qreal(wheelDelta) / 120.0;
    return std::pow(1.12, steps);
}

bool hasZoomModifier(const QWheelEvent* event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    return modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
}

bool isTouchPadScrollWheel(const QWheelEvent* event)
{
    return event->phase() != Qt::NoScrollPhase;
}

QPointF scrollDeltaForPan(const QWheelEvent* event)
{
    const QPoint pixelDelta = event->pixelDelta();
    if (!pixelDelta.isNull()) {
        return QPointF(-pixelDelta.x(), -pixelDelta.y());
    }

    const QPoint angleDelta = event->angleDelta();
    return QPointF(-angleDelta.x() / 120.0 * kAngleWheelPanStep,
                   -angleDelta.y() / 120.0 * kAngleWheelPanStep);
}

qreal normalizedRotation(qreal rotation)
{
    qreal result = std::fmod(rotation, 360.0);
    if (result < 0) {
        result += 360.0;
    }
    return result;
}

qreal snappedRightAngle(qreal rotation)
{
    return (qRound(normalizedRotation(rotation) / 90.0) % 4) * 90.0;
}

qreal roundedRightAngle(qreal rotation)
{
    return qRound(rotation / 90.0) * 90.0;
}

QScreen* initialScreenFor(QWidget* parent)
{
    if (parent) {
        if (QWindow* window = parent->windowHandle()) {
            if (window->screen()) {
                return window->screen();
            }
        }
        if (QWidget* window = parent->window()) {
            if (QWindow* handle = window->windowHandle()) {
                if (handle->screen()) {
                    return handle->screen();
                }
            }
        }
    }
    return QGuiApplication::primaryScreen();
}

QSize initialViewerSizeForImage(const QSize& imageSize, QWidget* parent)
{
    QSize availableSize(1280, 800);
    if (QScreen* screen = initialScreenFor(parent)) {
        availableSize = screen->availableGeometry().size();
    }

    const int maxWidth = qMax(kMinViewerWidth, qFloor(availableSize.width() * kMaxScreenWidthRatio));
    const int maxHeight = qMax(kMinCanvasHeight + kBottomBarHeight,
                               qFloor(availableSize.height() * kMaxScreenHeightRatio));
    const int maxCanvasHeight = qMax(kMinCanvasHeight, maxHeight - kBottomBarHeight);

    if (!imageSize.isValid() || imageSize.isEmpty()) {
        return QSize(qMin(920, maxWidth), qMin(680, maxHeight));
    }

    const QSizeF sourceSize(imageSize);
    const qreal aspectRatio = sourceSize.width() / sourceSize.height();
    int horizontalPadding = kSecondaryInitialPadding;
    int verticalPadding = kSecondaryInitialPadding;
    if (aspectRatio < 0.78) {
        horizontalPadding = kPrimaryInitialPadding;
    } else if (aspectRatio > 1.28) {
        verticalPadding = kPrimaryInitialPadding;
    } else {
        horizontalPadding = 72;
        verticalPadding = 56;
    }

    horizontalPadding = qMin(horizontalPadding, qMax(0, maxWidth - kMinViewerWidth));
    verticalPadding = qMin(verticalPadding, qMax(0, maxCanvasHeight - kMinCanvasHeight));

    const QSizeF imageBounds(qMax<qreal>(1.0, maxWidth - horizontalPadding),
                             qMax<qreal>(1.0, maxCanvasHeight - verticalPadding));
    const qreal scale = qMin<qreal>(1.0,
                                    qMin(imageBounds.width() / sourceSize.width(),
                                         imageBounds.height() / sourceSize.height()));
    const int canvasWidth = qBound(kMinViewerWidth,
                                   qRound(sourceSize.width() * scale) + horizontalPadding,
                                   maxWidth);
    const int canvasHeight = qBound(kMinCanvasHeight,
                                    qRound(sourceSize.height() * scale) + verticalPadding,
                                    maxCanvasHeight);
    return QSize(canvasWidth, canvasHeight + kBottomBarHeight);
}

QSize logicalPixmapSize(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return {};
    }

    const qreal dpr = qMax<qreal>(1.0, pixmap.devicePixelRatio());
    return QSize(qMax(1, qRound(pixmap.width() / dpr)),
                 qMax(1, qRound(pixmap.height() / dpr)));
}

} // namespace

class ImageViewer::IconButton : public QPushButton
{
public:
    IconButton(const QString& iconSource, const QString& tooltip, QWidget* parent = nullptr)
        : QPushButton(parent)
        , m_icon(iconSource)
    {
        setFixedSize(kToolbarButtonSize, kToolbarButtonSize);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(tooltip);
        setFlat(true);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QColor fill = Qt::transparent;
        if (isDown()) {
            fill = ThemeManager::instance().color(ThemeColor::Divider);
        } else if (underMouse()) {
            fill = ThemeManager::instance().color(ThemeColor::ListHover);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

        const int iconExtent = 24;
        const QRect iconRect(QPoint((width() - iconExtent) / 2, (height() - iconExtent) / 2),
                             QSize(iconExtent, iconExtent));
        const qreal dpr = devicePixelRatioF();
        const QSize pixmapSize(qRound(iconRect.width() * dpr), qRound(iconRect.height() * dpr));
        QPixmap pixmap = m_icon.pixmap(pixmapSize);
        if (pixmap.isNull()) {
            return;
        }

        pixmap.setDevicePixelRatio(dpr);
        QPixmap tinted(pixmap.size());
        tinted.setDevicePixelRatio(dpr);
        tinted.fill(Qt::transparent);

        QPainter iconPainter(&tinted);
        iconPainter.drawPixmap(0, 0, pixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(tinted.rect(), ThemeManager::instance().color(ThemeColor::PrimaryText));
        iconPainter.end();

        painter.drawPixmap(iconRect, tinted);
    }

private:
    QIcon m_icon;
};

ImageCanvas::ImageCanvas(QWidget* parent)
    : QWidget(parent)
    , m_verticalScrollBar(new SmoothScrollBar(this))
    , m_horizontalScrollBar(new SmoothScrollBar(this))
    , m_zoomIndicatorTimer(new QTimer(this))
    , m_scaleAnimation(new QVariantAnimation(this))
    , m_rotationAnimation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_horizontalScrollBar->setOrientation(Qt::Horizontal);
    m_scaleAnimation->setDuration(kZoomAnimationDurationMs);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scaleAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyScale(value.toReal(), m_scaleAnimationAnchor, false);
    });
    connect(m_scaleAnimation, &QVariantAnimation::finished, this, [this]() {
        showZoomIndicator();
    });

    m_rotationAnimation->setDuration(kRotationAnimationDurationMs);
    m_rotationAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_rotationAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyRotation(value.toReal(), m_rotationAnimationCenterRatio);
    });
    connect(m_rotationAnimation, &QVariantAnimation::finished, this, [this]() {
        applyRotation(snappedRightAngle(m_rotation), m_rotationAnimationCenterRatio);
    });

    m_zoomIndicatorTimer->setSingleShot(true);
    connect(m_zoomIndicatorTimer, &QTimer::timeout, this, [this]() {
        m_showZoomIndicator = false;
        update();
    });

    connect(m_verticalScrollBar, &SmoothScrollBar::valueChanged, this, [this](int value) {
        if (qRound(m_scrollPosition.y()) != value) {
            m_scrollPosition.setY(value);
            updateScrollBars();
            update();
        }
    });
    connect(m_horizontalScrollBar, &SmoothScrollBar::valueChanged, this, [this](int value) {
        if (qRound(m_scrollPosition.x()) != value) {
            m_scrollPosition.setX(value);
            updateScrollBars();
            update();
        }
    });
}

bool ImageCanvas::setImageSource(const QString& source)
{
    const QPixmap pixmap = ImageService::instance().pixmap(source);
    return setImagePixmap(pixmap, source);
}

bool ImageCanvas::setImagePixmap(const QPixmap& pixmap, const QString& source)
{
    if (pixmap.isNull()) {
        return false;
    }

    stopTransformAnimations();
    m_source = source;
    m_pixmap = pixmap;
    m_rotation = 0;
    m_scrollPosition = {};
    m_fitPending = true;
    fitToView();
    update();
    return true;
}

bool ImageCanvas::replaceImage(const QImage& image, const QString& source)
{
    if (image.isNull()) {
        return false;
    }

    return setImagePixmap(QPixmap::fromImage(image), source);
}

int ImageCanvas::zoomPercent() const
{
    return qRound(m_scale * 100.0);
}

QImage ImageCanvas::renderedImage() const
{
    if (m_pixmap.isNull()) {
        return {};
    }

    const QImage sourceImage = m_pixmap.toImage();
    if (qFuzzyIsNull(normalizedRotation(m_rotation))) {
        return sourceImage;
    }

    const QSizeF targetSize = rotatedSourceSize();
    QImage image(QSize(qCeil(targetSize.width()), qCeil(targetSize.height())),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(QRectF(QPointF(0, 0), targetSize).center());
    painter.rotate(m_rotation);
    painter.drawImage(QRectF(-sourceImage.width() / 2.0,
                             -sourceImage.height() / 2.0,
                             sourceImage.width(),
                             sourceImage.height()),
                      sourceImage);
    return image;
}

void ImageCanvas::zoomIn()
{
    animateScaleTo(m_scale * 1.2, rect().center());
}

void ImageCanvas::zoomOut()
{
    animateScaleTo(m_scale / 1.2, rect().center());
}

void ImageCanvas::rotateClockwise()
{
    if (m_rotationAnimation->state() == QAbstractAnimation::Running) {
        return;
    }

    animateRotationTo(snappedRightAngle(m_rotation) + 90.0);
}

void ImageCanvas::fitToView()
{
    if (m_pixmap.isNull() || width() <= 0 || height() <= 0) {
        return;
    }

    stopTransformAnimations();
    m_scale = fitScaleForViewport();
    m_scrollPosition = {};
    updateScrollBars();
    emit zoomPercentChanged(zoomPercent());
    update();
}

bool ImageCanvas::event(QEvent* event)
{
#ifdef Q_OS_MACOS
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            const qreal factor = 1.0 + gesture->value();
            if (factor > 0.0) {
                setScale(m_scale * factor, gesture->position());
            }
            event->accept();
            return true;
        }
    }
#endif
    return QWidget::event(event);
}

void ImageCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

    if (!m_pixmap.isNull()) {
        const QSizeF targetSize = contentSize();
        const QPointF topLeft = contentTopLeft();
        const QRectF targetRect(topLeft, targetSize);

        painter.save();
        painter.translate(targetRect.center());
        painter.rotate(m_rotation);
        painter.drawPixmap(QRectF(-m_pixmap.width() * m_scale / 2.0,
                                  -m_pixmap.height() * m_scale / 2.0,
                                  m_pixmap.width() * m_scale,
                                  m_pixmap.height() * m_scale),
                           m_pixmap,
                           QRectF(QPointF(0, 0), m_pixmap.size()));
        painter.restore();
    }

    if (m_showZoomIndicator) {
        const QString text = QStringLiteral("%1%").arg(zoomPercent());
        QFont font = AppFonts::applicationPixelSizedFont(15);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const QSize textSize(metrics.horizontalAdvance(text), metrics.height());
        const QRect indicatorRect((width() - textSize.width() - 30) / 2,
                                  (height() - textSize.height() - 18) / 2,
                                  textSize.width() + 30,
                                  textSize.height() + 18);
        QColor background = ThemeManager::instance().color(ThemeColor::PanelRaisedBackground);
        background.setAlpha(190);
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(indicatorRect, 10, 10);
        painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        painter.drawText(indicatorRect, Qt::AlignCenter, text);
    }
}

void ImageCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateScrollBarGeometry();
    if (m_fitPending) {
        m_fitPending = false;
        fitToView();
    } else {
        setScrollPosition(m_scrollPosition);
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event)
{
    if (isTouchPadScrollWheel(event) && !hasZoomModifier(event)) {
        const QPointF scrollDelta = scrollDeltaForPan(event);
        if (!scrollDelta.isNull()) {
            setScrollPosition(m_scrollPosition + scrollDelta);
            showScrollBars();
        }
        event->accept();
        return;
    }

    const int delta = event->pixelDelta().isNull()
                          ? event->angleDelta().y()
                          : event->pixelDelta().y();
    if (delta != 0) {
        setScale(m_scale * zoomStep(delta), event->position());
        event->accept();
        return;
    }

    QWidget::wheelEvent(event);
}

void ImageCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_pixmap.isNull()) {
        m_dragging = true;
        m_dragStartPos = event->pos();
        m_dragStartScroll = m_scrollPosition;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        const QPoint delta = event->pos() - m_dragStartPos;
        setScrollPosition(m_dragStartScroll - QPointF(delta.x(), delta.y()));
        showScrollBars();
        event->accept();
        return;
    }
    setCursor(Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QSizeF ImageCanvas::rotatedSourceSize() const
{
    if (m_pixmap.isNull()) {
        return {};
    }

    const QSizeF sourceSize = m_pixmap.size();
    const qreal radians = qDegreesToRadians(normalizedRotation(m_rotation));
    const qreal sinValue = qAbs(std::sin(radians));
    const qreal cosValue = qAbs(std::cos(radians));
    return QSizeF(sourceSize.width() * cosValue + sourceSize.height() * sinValue,
                  sourceSize.width() * sinValue + sourceSize.height() * cosValue);
}

QSizeF ImageCanvas::contentSize() const
{
    return rotatedSourceSize() * m_scale;
}

QPointF ImageCanvas::contentTopLeft() const
{
    const QSizeF size = contentSize();
    return QPointF(size.width() <= width() ? (width() - size.width()) / 2.0 : -m_scrollPosition.x(),
                   size.height() <= height() ? (height() - size.height()) / 2.0 : -m_scrollPosition.y());
}

void ImageCanvas::animateScaleTo(qreal scale, const QPointF& anchor)
{
    if (m_pixmap.isNull()) {
        return;
    }

    if (m_scaleAnimation->state() == QAbstractAnimation::Running) {
        m_scaleAnimation->stop();
    }

    const qreal targetScale = qBound(kMinScale, scale, kMaxScale);
    if (qFuzzyCompare(m_scale, targetScale)) {
        return;
    }

    m_scaleAnimationAnchor = anchor;
    m_scaleAnimation->setStartValue(m_scale);
    m_scaleAnimation->setEndValue(targetScale);
    m_scaleAnimation->start();
}

void ImageCanvas::animateRotationTo(qreal rotation)
{
    if (m_pixmap.isNull()) {
        return;
    }

    if (m_rotationAnimation->state() == QAbstractAnimation::Running) {
        m_rotationAnimation->stop();
    }

    const QPointF oldTopLeft = contentTopLeft();
    const QPointF viewportCenter(width() / 2.0, height() / 2.0);
    const QSizeF oldSize = contentSize();
    m_rotationAnimationCenterRatio = QPointF(
            oldSize.width() > 0
                    ? qBound<qreal>(0.0, (viewportCenter.x() - oldTopLeft.x()) / oldSize.width(), 1.0)
                    : 0.5,
            oldSize.height() > 0
                    ? qBound<qreal>(0.0, (viewportCenter.y() - oldTopLeft.y()) / oldSize.height(), 1.0)
                    : 0.5);
    m_rotationAnimation->setStartValue(snappedRightAngle(m_rotation));
    m_rotationAnimation->setEndValue(roundedRightAngle(rotation));
    m_rotationAnimation->start();
}

void ImageCanvas::applyScale(qreal scale, const QPointF& anchor, bool showIndicator)
{
    if (m_pixmap.isNull()) {
        return;
    }

    const QSizeF oldSize = contentSize();
    const QPointF oldTopLeft = contentTopLeft();
    QPointF ratio(0.5, 0.5);
    if (oldSize.width() > 0 && oldSize.height() > 0) {
        ratio.setX(qBound<qreal>(0.0, (anchor.x() - oldTopLeft.x()) / oldSize.width(), 1.0));
        ratio.setY(qBound<qreal>(0.0, (anchor.y() - oldTopLeft.y()) / oldSize.height(), 1.0));
    }

    const qreal nextScale = qBound(kMinScale, scale, kMaxScale);
    if (qFuzzyCompare(m_scale, nextScale)) {
        return;
    }

    m_scale = nextScale;
    const QSizeF newSize = contentSize();
    setScrollPosition(QPointF(newSize.width() * ratio.x() - anchor.x(),
                              newSize.height() * ratio.y() - anchor.y()));
    emit zoomPercentChanged(zoomPercent());
    if (showIndicator) {
        showZoomIndicator();
    }
}

void ImageCanvas::applyRotation(qreal rotation, const QPointF& centerRatio)
{
    m_rotation = rotation;
    const QSizeF newSize = contentSize();
    setScrollPosition(QPointF(newSize.width() * centerRatio.x() - width() / 2.0,
                              newSize.height() * centerRatio.y() - height() / 2.0));
}

void ImageCanvas::setScale(qreal scale, const QPointF& anchor)
{
    if (m_scaleAnimation->state() == QAbstractAnimation::Running) {
        m_scaleAnimation->stop();
    }
    applyScale(scale, anchor, true);
}

void ImageCanvas::stopTransformAnimations()
{
    if (m_scaleAnimation->state() == QAbstractAnimation::Running) {
        m_scaleAnimation->stop();
    }
    if (m_rotationAnimation->state() == QAbstractAnimation::Running) {
        m_rotationAnimation->stop();
    }
}

void ImageCanvas::setScrollPosition(const QPointF& position)
{
    const QSizeF size = contentSize();
    const qreal maxX = qMax<qreal>(0.0, size.width() - width());
    const qreal maxY = qMax<qreal>(0.0, size.height() - height());
    m_scrollPosition = QPointF(qBound<qreal>(0.0, position.x(), maxX),
                               qBound<qreal>(0.0, position.y(), maxY));
    updateScrollBars();
    update();
}

void ImageCanvas::updateScrollBars()
{
    const QSizeF size = contentSize();
    const int maxX = qMax(0, qCeil(size.width() - width()));
    const int maxY = qMax(0, qCeil(size.height() - height()));

    m_horizontalScrollBar->setRange(0, maxX);
    m_horizontalScrollBar->setPageStep(width());
    m_horizontalScrollBar->setValue(qRound(m_scrollPosition.x()));

    m_verticalScrollBar->setRange(0, maxY);
    m_verticalScrollBar->setPageStep(height());
    m_verticalScrollBar->setValue(qRound(m_scrollPosition.y()));
    updateScrollBarGeometry();
}

void ImageCanvas::updateScrollBarGeometry()
{
    const QRectF contentRect(contentTopLeft(), contentSize());
    const QRectF visibleContentRect = contentRect.intersected(QRectF(rect()));
    if (visibleContentRect.isEmpty()) {
        return;
    }

    const bool showVertical = m_verticalScrollBar->needsDisplay();
    const bool showHorizontal = m_horizontalScrollBar->needsDisplay();
    const int cornerReserve = showVertical && showHorizontal
            ? kScrollBarThickness + kScrollBarEdgeInset
            : 0;

    const int verticalX = qRound(visibleContentRect.right()) - kScrollBarThickness - kScrollBarEdgeInset;
    const int verticalY = qRound(visibleContentRect.top()) + kScrollBarEdgeInset;
    const int verticalHeight = qMax(0,
                                    qRound(visibleContentRect.height())
                                            - kScrollBarEdgeInset * 2
                                            - cornerReserve);
    m_verticalScrollBar->setGeometry(verticalX,
                                     verticalY,
                                     kScrollBarThickness,
                                     verticalHeight);

    const int horizontalX = qRound(visibleContentRect.left()) + kScrollBarEdgeInset;
    const int horizontalY = qRound(visibleContentRect.bottom()) - kScrollBarThickness - kScrollBarEdgeInset;
    const int horizontalWidth = qMax(0,
                                     qRound(visibleContentRect.width())
                                             - kScrollBarEdgeInset * 2
                                             - cornerReserve);
    m_horizontalScrollBar->setGeometry(horizontalX,
                                       horizontalY,
                                       horizontalWidth,
                                       kScrollBarThickness);
    m_verticalScrollBar->raise();
    m_horizontalScrollBar->raise();
}

void ImageCanvas::showScrollBars()
{
    updateScrollBars();
    if (m_verticalScrollBar->needsDisplay()) {
        m_verticalScrollBar->showScrollBar();
    }
    if (m_horizontalScrollBar->needsDisplay()) {
        m_horizontalScrollBar->showScrollBar();
    }
}

void ImageCanvas::showZoomIndicator()
{
    m_showZoomIndicator = true;
    m_zoomIndicatorTimer->start(kZoomIndicatorDurationMs);
    showScrollBars();
    update();
}

qreal ImageCanvas::fitScaleForViewport() const
{
    const QSizeF sourceSize = rotatedSourceSize();
    if (sourceSize.isEmpty() || width() <= 0 || height() <= 0) {
        return 1.0;
    }

    const qreal scale = qMin(width() / sourceSize.width(), height() / sourceSize.height());
    return qBound(kMinScale, qMin<qreal>(1.0, scale), kMaxScale);
}

ImageViewer::ImageViewer(const QString& imageSource, QWidget* parent)
    : QDialog(parent)
    , m_canvas(new ImageCanvas(this))
    , m_percentLabel(new QLabel(this))
{
    setupViewerUi(initialViewerSizeForImage(ImageService::instance().sourceSize(imageSource), parent));

    if (!m_canvas->setImageSource(imageSource)) {
        close();
        return;
    }
    updatePercentLabel(m_canvas->zoomPercent());
}

ImageViewer::ImageViewer(const QPixmap& initialPixmap,
                         const QString& imageSource,
                         QWidget* parent)
    : QDialog(parent)
    , m_canvas(new ImageCanvas(this))
    , m_percentLabel(new QLabel(this))
{
    setupViewerUi(initialViewerSizeForImage(logicalPixmapSize(initialPixmap), parent));

    if (!m_canvas->setImagePixmap(initialPixmap, imageSource)) {
        close();
        return;
    }
    updatePercentLabel(m_canvas->zoomPercent());
}

void ImageViewer::setupViewerUi(const QSize& initialSize)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("图片查看器"));
    setMinimumSize(kMinViewerWidth, kMinCanvasHeight + kBottomBarHeight);
    resize(initialSize);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_canvas, 1);

    auto* bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(kBottomBarHeight);
    bottomBar->setAutoFillBackground(true);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(18, 8, 18, 8);
    bottomLayout->setSpacing(10);
    bottomLayout->addStretch();

    auto* zoomOutButton = new IconButton(QStringLiteral(":/resources/icon/image_viewer_zoom_out.svg"),
                                         QStringLiteral("缩小"),
                                         bottomBar);
    auto* zoomInButton = new IconButton(QStringLiteral(":/resources/icon/image_viewer_zoom_in.svg"),
                                        QStringLiteral("放大"),
                                        bottomBar);
    auto* rotateButton = new IconButton(QStringLiteral(":/resources/icon/image_viewer_rotate.svg"),
                                        QStringLiteral("旋转"),
                                        bottomBar);
    auto* saveButton = new IconButton(QStringLiteral(":/resources/icon/image_viewer_save_as.svg"),
                                      QStringLiteral("另存为"),
                                      bottomBar);

    m_percentLabel->setAlignment(Qt::AlignCenter);
    m_percentLabel->setMinimumWidth(70);
    m_percentLabel->setFont(AppFonts::applicationPixelSizedFont(14));

    bottomLayout->addWidget(zoomOutButton);
    bottomLayout->addWidget(m_percentLabel);
    bottomLayout->addWidget(zoomInButton);
    bottomLayout->addWidget(rotateButton);
    bottomLayout->addWidget(saveButton);
    bottomLayout->addStretch();
    rootLayout->addWidget(bottomBar);

    connect(zoomOutButton, &QPushButton::clicked, m_canvas, &ImageCanvas::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, m_canvas, &ImageCanvas::zoomIn);
    connect(rotateButton, &QPushButton::clicked, m_canvas, &ImageCanvas::rotateClockwise);
    connect(saveButton, &QPushButton::clicked, this, &ImageViewer::saveAs);
    connect(m_canvas, &ImageCanvas::zoomPercentChanged, this, &ImageViewer::updatePercentLabel);
    auto applyTheme = [this, bottomBar]() {
        QPalette palette = bottomBar->palette();
        palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::PanelBackground));
        bottomBar->setPalette(palette);
        bottomBar->update();
        updatePercentLabel(m_canvas->zoomPercent());
        update();
    };
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, applyTheme);
}

bool ImageViewer::replaceImage(const QImage& image, const QString& source)
{
    if (!m_canvas->replaceImage(image, source)) {
        return false;
    }
    updatePercentLabel(m_canvas->zoomPercent());
    return true;
}

void ImageViewer::updatePercentLabel(int percent)
{
    m_percentLabel->setText(QStringLiteral("%1%").arg(percent));
    QPalette palette = m_percentLabel->palette();
    const QColor textColor = ThemeManager::instance().color(ThemeColor::PrimaryText);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Text, textColor);
    m_percentLabel->setPalette(palette);
}

void ImageViewer::saveAs()
{
    const QString defaultName = QFileInfo(m_canvas->imageSource()).completeBaseName().isEmpty()
            ? QStringLiteral("image.png")
            : QFileInfo(m_canvas->imageSource()).completeBaseName() + QStringLiteral(".png");
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    QStringLiteral("另存为"),
                                                    defaultName,
                                                    QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg);;BMP 图片 (*.bmp)"));
    if (fileName.isEmpty()) {
        return;
    }
    if (QFileInfo(fileName).suffix().isEmpty()) {
        fileName += QStringLiteral(".png");
    }
    m_canvas->renderedImage().save(fileName);
}
