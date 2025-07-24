
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Login.h"

#include <QApplication>
#include <QBoxLayout>

#include <QNetworkReply>
#include <QPushButton>
#include <QScreen>

#include <QtConcurrent>

#include "AvatarLoader.h"
#include "CurrentUser.h"
#include "DatabaseManager.h"
#include "GroupRepository.h"

#include "MainWindow.h"
#include "NetworkConfig.h"
#include "NetworkManager.h"
#include "NotificationManager.h"

#include "Register.h"
#include "UserRepository.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// 定义静态成员变量
const QRegularExpression Login::emailRegex("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

Login::Login(QWidget* parent) : FramelessWindow(parent) {
    // 固定窗口大小
    this->setFixedSize(320, 450);
    this->setMouseTracking(true);

    // 窗口居中显示
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect sg = screen->geometry();
    int cx = (sg.width() - width()) / 2;
    int cy = (sg.height() - height()) / 2;

    move(cx, cy);

    // 最外层垂直布局
    QVBoxLayout* main_vbox = new QVBoxLayout(this);

    main_vbox->setContentsMargins(11, 0, 11, 11);
    main_vbox->setSpacing(0);
    this->setLayout(main_vbox);

    // --------------------------
    // 1. 顶部右侧的关闭按钮
    // --------------------------
    QHBoxLayout* top_hbox = new QHBoxLayout;

    top_hbox->setContentsMargins(0, 0, 0, 0);
    top_hbox->addStretch(); // 左侧占位
    // 将按钮尺寸调整为 32×32，图标尺寸 20×20
    this->closeButton = new QPushButton(this);
    this->closeButton->setFixedSize(50, 50);
    this->closeButton->setIcon(QIcon(":/icon/close.png"));
    this->closeButton->setIconSize(QSize(30, 30));
    this->closeButton->setFlat(true);
    this->closeButton->setCursor(Qt::PointingHandCursor);
    this->closeButton->setStyleSheet(QString("QPushButton:pressed {""  background: none;""  border: none;""}"));

    // 点击立即关闭窗口
    connect(this->closeButton, &QPushButton::clicked, this, &Login::close);
    top_hbox->addWidget(this->closeButton, 0, Qt::AlignTop | Qt::AlignRight);
    main_vbox->addLayout(top_hbox);

    // --------------------------
    // 2. 用户头像
    // --------------------------
    this->userHead = new QLabel(this);
    this->userHead->setFixedSize(80, 80);
    this->userHead->setScaledContents(true);

    QPixmap icon(":/icon/icon.png");

    this->userHead->setPixmap(icon.scaled(QSize(this->userHead->size()) * 1.5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    // --------------------------
    // 3. 账户、密码、登录、注册等控件
    // --------------------------
    this->userAccountEdit = new LineEditComponent(this);
    this->userPasswordEdit = new LineEditComponent(this);
    this->userAccountEdit->setIcon(QPixmap(":/icon/skull.png"));
    this->userPasswordEdit->setIcon(QPixmap(":/icon/lock.png"));
    this->userAccountEdit->setIconSize(QSize(20, 20));
    this->userPasswordEdit->setIconSize(QSize(20, 20));

    // 设置焦点策略
    userAccountEdit->setFocusPolicy(Qt::StrongFocus);
    userPasswordEdit->setFocusPolicy(Qt::StrongFocus);
    userAccountEdit->getLineEdit()->setAlignment(Qt::AlignCenter);
    userPasswordEdit->getLineEdit()->setAlignment(Qt::AlignCenter);
    userPasswordEdit->getLineEdit()->setEchoMode(QLineEdit::Password);
    userAccountEdit->getLineEdit()->setPlaceholderText("输入账号");
    userPasswordEdit->getLineEdit()->setPlaceholderText("输入密码");
    this->userAccountEdit->setFixedHeight(40);
    this->userPasswordEdit->setFixedHeight(40);

    QFont font;

    font.setPixelSize(14);
    this->userAccountEdit->getLineEdit()->setFont(font);
    this->userPasswordEdit->getLineEdit()->setFont(font);
    this->loginButton = new QPushButton("登录", this);
    this->loginButton->installEventFilter(this);

    // 设置"登录"按钮圆角、背景色、字体颜色
    this->loginButton->setFixedHeight(40);
    this->loginButton->setCursor(Qt::PointingHandCursor);
    this->loginButton->setStyleSheet(QString("QPushButton {""  background-color: #0099ff;""  color: white;""  border: none;""  border-radius: 20px;""  font-size: 15px;""}""QPushButton:pressed {""  background: none;""  border: none;""}"));

    QPalette pale;

    pale.setColor(QPalette::WindowText, QColor(0x2d77e5));
    font.setPixelSize(13);
    this->registerButton = new QLabel("注册账号", this);
    this->registerButton->setPalette(pale);
    this->registerButton->adjustSize();
    this->registerButton->setCursor(Qt::PointingHandCursor);
    this->registerButton->installEventFilter(this);
    this->registerButton->setFont(font);

    QVBoxLayout* vbox_1 = new QVBoxLayout;

    vbox_1->setContentsMargins(20, 20, 20, 11);
    vbox_1->addWidget(this->userAccountEdit);
    vbox_1->addSpacing(10);
    vbox_1->addWidget(this->userPasswordEdit);
    vbox_1->addSpacing(30);
    vbox_1->addWidget(this->loginButton);
    vbox_1->addSpacing(20);
    vbox_1->addWidget(this->registerButton, 0, Qt::AlignCenter);

    // --------------------------
    // 4. 将头像和输入区放到主布局
    // --------------------------
    main_vbox->addSpacing(10); // 与顶部关闭按钮之间预留一些距离
    main_vbox->addWidget(this->userHead, 0, Qt::AlignCenter);
    main_vbox->addSpacing(45);
    main_vbox->addLayout(vbox_1);
    main_vbox->addStretch();

    // 设置Tab键顺序
    QWidget::setTabOrder(userAccountEdit, userPasswordEdit);
    QWidget::setTabOrder(userPasswordEdit, userAccountEdit);

    // 禁用按钮的Tab焦点
    closeButton->setFocusPolicy(Qt::NoFocus);
    loginButton->setFocusPolicy(Qt::NoFocus);
    registerButton->setFocusPolicy(Qt::NoFocus);

    // 回车键处理
    connect(userAccountEdit->getLineEdit(), &QLineEdit::returnPressed, this, [this] () {
        userPasswordEdit->getLineEdit()->setFocus();
    });
    connect(userPasswordEdit->getLineEdit(), &QLineEdit::returnPressed, this, [this] () {
        QString account = this->userAccountEdit->currentText().trimmed();
        QString password = this->userPasswordEdit->currentText();

        if (account.isEmpty()) {
            NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);
        }

        if (!isValidEmail(account)) {
            NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);
        }

        if (password.isEmpty()) {
            NotificationManager::instance().showMessage("请输入密码", NotificationManager::Error, this);
        }
        doLogin(account, password);
    });
    QTimer::singleShot(50, this, [this] () {
        userAccountEdit->getLineEdit()->clearFocus();
        userPasswordEdit->getLineEdit()->clearFocus();
    });
}

Login::~Login() {}

void Login::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    QLinearGradient gradient(QPoint(this->rect().topLeft()), QPoint(this->rect().bottomLeft()));

    gradient.setColorAt(0.2, QColor(0x0099ff));
    gradient.setColorAt(0.75, Qt::white);
    painter.setBrush(gradient);
    painter.drawRect(this->rect());
    QWidget::paintEvent(event);
}

