#include "PlusButton.h"

#include <QPainter>
#include <QPaintEvent>

namespace {

constexpr qreal kPlusHalfLength = 4.2;
constexpr qreal kPlusStrokeWidth = 1.6;

} // namespace

PlusButton::PlusButton(QWidget* parent)
        : StatefulPushButton(parent)
{
    setText(QString());
}

void PlusButton::paintEvent(QPaintEvent* event)
{
    StatefulPushButton::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center = QRectF(rect()).center();
    QPen pen(textColor(), kPlusStrokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawLine(QPointF(center.x() - kPlusHalfLength, center.y()),
                     QPointF(center.x() + kPlusHalfLength, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - kPlusHalfLength),
                     QPointF(center.x(), center.y() + kPlusHalfLength));
}
