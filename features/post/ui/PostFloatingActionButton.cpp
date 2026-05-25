#include "PostFloatingActionButton.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/QtFallbackLiquidGlass.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>

#ifdef Q_OS_MACOS
#include "platform/macos/MacPostBarBridge_p.h"
#endif

namespace {

constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr auto kUsesSystemFloatingBarBridgeProperty = "usesSystemFloatingBarBridge";
constexpr int kButtonDiameter = 44;

} // namespace

PostFloatingActionButton::PostFloatingActionButton(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFixedSize(sizeHint());

    m_liquidGlass = new QtFallbackLiquidGlassController(this);
    m_liquidGlass->setEffectMode(QtFallbackLiquidGlassController::EffectMode::GaussianBlur);
    m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            width() / 2.0);

#ifdef Q_OS_MACOS
    m_usesNativeButton = MacPostBarBridge::appearance() != MacPostBarBridge::Appearance::Unsupported;
    setProperty(kUsesSystemFloatingBarBridgeProperty, m_usesNativeButton);
#endif

    updatePanelShadow();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        syncPlatformButton();
        updatePanelShadow();
        updateQtFallbackLiquidGlassState();
        update();
    });
}

PostFloatingActionButton::~PostFloatingActionButton()
{
    releaseQtFallbackLiquidGlassResources(false);
#ifdef Q_OS_MACOS
    if (m_usesNativeButton) {
        MacPostBarBridge::clearActionButton(this);
    }
#endif
}

bool PostFloatingActionButton::event(QEvent* event)
{
    const bool handled = QWidget::event(event);

#ifdef Q_OS_MACOS
    if (m_usesNativeButton) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WinIdChange:
        case QEvent::ParentChange:
        case QEvent::ZOrderChange:
            syncPlatformButton();
            break;
        case QEvent::Hide:
            MacPostBarBridge::clearActionButton(this);
            break;
        default:
            break;
        }
        return handled;
    }
#endif

    switch (event->type()) {
    case QEvent::Show:
        updateQtFallbackLiquidGlassState();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        scheduleLiquidGlassUpdate(QtFallbackLiquidGlassController::refreshDelayMs());
        break;
    case QEvent::Hide:
        releaseQtFallbackLiquidGlassResources();
        break;
    default:
        break;
    }
    if (m_liquidGlass) {
        m_liquidGlass->handleHostEvent(event);
    }

    return handled;
}

QSize PostFloatingActionButton::minimumSizeHint() const
{
    return sizeHint();
}

QSize PostFloatingActionButton::sizeHint() const
{
    return QSize(kButtonDiameter, kButtonDiameter);
}

void PostFloatingActionButton::setVisualOpacity(qreal opacity)
{
    const qreal bounded = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_visualOpacity, bounded)) {
        return;
    }

    m_visualOpacity = bounded;
    update();
    syncPlatformButton();
}

void PostFloatingActionButton::setLiquidGlassSourceWidget(QWidget* widget)
{
    if (!m_liquidGlass) {
        return;
    }
    m_liquidGlass->setSourceWidget(widget);
    updateQtFallbackLiquidGlassState();
}

void PostFloatingActionButton::scheduleLiquidGlassUpdate(int delayMs)
{
    if (!m_liquidGlass || !shouldUseQtFallbackLiquidGlass()) {
        return;
    }
    m_liquidGlass->scheduleUpdate(delayMs);
}

void PostFloatingActionButton::scheduleLiquidGlassInteractiveUpdate()
{
    if (!m_liquidGlass || !shouldUseQtFallbackLiquidGlass()) {
        return;
    }
    m_liquidGlass->scheduleInteractiveUpdate();
}

bool PostFloatingActionButton::usesQtFallbackLiquidGlass() const
{
    return shouldUseQtFallbackLiquidGlass();
}

void PostFloatingActionButton::refreshPlatformAppearance()
{
#ifdef Q_OS_MACOS
    const bool shouldUseNative = MacPostBarBridge::appearance()
            != MacPostBarBridge::Appearance::Unsupported;
    const bool systemSuppressed = property(kSystemFloatingBarsSuppressedProperty).toBool();
    setProperty(kUsesSystemFloatingBarBridgeProperty, shouldUseNative);

    if (m_usesNativeButton && !shouldUseNative) {
        MacPostBarBridge::clearActionButton(this);
        m_usesNativeButton = false;
        updatePanelShadow();
        updateQtFallbackLiquidGlassState();
        update();
        return;
    }

    if (!m_usesNativeButton && shouldUseNative && systemSuppressed) {
        updateQtFallbackLiquidGlassState();
        update();
        return;
    }

    if (!m_usesNativeButton && shouldUseNative) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        releaseQtFallbackLiquidGlassResources(false);
        m_usesNativeButton = true;
        syncPlatformButton();
        update();
        return;
    }

    if (m_usesNativeButton) {
        syncPlatformButton();
    }
