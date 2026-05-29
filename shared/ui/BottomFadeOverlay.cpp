#include "BottomFadeOverlay.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QtGlobal>

BottomFadeOverlay::BottomFadeOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
}

void BottomFadeOverlay::setBackgroundRole(ThemeColor role)
{
    if (m_backgroundRole == role) {
        return;
    }

    m_backgroundRole = role;
    update();
}

void BottomFadeOverlay::setFadeHeight(int height)
{
    const int boundedHeight = qMax(0, height);
    if (m_fadeHeight == boundedHeight) {
        return;
    }

    m_fadeHeight = boundedHeight;
    update();
}

void BottomFadeOverlay::setSolidAlpha(int alpha)
{
    const int boundedAlpha = qBound(0, alpha, 255);
    if (m_solidAlpha == boundedAlpha) {
        return;
    }

    m_solidAlpha = boundedAlpha;
    update();
}

void BottomFadeOverlay::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
    const qreal fadeStop = rect().height() > 0
            ? qBound(0.0, static_cast<qreal>(m_fadeHeight) / rect().height(), 1.0)
            : 1.0;
    QColor background = ThemeManager::instance().color(m_backgroundRole);
    background.setAlpha(0);
    gradient.setColorAt(0.0, background);
    background.setAlpha(m_solidAlpha);
    gradient.setColorAt(fadeStop, background);
    gradient.setColorAt(1.0, background);
    painter.fillRect(rect(), gradient);
}
