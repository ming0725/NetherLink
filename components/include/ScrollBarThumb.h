
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

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
