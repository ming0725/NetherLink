#pragma once

#include <QColor>
#include <QRect>

class QPainter;

namespace LoadingSpinnerRenderer {

void drawCircularSpinner(QPainter* painter,
                         const QRect& rect,
                         const QColor& color,
                         qreal progress);

} // namespace LoadingSpinnerRenderer