void Login::getUserHeadBytes(const QPixmap& userhead) {
    this->userHead->setPixmap(userhead);
}

void Login::setLoginInfo(const QString& email, const QString& password) {
    userAccountEdit->getLineEdit()->setText(email);
    userPasswordEdit->getLineEdit()->setText(password);

    // 激活窗口并置顶
    this->show();
    this->raise();  // 将窗口移到最前
    this->activateWindow(); // 激活窗口
}

bool Login::isValidEmail(const QString& email) const {
    return (emailRegex.match(email).hasMatch());
}

bool Login::eventFilter(QObject* target, QEvent* event) {
    if (target == this->registerButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            Register* registerWindow = new Register();

            registerWindow->setAttribute(Qt::WA_DeleteOnClose);

            // 连接注册成功信号
            connect(registerWindow, &Register::registerSuccess, this, &Login::setLoginInfo);
            registerWindow->show();

            return (true);
        }
    }

    if (target == this->loginButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            QString account = this->userAccountEdit->currentText().trimmed();
            QString password = this->userPasswordEdit->currentText();

            if (account.isEmpty()) {
                NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);

                return (true);
            }

            if (!isValidEmail(account)) {
                NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);

                return (true);
            }

            if (password.isEmpty()) {
                NotificationManager::instance().showMessage("请输入密码", NotificationManager::Error, this);

                return (true);
            }

            if (this->isLogining) {
                return (false);
            }
            doLogin(account, password);

            return (true);
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            return (true);
        }
    }
    return (QWidget::eventFilter(target, event));
}

