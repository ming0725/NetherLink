/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_MAIN_WINDOW
#define INCLUDE_VIEW_MAINWINDOW_MAIN_WINDOW


/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>

#include "Utils/FramelessWindow.h"
#include "View/Mainwindow/ApplicationBar.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MainWindow : public FramelessWindow {
    public:
        MainWindow(QWidget*parent = nullptr);

        static QWidget* getInstance();

    protected:
        void resizeEvent(QResizeEvent*event) override;

        void mousePressEvent(QMouseEvent*event) override;

        bool eventFilter(QObject* watched, QEvent* ev);

    private slots:
        void onBarItemClicked(ApplicationBarItem* item);

    private:
        static QWidget* instance;
        ApplicationBar*appBar;
        QWidget*rightContent;
        int contentFixedWidth;
        QSplitter*splitter;
        QPushButton*btnMinimize;
        QPushButton*btnMaximize;
        QPushButton*btnClose;
        QStackedWidget* stack;
        QIcon iconClose, iconCloseHover;
};


#endif /* INCLUDE_VIEW_MAINWINDOW_MAIN_WINDOW */
