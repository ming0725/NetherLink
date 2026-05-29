#include "ThemedSelectButton.h"

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

ThemedSelectButton::ThemedSelectButton(QWidget* parent)
    : QToolButton(parent)
{
    setFixedSize(172, 34);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setPopupMode(QToolButton::DelayedPopup);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
}

void ThemedSelectButton::setMenuHoverSuppressed(bool suppressed)
{
    if (m_menuHoverSuppressed == suppressed) {
        return;
    }

    m_menuHoverSuppressed = suppressed;
    update();
}

bool ThemedSelectButton::event(QEvent* event)
{
    if (event->type() == QEvent::Enter) {
        m_hovered = true;
        setMenuHoverSuppressed(false);
        update();
    } else if (event->type() == QEvent::Leave) {
        m_hovered = false;
        setMenuHoverSuppressed(false);
        update();
    } else if (event->type() == QEvent::MouseButtonPress) {
        setMenuHoverSuppressed(false);
    }
    return QToolButton::event(event);
}

void ThemedSelectButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);

    const QColor borderColor = isEnabled()
            ? ThemeManager::instance().color(ThemeColor::Divider)
            : ThemeManager::instance().color(ThemeColor::ListHover);
    const QColor backgroundColor = isEnabled()
            ? ((m_hovered && !m_menuHoverSuppressed)
               ? ThemeManager::instance().color(ThemeColor::ListHover)
               : ThemeManager::instance().color(ThemeColor::InputBackground))
            : ThemeManager::instance().color(ThemeColor::PanelRaisedBackground);
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(backgroundColor);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

    QFont textFont = font();
    textFont.setPixelSize(14);
    painter.setFont(textFont);
    painter.setPen(isEnabled()
                   ? ThemeManager::instance().color(ThemeColor::PrimaryText)
                   : ThemeManager::instance().color(ThemeColor::TertiaryText));
    const QRect textRect = rect().adjusted(14, 0, -34, 0);
    painter.drawText(textRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(textFont).elidedText(text(), Qt::ElideRight, textRect.width()));

    if (!isEnabled()) {
        return;
    }

    const int centerX = width() - 19;
    const int centerY = height() / 2;
    QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                  1.6,
                  Qt::SolidLine,
                  Qt::RoundCap,
                  Qt::RoundJoin);
    painter.setPen(arrowPen);
    painter.drawLine(QPointF(centerX - 4.5, centerY - 2.0), QPointF(centerX, centerY + 2.5));
    painter.drawLine(QPointF(centerX, centerY + 2.5), QPointF(centerX + 4.5, centerY - 2.0));
}
