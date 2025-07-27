/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_DEFAULT_PAGE
#define INCLUDE_VIEW_MAINWINDOW_DEFAULT_PAGE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class DefaultPage : public QWidget {
    Q_OBJECT

    public:
        explicit DefaultPage(QWidget*parent = nullptr);

        void setImageSize(const QSize size); // 设置图片显示的固定大小

    protected:
        void paintEvent(QPaintEvent*event) override;

    private:
        QPixmap m_pixmap;
        QSize m_displaySize; // 固定显示区域大小
};

#endif /* INCLUDE_VIEW_MAINWINDOW_DEFAULT_PAGE */
