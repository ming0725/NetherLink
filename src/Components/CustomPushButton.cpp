/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "Components/CustomPushButton.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

CustomPushButton::CustomPushButton(QWidget* parent) : QPushButton(parent), m_radius(20), m_backgroundColor(QColor(0x0099ff)), m_hoverColor(QColor(0x0088ee)), m_pressColor(QColor(0x0077dd)), m_textColor(Qt::white), m_borderColor(Qt::transparent), m_borderWidth(0), m_isFlat(false), m_animationDuration(250), m_isHovered(false), m_isPressed(false), m_backgroundAnimation(nullptr) {
    initializeButton();
}

CustomPushButton::CustomPushButton(const QString& text, QWidget* parent) : QPushButton(text, parent), m_radius(20), m_backgroundColor(QColor(0x0099ff)), m_hoverColor(QColor(0x0088ee)), m_pressColor(QColor(0x0077dd)), m_textColor(Qt::white), m_borderColor(Qt::transparent), m_borderWidth(0), m_isFlat(false), m_animationDuration(250), m_isHovered(false), m_isPressed(false), m_backgroundAnimation(nullptr) {
    initializeButton();
}

CustomPushButton::~CustomPushButton() {
    if (m_backgroundAnimation) {
        delete m_backgroundAnimation;
    }
}

void CustomPushButton::initializeButton() {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    m_backgroundAnimation = new QPropertyAnimation(this, "backgroundColor");
    m_backgroundAnimation->setDuration(m_animationDuration);
    m_backgroundAnimation->setEasingCurve(QEasingCurve::OutCubic);
    updateStyle();
}

void CustomPushButton::updateStyle() {
    if (m_backgroundAnimation && (m_backgroundAnimation->state() != QAbstractAnimation::Running)) {
        m_backgroundColor = m_isPressed ? m_pressColor : m_isHovered ? m_hoverColor : property("backgroundColor").value <QColor>();
    }
    update();
}

int CustomPushButton::radius() const {
    return (m_radius);
}

void CustomPushButton::setRadius(int radius) {
    m_radius = radius;
    updateStyle();
}

QColor CustomPushButton::backgroundColor() const {
    return (m_backgroundColor);
}

void CustomPushButton::setBackgroundColor(const QColor& color) {
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        updateStyle();
    }
}

QColor CustomPushButton::hoverColor() const {
    return (m_hoverColor);
}

void CustomPushButton::setHoverColor(const QColor& color) {
    m_hoverColor = color;
    updateStyle();
}

QColor CustomPushButton::pressColor() const {
    return (m_pressColor);
}

void CustomPushButton::setPressColor(const QColor& color) {
    m_pressColor = color;
    updateStyle();
}

QColor CustomPushButton::textColor() const {
    return (m_textColor);
}

void CustomPushButton::setTextColor(const QColor& color) {
    m_textColor = color;
    updateStyle();
}

QColor CustomPushButton::borderColor() const {
    return (m_borderColor);
}

void CustomPushButton::setBorderColor(const QColor& color) {
    m_borderColor = color;
    updateStyle();
}

int CustomPushButton::borderWidth() const {
    return (m_borderWidth);
}

void CustomPushButton::setBorderWidth(int width) {
    m_borderWidth = width;
    updateStyle();
}

bool CustomPushButton::isFlat() const {
    return (m_isFlat);
}

void CustomPushButton::setFlat(bool flat) {
    m_isFlat = flat;
    updateStyle();
}

void CustomPushButton::setAnimationDuration(int duration) {
    m_animationDuration = duration;

    if (m_backgroundAnimation) {
        m_backgroundAnimation->setDuration(duration);
    }
}

int CustomPushButton::animationDuration() const {
    return (m_animationDuration);
}

void CustomPushButton::setDefaultStyle() {
    setCustomStyle(QColor(0xE0E0E0), // background
                   QColor(0xD0D0D0), // hover
                   QColor(0xC0C0C0), // press
                   QColor(0x333333) // text
                   );
}

