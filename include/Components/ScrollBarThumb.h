/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_SCROLL_BAR_THUMB
#define INCLUDE_COMPONENTS_SCROLL_BAR_THUMB

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ScrollBarThumb : public QWidget {
    Q_OBJECT

    public:
        explicit ScrollBarThumb(QWidget*parent = nullptr);

        void setColor(const QColor &newColor);

    protected:
        void paintEvent(QPaintEvent*event) override;

    private:
        QColor color;
};

#endif /* INCLUDE_COMPONENTS_SCROLL_BAR_THUMB */
