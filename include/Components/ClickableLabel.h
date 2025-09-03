/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_CLICKABLE_LABEL

#define INCLUDE_COMPONENTS_CLICKABLE_LABEL

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QLabel>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ClickableLabel : public QLabel {
    Q_OBJECT

    public:
        explicit ClickableLabel(QWidget* parent = nullptr);

        /**
         * @brief 设置圆角半径
         *
         * @param r
         */
        void setRadius(int r);

        /**
         * @brief setRoundedPixmap
         *
         * @param r
         */
        void setRoundedPixmap(const QPixmap&, int r = -1);

        /**
         * @brief 获取圆角半径
         *
         * @return 圆角半径
         */
        int getRadius();

    signals:
        void clicked();

        void hovered(bool entering);

    protected:
        void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;

        void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;

        void leaveEvent(QEvent*) Q_DECL_OVERRIDE;

        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

    private:
        int radius = -1;
};

#endif /* INCLUDE_COMPONENTS_CLICKABLE_LABEL */
