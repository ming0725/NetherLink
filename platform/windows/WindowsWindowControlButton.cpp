#include "WindowsWindowControlButton.h"

#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

WindowsWindowControlButton::WindowsWindowControlButton(Kind kind, QWidget* parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    setFixedSize(38, 32);
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

    const QPointF center = rect().center();
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
    painter.drawLine(QPointF(center.x() - 5.0, center.y() + 3.0),
                     QPointF(center.x() + 5.0, center.y() + 3.0));
}

void WindowsWindowControlButton::paintMaximizeIcon(QPainter& painter, const QPointF& center) const
{
    painter.drawRect(QRectF(center.x() - 4.5, center.y() - 4.5, 9.0, 9.0));
}

void WindowsWindowControlButton::paintRestoreIcon(QPainter& painter, const QPointF& center) const
{
    const QRectF backRect(center.x() - 2.5, center.y() - 5.0, 7.5, 7.5);
    const QRectF frontRect(center.x() - 5.0, center.y() - 2.5, 7.5, 7.5);
    QPainterPath path;
    path.addRect(backRect);
    path.addRect(frontRect);
    painter.drawPath(path);
}

void WindowsWindowControlButton::paintCloseIcon(QPainter& painter, const QPointF& center) const
{
    painter.drawLine(QPointF(center.x() - 4.5, center.y() - 4.5),
                     QPointF(center.x() + 4.5, center.y() + 4.5));
    painter.drawLine(QPointF(center.x() + 4.5, center.y() - 4.5),
                     QPointF(center.x() - 4.5, center.y() + 4.5));
}
