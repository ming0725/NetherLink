/**
 * @file Login.cpp
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-11 周一 09:11:22
 * @brief 【描述】 登录界面
 */

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QNetworkReply>
#include <QPushButton>
#include <QtConcurrent>

#include "Data/AvatarLoader.h"
#include "Data/CurrentUser.h"
#include "Data/DatabaseManager.h"
#include "Data/GroupRepository.h"
#include "Data/UserRepository.h"
#include "Network/NetworkConfig.h"
#include "Network/NetworkManager.h"
#include "ui_Login.h"

// #include "Util/ToastTip.hpp"
#include "Util/RoundedPixmap.hpp"
#include "Util/Validator.hpp"
#include "View/Mainwindow/NotificationManager.h"
#include "View/MainWindow/MainWindow.h"
#include "Window/Login.hpp"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/**//* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

    /**
     * @brief 【描述】 构造函数
     */
    Login::Login(QWidget* parent) : QWidget(parent), ui(new Ui::Login) {
        ui->setupUi(this);
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        界面_关闭按钮 = ui->btn_close;
        界面_头像标签 = ui->lbl_profile;
        界面_邮箱输入框 = ui->edit_email;
        界面_密码输入框 = ui->edit_password;
        界面_登录按钮 = ui->btn_login;
        界面_注册按钮 = ui->btn_register;

        // 禁用按钮的Tab焦点
        界面_关闭按钮->setFocusPolicy(Qt::NoFocus);
        界面_登录按钮->setFocusPolicy(Qt::NoFocus);
        界面_注册按钮->setFocusPolicy(Qt::NoFocus);

        // 设置Tab键顺序
        QWidget::setTabOrder(界面_邮箱输入框, 界面_密码输入框);
        QWidget::setTabOrder(界面_密码输入框, 界面_邮箱输入框);

        // 设置焦点
        界面_邮箱输入框->setFocus();

        // 回车键处理
        connect(界面_邮箱输入框, &QLineEdit::returnPressed, this, [=, this]() {
            界面_密码输入框->setFocus();
        });
        connect(界面_密码输入框, &QLineEdit::returnPressed, this, [=, this]() {
            if (界面_登录按钮->isEnabled()) {
                界面_登录按钮->click();
            }
        });

        QPixmap 默认头像(":/icon/icon.png");
        QPixmap 圆角头像 = Util::RoundedPixmap::函数_圆角头像(默认头像, 60);

        界面_头像标签->setPixmap(圆角头像);

        // 关闭按钮
        connect(界面_关闭按钮, &QPushButton::clicked, this, [=, this]() {
            界面_注册界面.close();
            this->close();
        });

        // 登录按钮
        connect(界面_登录按钮, &QPushButton::clicked, this, [=, this]() {
            if (成员变量_正在登录) {
                return;
            }

            if (Util::Validator::函数_检查输入框信息合法性(this, 界面_邮箱输入框, 界面_密码输入框)) {
                成员变量_邮箱 = 界面_邮箱输入框->text().trimmed();
                成员变量_密码 = 界面_密码输入框->text();
                函数_登录(成员变量_邮箱, 成员变量_密码);
            }
        });

        // 注册按钮
        connect(界面_注册按钮, &QPushButton::clicked, this, [=, this]() {
            界面_注册界面.show();

            // 注册成功信号
            connect(&界面_注册界面, &Register::sig_registerSuccess, this, &Login::函数_设置登录信息);
        });
    }

    /**
     * @brief 【描述】 析构函数
     */
    Login::~Login() {
        delete ui;
    }

    void Login::paintEvent(QPaintEvent* event) {
        QPainter painter(this);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        // 背景圆角矩形
        QRectF r = rect();
        qreal radius = 20;
        QPainterPath path;

        path.addRoundedRect(r, radius, radius);

        QLinearGradient gradient(QPoint(this->rect().topLeft()), QPoint(this->rect().bottomLeft()));

        gradient.setColorAt(0.2, QColor(0x0099ff));
        gradient.setColorAt(0.8, QColor(0xffffff));
        painter.fillPath(path, gradient);
        QWidget::paintEvent(event);
    }

    void Login::mousePressEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->button() == Qt::LeftButton)
            成员变量_鼠标偏移量 = (形参_鼠标事件->globalPosition() - frameGeometry().topLeft()).toPoint();
        QWidget::mousePressEvent(形参_鼠标事件);
    }

    void Login::mouseMoveEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->buttons() & Qt::LeftButton)
            move((形参_鼠标事件->globalPosition() - 成员变量_鼠标偏移量).toPoint());
        QWidget::mouseMoveEvent(形参_鼠标事件);
    }

    // void Login::函数_获取用户头像(const QPixmap& userhead) {

    // 界面_userHead->setPixmap(userhead);

    // }

    void Login::函数_设置登录信息(const QString& 形参_邮箱, const QString& 形参_密码) {
        界面_邮箱输入框->setText(形参_邮箱);
        界面_密码输入框->setText(形参_密码);

        // 激活窗口并置顶
        this->show();
        this->raise(); // 将窗口移到最前
        this->activateWindow(); // 激活窗口
    }

    void Login::函数_登录(QString 形参_邮箱, QString 形参_密码) {
        if (成员变量_正在登录) {
            return;
        }
        成员变量_正在登录 = true;
        界面_登录按钮->setEnabled(false);
        界面_登录按钮->setText("登录中...");

        // 1) 准备HTTP登录请求
        QJsonObject body;

        body["email"] = 形参_邮箱;
        body["passwd"] = 形参_密码;

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
        timeoutTimer->setInterval(5000); // 5秒超时
        // 连接超时定时器
        connect(timeoutTimer, &QTimer::timeout, this, [this, reply, manager, timeoutTimer]() {
            NotificationManager::instance().showMessage(this, NotificationManager::Error, "登录超时，请检查网络连接或服务器状态");

            if (reply) {
                reply->abort(); // 终止请求
                reply->deleteLater();
            }
            timeoutTimer->deleteLater();
            manager->deleteLater();
            成员变量_正在登录 = false;
            界面_登录按钮->setEnabled(true);
            界面_登录按钮->setText("登录");
        });

        // 启动超时定时器
        timeoutTimer->start();

        // 3) 处理响应
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager, timeoutTimer]() {
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
                    NotificationManager::instance().showMessage(this, NotificationManager::Error, errorMsg);
                    成员变量_正在登录 = false;
                    界面_登录按钮->setEnabled(true);
                    界面_登录按钮->setText("登录");
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
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "登录失败：返回数据格式错误");
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
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("登录失败：%1").arg(errorMsg));
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
            connect(&NetworkManager::instance(), &NetworkManager::wssConnected, this, [this, uid, token]() {
                函数_登录成功(uid, token);
            }, Qt::SingleShotConnection);

            // 6) 处理WebSocket错误
            connect(&NetworkManager::instance(), &NetworkManager::wssError, this, [=, this](const QString& err) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("WebSocket连接失败：%1").arg(err));

                if (reply) {
                    reply->abort();  // 终止请求
                    reply->deleteLater();
                }

                // if (timeoutTimer) {
                // timeoutTimer->stop();
                // timeoutTimer->deleteLater();
                // }
                if (manager) {
                    manager->deleteLater();
                }
                成员变量_正在登录 = false;
                界面_登录按钮->setEnabled(true);
                界面_登录按钮->setText("登录");
            }, Qt::SingleShotConnection);

            // reply->deleteLater();
            // manager->deleteLater();
        });

        // 7) 处理网络请求错误
        connect(reply, &QNetworkReply::errorOccurred, this, [=, this](QNetworkReply::NetworkError code) {
            if (code != QNetworkReply::NoError) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "登录网络错误");
            }
        });
    }

