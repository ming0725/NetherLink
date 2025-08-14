/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QScreen>

#include "Util/WindowEffect.h"
#include "View/AiChat/AiChatApplication.h"
#include "View/Chat/MessageApplication.h"
#include "View/Friend/FriendApplication.h"
#include "View/Mainwindow/MainWindow.h"
#include "View/Post/PostApplication.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
QWidget* MainWindow::instance = nullptr;

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

MainWindow::MainWindow(QWidget* parent) : FramelessWindow(parent), stack(new QStackedWidget(this)) {
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
    btnClose = new QPushButton(this);
    iconClose = QIcon(":/icon/close.png");
    iconCloseHover = QIcon(":/icon/hovered_close.png");
    btnClose->installEventFilter(this);
    btnMinimize->setIcon(QIcon(":/icon/minimize.png"));
    btnMaximize->setIcon(QIcon(":/icon/maximize.png"));
    btnClose->setIcon(QIcon(":/icon/close.png"));
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
    connect(appBar, &ApplicationBar::applicationClicked, this, &MainWindow::onBarItemClicked);

    // 信号连接
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, [=, this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect sg = screen->geometry();
    int cx = (sg.width() - width()) / 2;
    int cy = (sg.height() - height()) / 2;

    move(cx, cy);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    FramelessWindow::resizeEvent(event);

    int w = width(), h = height();
    int barW = appBar->width();

    appBar->setGeometry(0, 0, barW, h);
    stack->setGeometry(barW, 0, w - barW, h);

    int bw = btnMinimize->width();

    btnClose->move(w - bw, 0);
    btnMaximize->move(w - bw * 2, 0);
    btnMinimize->move(w - bw * 3, 0);
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    QWidget* fw = QApplication::focusWidget();

    if (qobject_cast <LineEditComponent*>(fw) || qobject_cast <QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    FramelessWindow::mousePressEvent(event);
}

void MainWindow::onBarItemClicked(ApplicationBarItem* item) {
    int idx = appBar->indexOfTopItem(item);

    if ((idx >= 0) && (idx < stack->count())) {
        stack->setCurrentIndex(idx);
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* ev) {
    if (watched == btnClose) {
        if (ev->type() == QEvent::Enter) {
            btnClose->setIcon(iconCloseHover);
        } else if (ev->type() == QEvent::Leave) {
            btnClose->setIcon(iconClose);
        }
    }
    return (FramelessWindow::eventFilter(watched, ev));
}

QWidget* MainWindow::getInstance() {
    if (!instance) {
        instance = new MainWindow();
    }
    return (instance);
}