void Login::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
}

void Login::doLogin(QString account, QString password) {
    if (isLogining) {
        return;
    }
    isLogining = true;
    loginButton->setEnabled(false);
    loginButton->setText("登录中...");

    // 1) 准备HTTP登录请求
    QJsonObject body;

    body["email"] = account;
    body["passwd"] = password;

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
    QUrl url(baseHttpUrl + "/api/login");
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 2) 创建网络管理器并发送请求
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkReply* reply = manager->post(request, payload);

    // 设置请求超时定时器
    QTimer* timeoutTimer = new QTimer(this);

    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(5000);  // 5秒超时
    // 连接超时定时器
    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, manager, timeoutTimer] () {
        NotificationManager::instance().showMessage("登录超时，请检查网络连接或服务器状态", NotificationManager::Error, this);

        if (reply) {
            reply->abort();  // 终止请求
            reply->deleteLater();
        }
        timeoutTimer->deleteLater();
        manager->deleteLater();
        isLogining = false;
        loginButton->setEnabled(true);
        loginButton->setText("登录");
    });

    // 启动超时定时器
    timeoutTimer->start();

    // 3) 处理响应
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager, timeoutTimer] () {
        // 停止超时定时器
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        // 如果请求被取消（超时导致），直接返回
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            manager->deleteLater();

            return;
        }

        // 处理网络错误，但排除400状态码
        if (reply->error() != QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (statusCode != 400) {
                QString errorMsg;

                switch (reply->error()) {
                    case QNetworkReply::ConnectionRefusedError:
                        errorMsg = "无法连接到服务器，请检查服务器是否启动";
                        break;
                    case QNetworkReply::RemoteHostClosedError:
                        errorMsg = "服务器连接已断开";
                        break;
                    case QNetworkReply::HostNotFoundError:
                        errorMsg = "找不到服务器";
                        break;
                    case QNetworkReply::TimeoutError:
                        errorMsg = "连接超时";
                        break;
                    default:
                        errorMsg = QString("网络错误：%1").arg(reply->errorString());
                }
                NotificationManager::instance().showMessage(errorMsg, NotificationManager::Error, this);
                isLogining = false;
                loginButton->setEnabled(true);
                loginButton->setText("登录");
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
        }
        QByteArray responseData = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 解析响应数据
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

        if (jsonError.error != QJsonParseError::NoError) {
            NotificationManager::instance().showMessage("登录失败：返回数据格式错误", NotificationManager::Error, this);
            reply->deleteLater();
            manager->deleteLater();

            return;
        }
        QJsonObject obj = doc.object();

        // 处理错误响应
        if ((statusCode == 400) || obj.contains("error")) {
            QString errorMsg;

            if (obj.contains("error")) {
                if (obj["error"].isString()) {
                    errorMsg = obj["error"].toString();
                } else {
                    errorMsg = QString("错误码：%1").arg(obj["error"].toInt());
                }
            } else {
                errorMsg = "未知错误";
            }
            NotificationManager::instance().showMessage(QString("登录失败：%1").arg(errorMsg), NotificationManager::Error, this);
            reply->deleteLater();
            manager->deleteLater();

            return;
        }

        // 登录成功，获取uid和token
        const QString uid = obj["uid"].toString();
        const QString token = obj["token"].toString();
        qDebug() << "登录成功，正在建立WebSocket连接...";

        // 4) 建立WebSocket连接
        QString wsUrl = NetworkConfig::instance().getWebSocketAddress() + "/ws?token=%1";
        QUrl websocketUrl(wsUrl.arg(token));

        // 使用NetworkManager的connectWss方法建立WebSocket连接
        NetworkManager::instance().connectWss(websocketUrl);

        // 5) 处理WebSocket连接成功
        connect(&NetworkManager::instance(), &NetworkManager::wssConnected, this, [this, uid, token] () {
            onLoginSuccess(uid, token);
        }, Qt::SingleShotConnection);

        // 6) 处理WebSocket错误
        connect(&NetworkManager::instance(), &NetworkManager::wssError, this, [this, reply, manager, timeoutTimer] (const QString& err) {
            NotificationManager::instance().showMessage(QString("WebSocket连接失败：%1").arg(err), NotificationManager::Error, this);

            if (reply) {
                reply->abort();          // 终止请求
                reply->deleteLater();
            }

            if (timeoutTimer) {
                timeoutTimer->stop();
                timeoutTimer->deleteLater();
            }

            if (manager) {
                manager->deleteLater();
            }
            isLogining = false;
            loginButton->setEnabled(true);
            loginButton->setText("登录");
        }, Qt::SingleShotConnection);
        reply->deleteLater();
        manager->deleteLater();
    });

    // 7) 处理网络请求错误
    connect(reply, &QNetworkReply::errorOccurred, this, [this] (QNetworkReply::NetworkError code) {
        if (code != QNetworkReply::NoError) {
            NotificationManager::instance().showMessage("登录网络错误", NotificationManager::Error, this);
        }
    });
}