#endif

    updatePanelShadow();
    updateQtFallbackLiquidGlassState();
    update();
}

void PostFloatingActionButton::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_liquidGlass) {
        m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                qMin(width(), height()) / 2.0);
    }
    scheduleLiquidGlassUpdate(0);
}

void PostFloatingActionButton::paintEvent(QPaintEvent*)
{
    if (m_usesNativeButton) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(m_visualOpacity);

    const QRectF contentRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = qMin(contentRect.width(), contentRect.height()) / 2.0;
    const bool usesGaussianFallback = shouldUseQtFallbackLiquidGlass() && m_liquidGlass;

    painter.setPen(Qt::NoPen);
    if (usesGaussianFallback) {
        m_liquidGlass->setShape(contentRect, radius);
        m_liquidGlass->paint(painter);
        if (m_hovered) {
            QColor hover = ThemeManager::instance().color(ThemeColor::PostBarItemSelectedBackground);
            hover.setAlpha(ThemeManager::instance().isDark() ? 70 : 44);
            painter.setBrush(hover);
            painter.drawEllipse(contentRect.adjusted(5.0, 5.0, -5.0, -5.0));
        }
    } else {
        QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
        background.setAlpha(ThemeManager::instance().isDark() ? 190 : 205);
        painter.setBrush(background);
        painter.drawEllipse(contentRect);
    }

    if (!shouldUseQtFallbackLiquidGlass()) {
        const qreal borderWidth = 2.0;
        const qreal inset = borderWidth / 2.0;
        QPen pen(ThemeManager::instance().color(ThemeColor::Accent));
        pen.setWidthF(borderWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(contentRect.adjusted(inset, inset, -inset, -inset));
    }

    const QColor iconColor = ThemeManager::instance().color(ThemeColor::PrimaryText);
    QPen iconPen(iconColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(iconPen);
    const QPointF center = contentRect.center();
    const qreal halfLength = 6.1;
    painter.drawLine(QPointF(center.x() - halfLength, center.y()),
                     QPointF(center.x() + halfLength, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - halfLength),
                     QPointF(center.x(), center.y() + halfLength));
}

void PostFloatingActionButton::mouseMoveEvent(QMouseEvent* event)
{
    if (m_usesNativeButton) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const bool hovered = rect().contains(event->pos());
    if (m_hovered != hovered) {
        m_hovered = hovered;
        if (m_hovered) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PostFloatingActionButton::leaveEvent(QEvent* event)
{
    if (!m_usesNativeButton) {
        m_hovered = false;
        unsetCursor();
        update();
    }
    QWidget::leaveEvent(event);
}

void PostFloatingActionButton::mousePressEvent(QMouseEvent* event)
{
    if (m_usesNativeButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit clicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PostFloatingActionButton::onNativeActionTriggered()
{
    emit clicked();
}

void PostFloatingActionButton::syncPlatformButton()
{
#ifdef Q_OS_MACOS
    if (!m_usesNativeButton) {
        return;
    }

    if (property(kSystemFloatingBarsSuppressedProperty).toBool()) {
        MacPostBarBridge::clearActionButton(this);
        return;
    }

    MacPostBarBridge::syncActionButton(this, m_visualOpacity);
#endif
}

void PostFloatingActionButton::updatePanelShadow()
{
    if (m_usesNativeButton || shouldUseQtFallbackLiquidGlass()) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        return;
    }

    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (!shadow) {
        shadow = new QGraphicsDropShadowEffect(this);
        setGraphicsEffect(shadow);
    }

    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::FloatingPanelShadow));
}

bool PostFloatingActionButton::shouldUseQtFallbackLiquidGlass() const
{
    return !m_usesNativeButton
            && ThemeManager::instance().postBarQtFallbackLiquidGlassEnabled();
}

void PostFloatingActionButton::updateQtFallbackLiquidGlassState()
{
    updatePanelShadow();
    if (m_liquidGlass) {
        m_liquidGlass->setEnabled(shouldUseQtFallbackLiquidGlass() && isVisible());
    }
}

void PostFloatingActionButton::releaseQtFallbackLiquidGlassResources(bool updateWidget)
{
    if (m_liquidGlass) {
        m_liquidGlass->release(updateWidget);
    }
}
