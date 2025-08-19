#include "Components/CustomPushButton.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

CustomPushButton::CustomPushButton(QWidget* parent)
        : QPushButton(parent)
        , m_radius(8)
        , m_normalColor(0x0099ff)
        , m_hoverColor(0x0088ee)
        , m_pressColor(0x0077dd)
        , m_disabledColor(0xa0a0a0)
        , m_currentColor(m_normalColor)
        , m_textColor(Qt::white)
        , m_borderColor(Qt::transparent)
        , m_borderWidth(0)
        , m_isFlat(false)
        , m_animationDuration(200)
        , m_isHovered(false)
        , m_isPressed(false)
        , m_colorAnimation(new QPropertyAnimation(this, "currentColor"))
{
    initializeButton();
}

CustomPushButton::CustomPushButton(const QString& text, QWidget* parent)
        : QPushButton(text, parent)
        , m_radius(8)
        , m_normalColor(0x0099ff)
        , m_hoverColor(0x0088ee)
        , m_pressColor(0x0077dd)
        , m_disabledColor(0xa0a0a0)
        , m_currentColor(m_normalColor)
        , m_textColor(Qt::white)
        , m_borderColor(Qt::transparent)
        , m_borderWidth(0)
        , m_isFlat(false)
        , m_animationDuration(200)
        , m_isHovered(false)
        , m_isPressed(false)
        , m_colorAnimation(new QPropertyAnimation(this, "currentColor"))
{
    initializeButton();
}

CustomPushButton::~CustomPushButton()
{
    delete m_colorAnimation;
}

void CustomPushButton::initializeButton()
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);

    m_colorAnimation->setDuration(m_animationDuration);
    m_colorAnimation->setEasingCurve(QEasingCurve::OutCubic);

    applyStateColor();
}

void CustomPushButton::applyStateColor()
{
    QColor target;
    if (!isEnabled()) {
        target = m_disabledColor;
    } else if (m_isPressed) {
        target = m_pressColor;
    } else if (m_isHovered) {
        target = m_hoverColor;
    } else {
        target = m_normalColor;
    }

    m_colorAnimation->stop();
    m_colorAnimation->setStartValue(m_currentColor);
    m_colorAnimation->setEndValue(target);
    m_colorAnimation->start();
}


int CustomPushButton::radius() const { return m_radius; }
void CustomPushButton::setRadius(int r) { m_radius = r; update(); }

QColor CustomPushButton::normalColor() const { return m_normalColor; }
void CustomPushButton::setNormalColor(const QColor& c) { m_normalColor = c; applyStateColor();}

QColor CustomPushButton::hoverColor() const { return m_hoverColor; }
void CustomPushButton::setHoverColor(const QColor& c) { m_hoverColor = c; }

QColor CustomPushButton::pressColor() const { return m_pressColor; }
void CustomPushButton::setPressColor(const QColor& c) { m_pressColor = c; }

QColor CustomPushButton::disabledColor() const { return m_disabledColor; }
void CustomPushButton::setDisabledColor(const QColor& c) { m_disabledColor = c; }

QColor CustomPushButton::currentColor() const { return m_currentColor; }
void CustomPushButton::setCurrentColor(const QColor& c)
{
    m_currentColor = c;
    update();
}

QColor CustomPushButton::textColor() const { return m_textColor; }
void CustomPushButton::setTextColor(const QColor& c) { m_textColor = c; update(); }

QColor CustomPushButton::borderColor() const { return m_borderColor; }
void CustomPushButton::setBorderColor(const QColor& c) { m_borderColor = c; update(); }

int CustomPushButton::borderWidth() const { return m_borderWidth; }
void CustomPushButton::setBorderWidth(int w) { m_borderWidth = w; update(); }

bool CustomPushButton::isFlat() const { return m_isFlat; }
void CustomPushButton::setFlat(bool f) { m_isFlat = f; update(); }

void CustomPushButton::setAnimationDuration(int ms)
{
    m_animationDuration = ms;
    m_colorAnimation->setDuration(ms);
}
int CustomPushButton::animationDuration() const { return m_animationDuration; }


void CustomPushButton::setDefaultStyle() {
    setNormalColor(0xE0E0E0); setHoverColor(0xD0D0D0);
    setPressColor(0xC0C0C0); setTextColor(0x333333);
}

void CustomPushButton::setPrimaryStyle() {
    setNormalColor(0x0099FF); setHoverColor(0x0088EE);
    setPressColor(0x0077DD); setTextColor(0xFFFFFF);
}

void CustomPushButton::setSuccessStyle() {
    setNormalColor(0x28A745); setHoverColor(0x218838);
    setPressColor(0x1E7E34); setTextColor(0xFFFFFF);
}

void CustomPushButton::setWarningStyle() {
    setNormalColor(0xFFC107); setHoverColor(0xE0A800);
    setPressColor(0xD39E00); setTextColor(0x333333);
}

void CustomPushButton::setDangerStyle() {
    setNormalColor(0xDC3545); setHoverColor(0xC82333);
    setPressColor(0xBD2130); setTextColor(0xFFFFFF);
}


void CustomPushButton::enterEvent(QEnterEvent* e)
{
    if (!isEnabled()) return;
    m_isHovered = true;
    applyStateColor();
    QPushButton::enterEvent(e);
}

void CustomPushButton::leaveEvent(QEvent* e)
{
    if (!isEnabled()) return;
    m_isHovered = false;
    m_isPressed = false;
    applyStateColor();
    QPushButton::leaveEvent(e);
}

void CustomPushButton::mousePressEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    if (e->button() == Qt::LeftButton) {
        m_isPressed = true;
        applyStateColor();
    }
    QPushButton::mousePressEvent(e);
}

void CustomPushButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    if (e->button() == Qt::LeftButton) {
        m_isPressed = false;
        applyStateColor();
    }
    QPushButton::mouseReleaseEvent(e);
}

void CustomPushButton::paintEvent(QPaintEvent* e)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), m_radius, m_radius);

    if (!m_isFlat)
        p.fillPath(path, m_currentColor);

    if (m_borderWidth > 0) {
        p.setPen(QPen(m_borderColor, m_borderWidth));
        p.drawPath(path);
    }

    p.setPen(m_textColor);
    p.drawText(rect(), Qt::AlignCenter, text());
}

bool CustomPushButton::event(QEvent* e)
{
    if (e->type() == QEvent::EnabledChange) {
        m_isHovered = false;
        m_isPressed = false;
        applyStateColor();
    }
    return QPushButton::event(e);
}