/**
 * @brief 【描述】 登录成功
 *
 * @param uid 【参数注释】 {text}
 * @param token 【参数注释】 {text}
 */
    void Login::函数_登录成功(const QString& uid, const QString& token) {
        NotificationManager::instance().showMessage(this, NotificationManager::Success, "登录成功");
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
        界面_登录按钮->setText("加载联系人中...");

        // 开始异步加载联系人信息
        QFuture <void> future = QtConcurrent::run([this, uid, token]() {
            loadContacts(uid, token);
        });

        contactsWatcher->setFuture(future);
    }

    void Login::loadContacts(const QString& uid, const QString& token) {
        qDebug() << "开始获取联系人信息...";

        // 在主线程中创建请求
        QMetaObject::invokeMethod(this, [this, uid, token]() {
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
            connect(reply, &QNetworkReply::finished, this, [this, reply, uid, token]() {
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
                    NotificationManager::instance().showMessage(this, NotificationManager::Error, "登录返回解析失败");

                    return;
                }
                QJsonObject obj = doc.object();
                qDebug() << "获取到联系人数据，开始处理...";

                // 发送WebSocket登录请求
                QJsonObject loginPayload;
                loginPayload["uid"] = uid;
                loginPayload["token"] = token;
                QJsonObject wsLoginMsg;
                wsLoginMsg["type"] = "Login";
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
            connect(reply, &QNetworkReply::errorOccurred, this, [=, this](QNetworkReply::NetworkError code) {
                if (code != QNetworkReply::NoError) {
                    NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("网络错误: %1").arg(code));
                }
            });
        }, Qt::QueuedConnection);
    }

    void Login::onContactsLoaded() {
        // 显示加载状态
        界面_登录按钮->setText("创建主窗口中...");

        // 预先在主线程中创建主窗口，但不显示
        mainWindow = MainWindow::getInstance();
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);

        // 显示主窗口
        QTimer::singleShot(2000, [=, this]() {
            if (mainWindow) {
                mainWindow->show();
                this->close();
                成员变量_正在登录 = false;
            }
        });
    }
}
