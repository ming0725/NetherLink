#include "LoadingSpinnerRenderer.h"

#include <QPainter>
#include <QPen>
#include <QtMath>

namespace LoadingSpinnerRenderer {

void drawCircularSpinner(QPainter* painter,
                         const QRect& rect,
                         const QColor& color,
                         qreal progress)
{
    if (!painter || rect.isEmpty()) {
        return;
    }

    const int side = qMax(4, qMin(rect.width(), rect.height()));
    QRectF bounds(QPointF(0.0, 0.0), QSizeF(side, side));
    bounds.moveCenter(QPointF(rect.center()));
    const qreal penWidth = qBound<qreal>(1.4, side / 7.5, 2.2);
    const qreal inset = penWidth * 0.5 + 0.5;
    bounds.adjust(inset, inset, -inset, -inset);

    const qreal normalizedProgress = progress - qFloor(progress);
    const qreal pulse = (qSin(normalizedProgress * M_PI * 2.0 - M_PI / 2.0) + 1.0) * 0.5;
    const qreal spanDegrees = 92.0 + pulse * 86.0;
    const qreal leadDegrees = normalizedProgress * 360.0;
    QColor trackColor = color;
    trackColor.setAlpha(qRound(color.alpha() * 0.16));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(trackColor, penWidth, Qt::SolidLine, Qt::RoundCap));
    painter->drawEllipse(bounds);

    painter->setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap));
    const int startAngle = qRound((-90.0 - leadDegrees) * 16.0);
    const int spanAngle = qRound(-spanDegrees * 16.0);
    painter->drawArc(bounds, startAngle, spanAngle);
    painter->restore();
}

} // namespace LoadingSpinnerRenderer