void Login::onLoginSuccess(const QString& uid, const QString& token) {
    NotificationManager::instance().showMessage("登录成功", NotificationManager::Success, this);
    currentUid = uid;
    currentToken = token;

    // 设置当前用户ID并初始化数据库（在主线程）
    CurrentUser::instance().setUserInfo(uid, token);
    qDebug() << "CurrentUser已设置当前id" << uid;
    DatabaseManager::instance().initUserDatabase(uid);
    qDebug() << "DatabaseManager已设置当前id" << uid;

    // 创建异步加载观察器
    if (!contactsWatcher) {
        contactsWatcher = new QFutureWatcher <void>(this);
        connect(contactsWatcher, &QFutureWatcher <void>::finished, this, &Login::onContactsLoaded);
    }

    // 显示加载状态
    loginButton->setText("加载联系人中...");

    // 开始异步加载联系人信息
    QFuture <void> future = QtConcurrent::run([this, uid, token] () {
        loadContacts(uid, token);
    });

    contactsWatcher->setFuture(future);
}

void Login::loadContacts(const QString& uid, const QString& token) {
    qDebug() << "开始获取联系人信息...";

    // 在主线程中创建请求
    QMetaObject::invokeMethod(this, [this, uid, token] () {
        // 创建网络访问管理器
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);

        // 准备请求头
        QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
        QNetworkRequest request(QUrl(baseHttpUrl + "/api/contacts"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

        // 发送GET请求
        QNetworkReply* reply = manager->get(request);

        // 处理响应
        connect(reply, &QNetworkReply::finished, this, [this, reply, uid, token] () {
            // 检查是否有错误
            if (reply->error() != QNetworkReply::NoError) {
                qWarning() << "获取联系人信息失败:" << reply->errorString();
                reply->deleteLater();

                return;
            }

            // 读取响应数据
            QByteArray data = reply->readAll();
            reply->deleteLater();

            // 解析JSON响应
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(data, &err);

            if ((err.error != QJsonParseError::NoError) || !doc.isObject()) {
                NotificationManager::instance().showMessage("登录返回解析失败", NotificationManager::Error, this);

                return;
            }
            QJsonObject obj = doc.object();
            qDebug() << "获取到联系人数据，开始处理...";

            // 发送WebSocket登录请求
            QJsonObject loginPayload;
            loginPayload["uid"] = uid;
            loginPayload["token"] = token;
            QJsonObject wsLoginMsg;
            wsLoginMsg["type"] = "login";
            wsLoginMsg["payload"] = loginPayload;
            QString jsonString = QJsonDocument(wsLoginMsg).toJson(QJsonDocument::Compact);
            NetworkManager::instance().sendWsMessage(jsonString);

            // 加载用户信息
            QJsonObject userObj = obj["user"].toObject();
            QString avatarUrl = userObj["avatar_url"].toString();
            QString userName = userObj["name"].toString();

            // 加载当前用户头像
            AvatarLoader::instance().loadAvatar(uid, QUrl(avatarUrl), false);
            CurrentUser::instance().setUserInfo(uid, token, userName, avatarUrl);

            // 加载联系人信息和头像
            UserRepository::instance().loadUserAndContacts(userObj, obj["friends"].toArray());
            QJsonArray friends = obj["friends"].toArray();

            for (const QJsonValue& friendVal : friends) {
                QJsonObject friendObj = friendVal.toObject();
                QString friendId = friendObj["uid"].toString();
                QString friendAvatarUrl = friendObj["avatar_url"].toString();

                if (!friendAvatarUrl.isEmpty()) {
                    AvatarLoader::instance().loadAvatar(friendId, QUrl(friendAvatarUrl), false);
                }
            }

            // 加载群组信息、成员和头像
            QJsonArray groups = obj["groups"].toArray();
            GroupRepository::instance().loadGroupsAndMembers(groups);

            for (const QJsonValue& groupVal : groups) {
                QJsonObject groupObj = groupVal.toObject();
                QString groupId = QString::number(groupObj["gid"].toInt());
                QString groupAvatarUrl = groupObj["avatar_url"].toString();

                if (!groupAvatarUrl.isEmpty()) {
                    AvatarLoader::instance().loadAvatar(groupId, QUrl(groupAvatarUrl), true);
                }

                // 加载群成员头像
                QJsonArray members = groupObj["members"].toArray();

                for (const QJsonValue& memberVal : members) {
                    QJsonObject memberObj = memberVal.toObject();
                    QString memberId = memberObj["uid"].toString();
                    QString memberAvatarUrl = memberObj["avatar_url"].toString();

                    if (!memberAvatarUrl.isEmpty()) {
                        AvatarLoader::instance().loadAvatar(memberId, QUrl(memberAvatarUrl), false);
                    }
                }
            }
        });

        // 处理网络错误
        connect(reply, &QNetworkReply::errorOccurred, this, [this] (QNetworkReply::NetworkError code) {
            if (code != QNetworkReply::NoError) {
                NotificationManager::instance().showMessage(QString("网络错误: %1").arg(code), NotificationManager::Error, this);
            }
        });
    }, Qt::QueuedConnection);
}

void Login::onContactsLoaded() {
    // 显示加载状态
    loginButton->setText("创建主窗口中...");

    // 预先在主线程中创建主窗口，但不显示
    mainWindow = MainWindow::getInstance();
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    // 延迟显示主窗口
    QTimer::singleShot(500, [this] () {
        if (mainWindow) {
            mainWindow->show();
            this->close();
            isLogining = false;
        }
    });
}

void Login::mousePressEvent(QMouseEvent* event) {
    QWidget* fw = QApplication::focusWidget();

    if (qobject_cast <LineEditComponent*>(fw) || qobject_cast <QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    FramelessWindow::mousePressEvent(event);
}
