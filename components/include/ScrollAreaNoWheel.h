
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QScrollArea>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ScrollAreaNoWheel : public QScrollArea {
    Q_OBJECT

    public:
        explicit ScrollAreaNoWheel(QWidget*parent = nullptr);

    protected:
        void wheelEvent(QWheelEvent*event) override;

};
