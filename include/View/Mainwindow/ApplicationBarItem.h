/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_APPLICATION_BAR_ITEM
#define INCLUDE_VIEW_MAINWINDOW_APPLICATION_BAR_ITEM


/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPropertyAnimation>
#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ApplicationBarItem : public QWidget {
    Q_OBJECT

    private:
        QPixmap normalPixmap;
        QPixmap selectedPixmap;
        bool hoverd = false;
        bool selected = false;
        qreal pixmapScale = 0.6;
        qreal rippleRadius = 0.0;
        QVariantAnimation* rippleAnim = nullptr;

    public:
        ApplicationBarItem(QPixmap normal, QWidget* parent = nullptr);

        ApplicationBarItem(QPixmap normal, QPixmap selected, QWidget* parent = nullptr);

        void setPixmapScale(qreal scale);

        void setSelected(bool);

        bool isSelected();

    protected:
        void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;

        void leaveEvent(QEvent*) Q_DECL_OVERRIDE;

        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

        void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;

    signals:
        void itemClicked(ApplicationBarItem* item);

};


#endif /* INCLUDE_VIEW_MAINWINDOW_APPLICATION_BAR_ITEM */
