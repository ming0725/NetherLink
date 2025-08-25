/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <QPainter>
#include <QScreen>

#include "Data/AvatarLoader.h"
#include "Network/NetworkManager.h"
#include "View/Friend/ApplyWindow.h"
#include "View/Friend/SearchFriendWindow.h"

#include "Util/ToastTip.hpp"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

ApplyWindow::ApplyWindow(Type type, const QString& id, const QString& name, const QString& avatarUrl, QWidget* parent) : QWidget(parent), type(type), targetId(id), targetName(name), targetAvatar(avatarUrl) {
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(WINDOW_MIN_WIDTH);
    setMinimumHeight(WINDOW_MIN_HEIGHT);
    initUI();
    setupConnections();
    adjustSize();

    // 设置窗口位置到屏幕中央
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect sg = screen->geometry();
    int cx = (sg.width() - width()) / 2;
    int cy = (sg.height() - height()) / 2;

    move(cx, cy);
}

ApplyWindow::~ApplyWindow() {
    // 析构函数为空，Qt会自动处理子控件的内存释放
}

void ApplyWindow::initUI() {
    // 创建标题标签
    titleLabel = new QLabel((type == User) ? "添加好友" : "加入群聊", this);

    QFont titleFont;

    titleFont.setPixelSize(14);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    // 创建关闭按钮
    btnClose = new QPushButton(this);
    iconClose = QIcon(":/icon/close.png");
    iconCloseHover = QIcon(":/icon/hovered_close.png");
    btnClose->setIcon(iconClose);
    btnClose->setIconSize(QSize(16, 16));
    btnClose->setFixedSize(BTN_SIZE, BTN_SIZE);
    btnClose->installEventFilter(this);
    btnClose->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #C42B1C;
        }
    )");

    // 创建头像标签
    avatarLabel = new AvatarLabel(this);
    avatarLabel->setAvatarSize(AVATAR_SIZE);
    avatarLabel->loadAvatar(targetId, QUrl(targetAvatar), type == Group);

    // 创建名称标签
    nameLabel = new QLabel(targetName, this);

    QFont nameFont;

    nameFont.setPixelSize(16);
    nameLabel->setFont(nameFont);

    // 创建消息输入框
    messageEdit = new QTextEdit(this);
    messageEdit->setPlaceholderText("请输入验证信息");
    messageEdit->setFixedHeight(MESSAGE_HEIGHT);
    messageEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #F5F5F5;
            border: 1px solid #E0E0E0;
            border-radius: 4px;
            padding: 8px;
        }
        QTextEdit:focus {
            border: 1px solid #0099FF;
        }
    )");

    // 创建按钮
    btnSend = new QPushButton((type == User) ? "添加" : "加入", this);
    btnCancel = new QPushButton("取消", this);
    btnSend->setFixedSize(BTN_WIDTH, BTN_HEIGHT);
    btnCancel->setFixedSize(BTN_WIDTH, BTN_HEIGHT);
    btnSend->setStyleSheet(R"(
        QPushButton {
            background-color: #0099FF;
            border: none;
            border-radius: 4px;
            color: white;
        }
        QPushButton:hover {
            background-color: #0088E8;
        }
        QPushButton:pressed {
            background-color: #0077CC;
        }
    )");
    btnCancel->setStyleSheet(R"(
        QPushButton {
            background-color: #F5F5F5;
            border: none;
            border-radius: 4px;
            color: #333333;
        }
        QPushButton:hover {
            background-color: #EBEBEB;
        }
        QPushButton:pressed {
            background-color: #D7D7D7;
        }
    )");
}

void ApplyWindow::setupConnections() {
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnCancel, &QPushButton::clicked, this, &ApplyWindow::onCancelClicked);
    connect(btnSend, &QPushButton::clicked, this, &ApplyWindow::onSendClicked);

    // 添加好友请求响应的处理
    connect(&NetworkManager::instance(), &NetworkManager::friendRequestResponse, this, [=, this](bool success, const QString& message) {
        // 重新启用发送按钮
        btnSend->setEnabled(true);

        if (success) {
            // 成功时在父窗口显示成功提示
            auto* searchFriendWindow = SearchFriendWindow::getInstance();

            if (searchFriendWindow) {
                searchFriendWindow->show();
                searchFriendWindow->raise();
                searchFriendWindow->activateWindow();
                Util::ToastTip::函数_实例().函数_显示消息(searchFriendWindow, Util::ToastTip::枚举_消息类型::ENUM_SUCCESS, message);
            }
            close();
        } else {
            // 失败时在当前窗口显示错误提示
            Util::ToastTip::函数_实例().函数_显示消息(this, Util::ToastTip::枚举_消息类型::ENUM_ERROR, message);
        }
    });
}

void ApplyWindow::resizeEvent(QResizeEvent*) {
    // FramelessWindow::resizeEvent(event);
    int w = width();

    // 设置标题和关闭按钮位置
    titleLabel->setGeometry(0, 0, w, TITLE_HEIGHT);
    btnClose->move(w - BTN_SIZE, 0);

    // 设置头像位置
    int avatarX = (w - AVATAR_SIZE) / 2;

    avatarLabel->move(avatarX, TITLE_HEIGHT + MARGIN);

    // 设置名称标签位置
    int nameY = avatarLabel->y() + AVATAR_SIZE + MARGIN / 2;

    nameLabel->setGeometry(MARGIN, nameY, w - 2 * MARGIN, 24);
    nameLabel->setAlignment(Qt::AlignCenter);

    // 设置消息输入框位置
    int messageY = nameY + 24 + MARGIN;

    messageEdit->setGeometry(MARGIN, messageY, w - 2 * MARGIN, MESSAGE_HEIGHT);

    // 设置按钮位置（右下角，发送按钮在左边）
    int buttonY = messageY + MESSAGE_HEIGHT + MARGIN;
    int spacing = 10;

    btnSend->move(w - 2 * BTN_WIDTH - spacing - MARGIN, buttonY);
    btnCancel->move(w - BTN_WIDTH - MARGIN, buttonY);

    // 调整窗口高度以适应内容
    int contentHeight = buttonY + BTN_HEIGHT + MARGIN;

    if (height() != contentHeight) {
        resize(w, contentHeight);
    }
}

void ApplyWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制白色背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(rect());
}

// bool ApplyWindow::eventFilter(QObject* watched, QEvent* ev) {

// if (watched == btnClose) {

// if (ev->type() == QEvent::Enter) {

// btnClose->setIcon(iconCloseHover);

// } else if (ev->type() == QEvent::Leave) {

// btnClose->setIcon(iconClose);

// }

// }

// return (FramelessWindow::eventFilter(watched, ev));

// }

void ApplyWindow::onCancelClicked() {
    close();
}

void ApplyWindow::onSendClicked() {
    if (type != User) {
        qDebug() << "不是好友请求类型，忽略";

        return; // 暂时只处理好友请求
    }

    QString message = messageEdit->toPlainText().trimmed();

    // 构造好友请求消息
    QJsonObject payload;

    payload["to_uid"] = targetId;
    payload["message"] = message;

    QJsonObject wsMessage;

    wsMessage["type"] = "friend_request";
    wsMessage["payload"] = payload;

    // 发送WebSocket消息
    QString jsonString = QJsonDocument(wsMessage).toJson(QJsonDocument::Compact);

    qDebug() << "发送好友请求:" << jsonString;
    NetworkManager::instance().sendWsMessage(jsonString);

    // 禁用发送按钮，防止重复发送
    btnSend->setEnabled(false);
}
