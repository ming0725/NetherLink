
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "ScrollAreaNoWheel.h"

#include <QWheelEvent>

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

ScrollAreaNoWheel::ScrollAreaNoWheel(QWidget*parent) : QScrollArea(parent) {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ScrollAreaNoWheel::wheelEvent(QWheelEvent*event) {
    event->ignore();
}
