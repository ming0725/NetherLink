/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>

#include "Components/ScrollBarThumb.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

ScrollBarThumb::ScrollBarThumb(QWidget*parent) : QWidget(parent), color(QColor(0x3f, 0x3f, 0x3f)) {}

void ScrollBarThumb::setColor(const QColor &newColor) {
    color = newColor;
}

void ScrollBarThumb::paintEvent(QPaintEvent* /* event */) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(0.2);
    painter.setBrush(QBrush(color));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), width() / 2, width() / 2);
}
