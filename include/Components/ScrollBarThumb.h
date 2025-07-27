/**
 * @file ScrollBarThumb.h
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-07-26 6 22:41:02
 * @brief 【描述】
 */

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

        /**
         * @brief 【描述】 Set the Color object
         * 
         * @param newColor 【参数注释】 {text}
         */
        void setColor(const QColor &newColor);

    protected:
        /**
         * @brief 【描述】
         *
         * @param event 【参数注释】 {text}
         */
        void paintEvent(QPaintEvent*event) override;

    private:
        QColor color;
};

#endif /* INCLUDE_COMPONENTS_SCROLL_BAR_THUMB */
