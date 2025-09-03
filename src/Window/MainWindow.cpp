/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Window/MainWindow.hpp"

#include <QMouseEvent>
#include <QPainterPath>
#include <QScreen>

#include "ui_MainWindow.h"
#include "View/AiChat/AiChatApplication.h"
#include "View/Chat/MessageApplication.h"
#include "View/Friend/FriendApplication.h"

#include "View/Post/PostApplication.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
    QWidget* MainWindow::instance = nullptr;

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

    MainWindow::MainWindow(QWidget* parent) : QWidget(parent), ui(new Ui::MainWindow) {
        ui->setupUi(this);
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        界面_关闭按钮 = ui->btn_close;
        界面_导航栏 = ui->NavBar;
        界面_容器 = ui->RoundStackedWidget;
        connect(界面_关闭按钮, &QPushButton::clicked, this, [=, this] {
            this->close();
        });
        界面_容器->insertWidget(PAGE_MESSAGE, new MessageApplication(this));
        界面_容器->insertWidget(PAGE_FRIEND, new FriendApplication(this));
        界面_容器->insertWidget(PAGE_POST, new PostApplication(this));
        界面_容器->insertWidget(PAGE_AI, new AiChatApplication(this));

        // 界面_容器->addWidget(new DefaultPage(this));
        //// 默认选中第一个
        界面_容器->setCurrentIndex(PAGE_MESSAGE);
        connect(界面_导航栏, &NavBar::sig_Chat, this, [=, this]() {
            界面_容器->setCurrentIndex(PAGE_MESSAGE);
        });
        connect(界面_导航栏, &NavBar::sig_Friend, this, [=, this]() {
            界面_容器->setCurrentIndex(PAGE_FRIEND);
        });
        connect(界面_导航栏, &NavBar::sig_Post, this, [=, this]() {
            界面_容器->setCurrentIndex(PAGE_POST);
        });
        connect(界面_导航栏, &NavBar::sig_AI, this, [=, this]() {
            界面_容器->setCurrentIndex(PAGE_AI);
        });

        // QPixmap 默认头像(":/icon/icon.png");
        // QPixmap 圆角头像 = Util::RoundedPixmap::函数_圆角头像(默认头像, 60);
        // ui->btn_profile->setIcon(圆角头像);
        //// 窗口基础设置
        // resize(950, 650);
        // setAttribute(Qt::WA_TranslucentBackground);
        // appBar = new ApplicationBar(this);
        //// appBar->setFixedWidth(54);
        //// 窗口控制按钮
        // btnMinimize = new QPushButton(this);
        // btnMaximize = new QPushButton(this);
        // btnClose = new QPushButton(this);
        //// btnClose->installEventFilter(this);
        //// btnMinimize->setIcon(QIcon(":/icon/minimize.png"));
        //// btnMaximize->setIcon(QIcon(":/icon/maximize.png"));
        //// btnClose->setIcon(QIcon(":/icon/close.png"));
        //// btnMinimize->setIconSize(QSize(16, 16));
        //// btnMaximize->setIconSize(QSize(16, 16));
        //// btnClose->setIconSize(QSize(16, 16));
        // auto btnStyle = R"(
        // QPushButton {
        // background-color: transparent;
        // border: none;
        // }
        // QPushButton:hover {
        // background-color: #E9E9E9;
        // }
        // )";
        // btnMinimize->setStyleSheet(btnStyle);
        // btnMaximize->setStyleSheet(btnStyle);
        // btnClose->setStyleSheet(R"(
        // QPushButton {
        // background-color: transparent;
        // border: none;
        // }
        // QPushButton:hover {
        // background-color: #C42B1C;
        // }
        // )");
        // int btnSize = 32;
        // btnMinimize->setFixedSize(btnSize, btnSize);
        // btnMaximize->setFixedSize(btnSize, btnSize);
        // btnClose->setFixedSize(btnSize, btnSize);
        //// 绑定点击信号，切换栈页
        // connect(appBar, &ApplicationBar::applicationClicked, this, &MainWindow::onBarItemClicked);
        //// 信号连接
        // connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
        // connect(btnMaximize, &QPushButton::clicked, this, [=, this]() {
        // if (isMaximized()) {
        // showNormal();
        // } else {
        // showMaximized();
        // }
        // });
        // connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
        // QScreen* screen = QGuiApplication::primaryScreen();
        // QRect sg = screen->geometry();
        // int cx = (sg.width() - width()) / 2;
        // int cy = (sg.height() - height()) / 2;
        // move(cx, cy);
    }

    MainWindow::~MainWindow() {
        delete ui;
    }

    void MainWindow::paintEvent(QPaintEvent* event) {
        QWidget::paintEvent(event);

        QPainter painter(this);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        QColor bgColor(Qt::white);
        QPainterPath path;

        path.addRoundedRect(rect(), 20, 20);
        painter.fillPath(path, bgColor);
    }

    void MainWindow::mousePressEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->button() == Qt::LeftButton)
            成员变量_鼠标偏移量 = (形参_鼠标事件->globalPosition() - frameGeometry().topLeft()).toPoint();
        QWidget::mousePressEvent(形参_鼠标事件);
    }

    void MainWindow::mouseMoveEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->buttons() & Qt::LeftButton)
            move((形参_鼠标事件->globalPosition() - 成员变量_鼠标偏移量).toPoint());
        QWidget::mouseMoveEvent(形参_鼠标事件);
    }

// void MainWindow::onBarItemClicked(ApplicationBarItem* item) {

// int idx = appBar->indexOfTopItem(item);

// if ((idx >= 0) && (idx < stack->count())) {

// stack->setCurrentIndex(idx);

// }

// }

//// bool MainWindow::eventFilter(QObject* watched, QEvent* ev) {

//// if (watched == btnClose) {

//// if (ev->type() == QEvent::Enter) {

//// btnClose->setIcon(iconCloseHover);

//// } else if (ev->type() == QEvent::Leave) {

//// btnClose->setIcon(iconClose);

//// }

//// }

////// return (eventFilter(watched, ev));

//// }

    QWidget* MainWindow::getInstance() {
        if (!instance) {
            instance = new MainWindow();
        }
        return (instance);
    }
}
