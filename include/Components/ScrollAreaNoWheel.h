/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_SCROLL_AREA_NO_WHEEL

#define INCLUDE_COMPONENTS_SCROLL_AREA_NO_WHEEL

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QScrollArea>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ScrollAreaNoWheel : public QScrollArea {
    Q_OBJECT

    public:
        explicit ScrollAreaNoWheel(QWidget* parent = nullptr);

    protected:
        void wheelEvent(QWheelEvent* event) override;
};

#endif /* INCLUDE_COMPONENTS_SCROLL_AREA_NO_WHEEL */
