#pragma once

#include "ApplicationBar.h"
#include "DefaultPage.h"
#include "FramelessWindow.h"
#include "FriendApplication.h"
#include "WindowEffect.h"
#include <QApplication>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>

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
