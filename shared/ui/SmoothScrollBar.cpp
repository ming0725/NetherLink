#include "SmoothScrollBar.h"
#include "shared/theme/ThemeManager.h"
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>

SmoothScrollBar::SmoothScrollBar(QWidget *parent)
    : QWidget(parent)
    , m_minimum(0)
    , m_maximum(0)
    , m_pageStep(0)
    , m_value(0)
    , m_orientation(Qt::Vertical)
    , m_opacity(0.0)
    , m_isDragging(false)
{
    setFixedWidth(8);  // 设置滚动条宽度
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    // 初始化淡入淡出动画
    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setDuration(200);  // 加快动画速度
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        updateVisibility();
    });
    
    // 初始化淡出计时器
    m_fadeOutTimer = new QTimer(this);
    m_fadeOutTimer->setSingleShot(true);
    connect(m_fadeOutTimer, &QTimer::timeout, this, &SmoothScrollBar::startFadeOut);
}

void SmoothScrollBar::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) {
        return;
    }

    m_orientation = orientation;
    if (m_orientation == Qt::Horizontal) {
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
        setFixedHeight(8);
    } else {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setFixedWidth(8);
    }
    update();
}

void SmoothScrollBar::setRange(int min, int max)
{
    const bool changed = (m_minimum != min || m_maximum != max);
    m_minimum = min;
    m_maximum = max;
    if (!needsDisplay()) {
        m_value = min;
        m_opacity = 0.0;
        hide();
    }
    if (changed && m_isDragging && !m_updatingFromDrag) {
        rebaseDragAnchor();
    }
    update();
}

void SmoothScrollBar::setPageStep(int step)
{
    const bool changed = (m_pageStep != step);
    m_pageStep = step;
    if (changed && m_isDragging && !m_updatingFromDrag) {
        rebaseDragAnchor();
    }
    update();
}

void SmoothScrollBar::setValue(int value)
{
    value = qBound(m_minimum, value, m_maximum);
    if (m_value != value) {
        m_value = value;
        if (m_isDragging && !m_updatingFromDrag) {
            rebaseDragAnchor();
        }
        emit valueChanged(value);
        update();
    }
}

void SmoothScrollBar::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        updateVisibility();
        update();
    }
}

void SmoothScrollBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (!needsDisplay()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 只绘制滚动条滑块，不绘制背景轨道
    painter.setOpacity(m_opacity);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::ScrollThumb));
    painter.drawRoundedRect(getHandleRect(), 4, 4);
}

QRect SmoothScrollBar::getHandleRect() const
{
    if (!needsDisplay()) return QRect();

    const qreal visibleRatio = qreal(m_pageStep) / (m_maximum - m_minimum + m_pageStep);
    const qreal valueRatio = qreal(m_value - m_minimum) / (m_maximum - m_minimum);

    if (m_orientation == Qt::Horizontal) {
        const int handleWidth = qMax(static_cast<int>(width() * visibleRatio), 30);
        const int handleX = valueRatio * (width() - handleWidth);
        return QRect(handleX, 0, handleWidth, height());
    }

    const int handleHeight = qMax(static_cast<int>(height() * visibleRatio), 30);
    const int handleY = valueRatio * (height() - handleHeight);
    return QRect(0, handleY, width(), handleHeight);
}

void SmoothScrollBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && needsDisplay()) {
        m_isDragging = true;
        m_dragStartPosition = event->pos();
        m_dragStartValue = m_value;
        updateValue(event->pos());
    }
}

void SmoothScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        updateValue(event->pos());
    }
}

void SmoothScrollBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_isDragging = false;
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        startFadeOut();  // 如果鼠标不在滚动条区域内，立即开始淡出
    }
}

void SmoothScrollBar::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    showScrollBar();
    m_fadeOutTimer->stop();  // 停止任何正在进行的淡出计时
}

void SmoothScrollBar::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (!m_isDragging) {
        startFadeOut();  // 立即开始淡出
    }
}

void SmoothScrollBar::updateValue(const QPoint &pos)
{
    if (!needsDisplay()) return;

    QRect handleRect = getHandleRect();
    const int trackLength = m_orientation == Qt::Horizontal
            ? width() - handleRect.width()
            : height() - handleRect.height();
    if (trackLength <= 0) {
        return;
    }

    const int pointerDelta = m_orientation == Qt::Horizontal
            ? pos.x() - m_dragStartPosition.x()
            : pos.y() - m_dragStartPosition.y();
    const qreal valueRatio = qreal(pointerDelta) / trackLength;
    const int valueDelta = valueRatio * (m_maximum - m_minimum);
    m_updatingFromDrag = true;
    setValue(m_dragStartValue + valueDelta);
    m_updatingFromDrag = false;
}

void SmoothScrollBar::showScrollBar()
{
    if (!needsDisplay()) {
        hide();
        return;
    }

    show();
    m_fadeOutTimer->stop();
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void SmoothScrollBar::startFadeOut()
{
    if (!m_isDragging) {
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(m_opacity);
        m_fadeAnimation->setEndValue(0.0);
        m_fadeAnimation->start();
    }
}

void SmoothScrollBar::updateVisibility()
{
    const bool visible = needsDisplay() && (m_opacity > 0.0 || m_isDragging);
    setAttribute(Qt::WA_TransparentForMouseEvents, !visible);
    setVisible(visible);
}

void SmoothScrollBar::rebaseDragAnchor()
{
    m_dragStartPosition = mapFromGlobal(QCursor::pos());
    m_dragStartValue = m_value;
}
