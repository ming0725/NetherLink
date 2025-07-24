
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "FramelessWindow.h"

#include <QApplication>
#include <QSplitter>
#include <QStackedWidget>

#include "ApplicationBar.h"
#include <QPushButton>

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
