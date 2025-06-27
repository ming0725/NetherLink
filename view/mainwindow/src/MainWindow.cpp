#include "MainWindow.h"
#include "WindowEffect.h"
#include "MessageApplication.h"
#include "AiChatApplication.h"
#include "PostApplication.h"
#include <QScreen>
#include <QGuiApplication>
#include <QPainterPath>
#include <QJsonDocument>
#include <QJsonObject>
#include "CurrentUser.h"

QWidget* MainWindow::instance = nullptr;

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
    , stack(new QStackedWidget(this))
{
    // 窗口基础设置
    resize(950, 650);
    setMinimumHeight(525);
    setMinimumWidth(685);
    setAttribute(Qt::WA_TranslucentBackground);
    HWND hwnd = HWND(winId());
    WindowEffect w;
    w.setAeroEffect(hwnd);
    appBar = new ApplicationBar(this);
    appBar->setFixedWidth(54);

    // 窗口控制按钮
    btnMinimize = new QPushButton(this);
    btnMaximize = new QPushButton(this);
    btnClose    = new QPushButton(this);
    iconClose       = QIcon(":/resources/icon/close.png");
    iconCloseHover  = QIcon(":/resources/icon/hovered_close.png");
    btnClose->installEventFilter(this);
    btnMinimize->setIcon(QIcon(":/resources/icon/minimize.png"));
    btnMaximize->setIcon(QIcon(":/resources/icon/maximize.png"));
    btnClose->setIcon(QIcon(":/resources/icon/close.png"));

    btnMinimize->setIconSize(QSize(16, 16));
    btnMaximize->setIconSize(QSize(16, 16));
    btnClose->setIconSize(QSize(16, 16));

    auto btnStyle = R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #E9E9E9;
        }
    )";
    btnMinimize->setStyleSheet(btnStyle);
    btnMaximize->setStyleSheet(btnStyle);
    btnClose->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #C42B1C;
        }
    )");

    int btnSize = 32;
    btnMinimize->setFixedSize(btnSize, btnSize);
    btnMaximize->setFixedSize(btnSize, btnSize);
    btnClose->setFixedSize(btnSize, btnSize);
    stack->addWidget(new MessageApplication(this));
    stack->addWidget(new FriendApplication(this));
    stack->addWidget(new PostApplication(this));
    stack->addWidget(new AiChatApplication(this));
    stack->addWidget(new DefaultPage(this));
    // 默认选中第一个
    stack->setCurrentIndex(0);

    // 绑定点击信号，切换栈页
    connect(appBar, &ApplicationBar::applicationClicked,
            this, &MainWindow::onBarItemClicked);

    // 信号连接
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, [=](){
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect   sg     = screen->geometry();
    int     cx     = (sg.width()  - width())  / 2;
    int     cy     = (sg.height() - height()) / 2;
    move(cx, cy);

}


void MainWindow::resizeEvent(QResizeEvent* event)
{
    FramelessWindow::resizeEvent(event);
    int w = width(), h = height();
    int barW = appBar->width();
    appBar->setGeometry(0, 0, barW, h);
    stack->setGeometry(barW, 0, w - barW, h);
    int bw = btnMinimize->width();
    btnClose->move(w - bw,    0);
    btnMaximize->move(w - bw*2,0);
    btnMinimize->move(w - bw*3,0);
}


void MainWindow::mousePressEvent(QMouseEvent* event)
{
    QWidget* fw = QApplication::focusWidget();
    if (qobject_cast<LineEditComponent*>(fw)
        || qobject_cast<QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    FramelessWindow::mousePressEvent(event);
}


void MainWindow::onBarItemClicked(ApplicationBarItem *item)
{
    int idx = appBar->indexOfTopItem(item);
    if (idx >= 0 && idx < stack->count()) {
        stack->setCurrentIndex(idx);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (watched == btnClose) {
        if (ev->type() == QEvent::Enter) {
            btnClose->setIcon(iconCloseHover);
        }
        else if (ev->type() == QEvent::Leave) {
            btnClose->setIcon(iconClose);
        }
    }
    return FramelessWindow::eventFilter(watched, ev);
}

QWidget *MainWindow::getInstance() {
    if (!instance) {
        instance = new MainWindow();
    }
    return instance;
}

