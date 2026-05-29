#include "WindowsWindowControlButton.h"

#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

namespace {
constexpr int kWindowControlButtonWidth = 32;
constexpr int kWindowControlButtonHeight = 32;
constexpr qreal kWindowControlIconScale = 0.5;
constexpr qreal kMinimizeIconHalfWidth = 3.8;
constexpr qreal kMinimizeIconYOffset = 2.5;
constexpr qreal kMaximizeIconSize = 7.0;
constexpr qreal kRestoreIconSize = 6.0;
constexpr qreal kRestoreIconOffset = 2.2;
constexpr qreal kCloseIconRadius = 3.8;

QString windowControlIconPath(WindowsWindowControlButton::Kind kind, bool hoveredOrPressed)
{
    switch (kind) {
    case WindowsWindowControlButton::Kind::Minimize:
        return QStringLiteral(":/resources/icon/minimize.png");
    case WindowsWindowControlButton::Kind::Maximize:
        return QStringLiteral(":/resources/icon/maximize.png");
    case WindowsWindowControlButton::Kind::Close:
        return hoveredOrPressed
                ? QStringLiteral(":/resources/icon/hovered_close.png")
                : QStringLiteral(":/resources/icon/close.png");
    }
    return {};
}
}

WindowsWindowControlButton::WindowsWindowControlButton(Kind kind, QWidget* parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    setFixedSize(kWindowControlButtonWidth, kWindowControlButtonHeight);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, false);
}

void WindowsWindowControlButton::setNativeState(bool hovered, bool pressed)
{
    if (m_hovered != hovered) {
        m_hovered = hovered;
    }
    if (isDown() != pressed) {
        setDown(pressed);
    }
    update();
}

void WindowsWindowControlButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void WindowsWindowControlButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    setDown(false);
    update();
    QAbstractButton::leaveEvent(event);
}

void WindowsWindowControlButton::mousePressEvent(QMouseEvent* event)
{
    QAbstractButton::mousePressEvent(event);
    update();
}

void WindowsWindowControlButton::mouseReleaseEvent(QMouseEvent* event)
{
    QAbstractButton::mouseReleaseEvent(event);
    update();
}

void WindowsWindowControlButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool pressed = isDown();
    if (m_hovered || pressed) {
        QColor fill = m_kind == Kind::Close
                ? ThemeManager::instance().color(ThemeColor::WindowCloseHover)
                : ThemeManager::instance().color(pressed ? ThemeColor::ControlPressed
                                                         : ThemeColor::ControlHover);
        if (pressed && m_kind == Kind::Close) {
            fill = fill.darker(112);
        }
        painter.fillRect(rect(), fill);
    }

    const QColor iconColor = m_kind == Kind::Close && (m_hovered || pressed)
            ? QColor(255, 255, 255)
            : ThemeManager::instance().color(ThemeColor::SecondaryText);
    painter.setPen(QPen(iconColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QPointF center(width() / 2.0, height() / 2.0);
    const QPixmap icon(windowControlIconPath(m_kind, m_hovered || pressed));
    if (!icon.isNull()) {
        const qreal iconSize = qMin(width(), height()) * kWindowControlIconScale;
        const qreal dpr = devicePixelRatioF();
        const int targetPixelSize = qMax(1, qRound(iconSize * dpr));
        QPixmap scaledIcon = icon.scaled(QSize(targetPixelSize, targetPixelSize),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        scaledIcon.setDevicePixelRatio(dpr);
        const QSizeF logicalIconSize(scaledIcon.width() / dpr, scaledIcon.height() / dpr);
        const QPointF iconTopLeft(center.x() - logicalIconSize.width() / 2.0,
                                  center.y() - logicalIconSize.height() / 2.0);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(iconTopLeft, scaledIcon);
        return;
    }
    switch (m_kind) {
    case Kind::Minimize:
        paintMinimizeIcon(painter, center);
        break;
    case Kind::Maximize:
        if (window() && window()->isMaximized()) {
            paintRestoreIcon(painter, center);
        } else {
            paintMaximizeIcon(painter, center);
        }
        break;
    case Kind::Close:
        paintCloseIcon(painter, center);
        break;
    }
}

void WindowsWindowControlButton::paintMinimizeIcon(QPainter& painter, const QPointF& center) const
{
    painter.drawLine(QPointF(center.x() - kMinimizeIconHalfWidth, center.y() + kMinimizeIconYOffset),
                     QPointF(center.x() + kMinimizeIconHalfWidth, center.y() + kMinimizeIconYOffset));
}

void WindowsWindowControlButton::paintMaximizeIcon(QPainter& painter, const QPointF& center) const
{
    const qreal halfSize = kMaximizeIconSize / 2.0;
    painter.drawRect(QRectF(center.x() - halfSize, center.y() - halfSize,
                            kMaximizeIconSize, kMaximizeIconSize));
}

void WindowsWindowControlButton::paintRestoreIcon(QPainter& painter, const QPointF& center) const
{
    const qreal halfSize = kRestoreIconSize / 2.0;
    const QRectF backRect(center.x() - halfSize + kRestoreIconOffset,
                          center.y() - halfSize - kRestoreIconOffset,
                          kRestoreIconSize,
                          kRestoreIconSize);
    const QRectF frontRect(center.x() - halfSize - kRestoreIconOffset,
                           center.y() - halfSize + kRestoreIconOffset,
                           kRestoreIconSize,
                           kRestoreIconSize);
    QPainterPath path;
    path.addRect(backRect);
    path.addRect(frontRect);
    painter.drawPath(path);
}

void WindowsWindowControlButton::paintCloseIcon(QPainter& painter, const QPointF& center) const
{
    painter.drawLine(QPointF(center.x() - kCloseIconRadius, center.y() - kCloseIconRadius),
                     QPointF(center.x() + kCloseIconRadius, center.y() + kCloseIconRadius));
    painter.drawLine(QPointF(center.x() + kCloseIconRadius, center.y() - kCloseIconRadius),
                     QPointF(center.x() - kCloseIconRadius, center.y() + kCloseIconRadius));
}