void CustomPushButton::setPrimaryStyle() {
    setCustomStyle(QColor(0x0099FF), // background
                   QColor(0x0088EE), // hover
                   QColor(0x0077DD), // press
                   QColor(0xFFFFFF) // text
                   );
}

void CustomPushButton::setSuccessStyle() {
    setCustomStyle(QColor(0x28A745), // background
                   QColor(0x218838), // hover
                   QColor(0x1E7E34), // press
                   QColor(0xFFFFFF) // text
                   );
}

void CustomPushButton::setWarningStyle() {
    setCustomStyle(QColor(0xFFC107), // background
                   QColor(0xE0A800), // hover
                   QColor(0xD39E00), // press
                   QColor(0x333333) // text
                   );
}

void CustomPushButton::setDangerStyle() {
    setCustomStyle(QColor(0xDC3545), // background
                   QColor(0xC82333), // hover
                   QColor(0xBD2130), // press
                   QColor(0xFFFFFF) // text
                   );
}

void CustomPushButton::setCustomStyle(const QColor& background, const QColor& hover, const QColor& press, const QColor& text) {
    m_backgroundColor = background;
    m_hoverColor = hover;
    m_pressColor = press;
    m_textColor = text;
    updateStyle();
}

// 事件处理
void CustomPushButton::enterEvent(QEnterEvent* event) {
    m_isHovered = true;

    if (m_backgroundAnimation) {
        m_backgroundAnimation->stop();
        m_backgroundAnimation->setStartValue(m_backgroundColor);
        m_backgroundAnimation->setEndValue(m_hoverColor);
        m_backgroundAnimation->start();
    }
    QPushButton::enterEvent(event);
}

void CustomPushButton::leaveEvent(QEvent* event) {
    m_isHovered = false;

    if (m_backgroundAnimation) {
        m_backgroundAnimation->stop();

        QColor normalColor = m_backgroundAnimation->startValue().value <QColor>();

        m_backgroundAnimation->setStartValue(m_backgroundColor);
        m_backgroundAnimation->setEndValue(normalColor);
        m_backgroundAnimation->start();
    }
    QPushButton::leaveEvent(event);
}

void CustomPushButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isPressed = true;

        if (m_backgroundAnimation) {
            m_backgroundAnimation->stop();
            m_backgroundAnimation->setStartValue(m_backgroundColor);
            m_backgroundAnimation->setEndValue(m_pressColor);
            m_backgroundAnimation->start();
        }
    }
    QPushButton::mousePressEvent(event);
}

void CustomPushButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isPressed = false;

        if (m_backgroundAnimation) {
            m_backgroundAnimation->stop();

            QColor startColor = m_backgroundColor;
            QColor endColor = m_isHovered ? m_hoverColor : m_backgroundColor;

            m_backgroundAnimation->setStartValue(startColor);
            m_backgroundAnimation->setEndValue(endColor);
            m_backgroundAnimation->start();
        }
    }
    QPushButton::mouseReleaseEvent(event);
}

void CustomPushButton::paintEvent(QPaintEvent* /* event */) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;

    path.addRoundedRect(rect(), m_radius, m_radius);

    if (!m_isFlat) {
        painter.fillPath(path, m_backgroundColor);
    }

    if (m_borderWidth > 0) {
        painter.setPen(QPen(m_borderColor, m_borderWidth));
        painter.drawPath(path);
    }
    painter.setPen(m_textColor);
    painter.drawText(rect(), Qt::AlignCenter, text());
}

bool CustomPushButton::event(QEvent* e) {
    switch (e->type()) {
        case QEvent::EnabledChange:
        case QEvent::FontChange:
        case QEvent::StyleChange:
            updateStyle();
            break;
        default:
            break;
    }
    return (QPushButton::event(e));
}
