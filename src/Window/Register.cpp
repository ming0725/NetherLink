/**
 * @file Register.cpp
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-14 周四 08:52:53
 * @brief 【描述】 注册界面
 */

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QFileDialog>
#include <QHttpMultiPart>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPushButton>
#include <QTimer>

#include "Network/NetworkConfig.h"
#include "ui_Register.h"

// #include "Util/ToastTip.hpp"
#include "Util/Validator.hpp"
#include "View/Mainwindow/NotificationManager.h"
#include "Window/Register.hpp"

#include "Util/RoundedPixmap.hpp"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/**//* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

    Register::Register(QWidget* parent) : QWidget(parent), ui(new Ui::Register) {
        ui->setupUi(this);
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        界面_关闭按钮 = ui->btn_close;
        界面_头像按钮 = ui->btn_profile;
        界面_昵称输入框 = ui->edit_userName;
        界面_邮箱输入框 = ui->edit_email;
        界面_验证码输入框 = ui->edit_captcha;
        界面_验证码按钮 = ui->btn_captcha;
        界面_密码输入框 = ui->edit_password;
        界面_确认密码输入框 = ui->edit_confirmPassword;
        界面_注册按钮 = ui->btn_register;
        界面_登录按钮 = ui->btn_login;

        // 禁用按钮的Tab焦点
        界面_关闭按钮->setFocusPolicy(Qt::NoFocus);
        界面_头像按钮->setFocusPolicy(Qt::NoFocus);
        界面_验证码按钮->setFocusPolicy(Qt::NoFocus);
        界面_注册按钮->setFocusPolicy(Qt::NoFocus);
        界面_登录按钮->setFocusPolicy(Qt::NoFocus);

        // 设置Tab键顺序
        QWidget::setTabOrder(界面_昵称输入框, 界面_邮箱输入框);
        QWidget::setTabOrder(界面_邮箱输入框, 界面_验证码输入框);
        QWidget::setTabOrder(界面_验证码输入框, 界面_密码输入框);
        QWidget::setTabOrder(界面_密码输入框, 界面_确认密码输入框);
        QWidget::setTabOrder(界面_确认密码输入框, 界面_昵称输入框);

        // 设置焦点
        界面_昵称输入框->setFocus();

        // 回车键处理
        connect(界面_昵称输入框, &QLineEdit::returnPressed, this, [=, this]() {
            界面_邮箱输入框->setFocus();
        });
        connect(界面_邮箱输入框, &QLineEdit::returnPressed, this, [=, this]() {
            界面_验证码输入框->setFocus();

            if (界面_验证码按钮->isEnabled()) {
                界面_验证码按钮->click();
            }
        });
        connect(界面_验证码输入框, &QLineEdit::returnPressed, this, [=, this]() {
            界面_密码输入框->setFocus();
        });
        connect(界面_密码输入框, &QLineEdit::returnPressed, this, [=, this]() {
            界面_确认密码输入框->setFocus();
        });
        connect(界面_确认密码输入框, &QLineEdit::returnPressed, this, [=, this]() {
            if (界面_注册按钮->isEnabled()) {
                界面_注册按钮->click();
            }
        });

        QPixmap 默认头像(":/icon/icon.png");
        QPixmap 圆角头像 = Util::RoundedPixmap::函数_圆角头像(默认头像, 60);

        界面_头像按钮->setIcon(圆角头像);

        // 关闭按钮
        connect(界面_关闭按钮, &QPushButton::clicked, this, [=, this] {
            this->close();
        });

        // 头像按钮
        connect(界面_头像按钮, &QPushButton::clicked, this, [=, this] {
            QString filePath = QFileDialog::getOpenFileName(this, "选择头像", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");

            if (!filePath.isEmpty()) {
                函数_上传头像(filePath);
            }
        });

        // 获取验证码按钮
        connect(界面_验证码按钮, &QPushButton::clicked, this, [=, this]() {
            if (Util::Validator::函数_检查输入框信息合法性(this, 界面_邮箱输入框)) {
                成员变量_邮箱 = 界面_邮箱输入框->text().trimmed();
                函数_请求验证码(成员变量_邮箱);
            }
        });

        // 注册按钮
        connect(界面_注册按钮, &QPushButton::clicked, this, [=, this]() {
            if (成员变量_正在注册) {
                return;
            }

            if (Util::Validator::函数_检查输入框信息合法性(this, avatarUrl, 界面_昵称输入框, 界面_邮箱输入框, 界面_验证码输入框, 界面_密码输入框, 界面_确认密码输入框)) {
                成员变量_昵称 = 界面_昵称输入框->text().trimmed();
                成员变量_邮箱 = 界面_邮箱输入框->text().trimmed();
                成员变量_验证码 = 界面_验证码输入框->text().trimmed();
                成员变量_密码 = 界面_密码输入框->text();
                成员变量_确认密码 = 界面_确认密码输入框->text();
                函数_注册(成员变量_昵称, 成员变量_邮箱, 成员变量_验证码, 成员变量_密码);
            }
        });

        // 登录按钮
        connect(界面_登录按钮, &QPushButton::clicked, this, [=, this]() {
            this->close();
        });
    }

    Register::~Register() {
        delete ui;
    }

    void Register::paintEvent(QPaintEvent* event) {
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

    void Register::mouseMoveEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->buttons() & Qt::LeftButton)
            move((形参_鼠标事件->globalPosition() - 成员变量_鼠标偏移量).toPoint());
        QWidget::mouseMoveEvent(形参_鼠标事件);
    }

    void Register::mousePressEvent(QMouseEvent* 形参_鼠标事件) {
        if (形参_鼠标事件->button() == Qt::LeftButton)
            成员变量_鼠标偏移量 = (形参_鼠标事件->globalPosition() - frameGeometry().topLeft()).toPoint();
        QWidget::mousePressEvent(形参_鼠标事件);
    }

    void Register::函数_上传头像(const QString& filePath) {
        // 先将图片处理成正方形
        QPixmap originalPixmap(filePath);

        if (originalPixmap.isNull()) {
            NotificationManager::instance().showMessage(this, NotificationManager::Error, "图片加载失败");

            return;
        }

        // 计算正方形的边长（取宽高的最小值）
        int size = qMin(originalPixmap.width(), originalPixmap.height());

        // 计算裁剪的起始位置（居中裁剪）
        int x = (originalPixmap.width() - size) / 2;
        int y = (originalPixmap.height() - size) / 2;

        // 裁剪成正方形
        QPixmap squarePixmap = originalPixmap.copy(x, y, size, size);

        // 将正方形图片保存到临时文件
        QString tempPath = QDir::tempPath() + "/temp_avatar_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

        if (!squarePixmap.save(tempPath, "PNG")) {
            NotificationManager::instance().showMessage(this, NotificationManager::Error, "图片处理失败");

            return;
        }

        QFile* file = new QFile(tempPath);

        if (!file->open(QIODevice::ReadOnly)) {
            file->remove(); // 删除临时文件
            delete file;
            NotificationManager::instance().showMessage(this, NotificationManager::Error, "文件处理失败");

            return;
        }

        // 创建multipart请求
        QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        // 设置文件部分
        QHttpPart imagePart;

        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"avatar.png\""));
        imagePart.setBodyDevice(file);
        file->setParent(multiPart); // 让multiPart管理file的生命周期
        multiPart->append(imagePart);

        // 修改请求URL为Go服务器地址
        QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
        QUrl url(baseHttpUrl + "/api/upload_image");
        QNetworkRequest request(url);

        // 创建临时的QNetworkAccessManager进行上传
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->post(request, multiPart);

        multiPart->setParent(reply); // 让reply管理multiPart的生命周期
        // 处理响应
        connect(reply, &QNetworkReply::finished, this, [this, reply, tempPath, manager]() {
            reply->deleteLater();
            manager->deleteLater();

            // 删除临时文件
            QFile::remove(tempPath);

            if (reply->error() != QNetworkReply::NoError) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "上传失败：网络错误");

                return;
            }
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);

            if (!doc.isObject()) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "上传失败：响应格式错误");

                return;
            }
            QJsonObject obj = doc.object();

            // 检查是否有错误
            if (obj.contains("error")) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("上传失败：%1").arg(obj["error"].toString()));

                return;
            }

            // 成功响应中获取url
            if (obj.contains("url")) {
                // 保存头像URL
                this->avatarUrl = obj["url"].toString();

                // 更新头像显示
                QPixmap newAvatar(tempPath); // 使用处理后的正方形图片
                // if (!newAvatar.isNull()) {
                // 界面_头像按钮->setIcon(createRoundedPixmap(newAvatar));
                // }
                NotificationManager::instance().showMessage(this, NotificationManager::Success, "头像上传成功");
            }
        });
    }

    void Register::函数_请求验证码(const QString& 形参_邮箱) {
        界面_验证码按钮->setEnabled(false);

        QJsonObject body;

        body["email"] = 形参_邮箱;

        QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
        QUrl url(baseHttpUrl + "/api/send_code");
        QNetworkRequest request(url);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 创建网络管理器
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->post(request, payload);

        // 处理请求完成
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
            QByteArray responseData = reply->readAll();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // 处理网络错误，但排除400状态码
            if ((reply->error() != QNetworkReply::NoError) && (statusCode != 400)) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("获取验证码失败：网络错误 - %1").arg(reply->errorString()));
                界面_验证码按钮->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 解析响应数据
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error != QJsonParseError::NoError) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "获取验证码失败：返回数据格式错误");
                界面_验证码按钮->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            QJsonObject obj = doc.object();

            // 处理错误响应
            if ((statusCode == 400) || obj.contains("error")) {
                QString errorMsg = obj["error"].toString();
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("获取验证码失败：%1").arg(errorMsg));
                界面_验证码按钮->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            NotificationManager::instance().showMessage(this, NotificationManager::Success, "获取验证码成功");

            // 倒计时60秒
            int countdown = 60;
            QTimer* timer = new QTimer(this);
            connect(timer, &QTimer::timeout, this, [this, timer, countdown]() mutable {
                if (--countdown <= 0) {
                    timer->stop();
                    timer->deleteLater();
                    界面_验证码按钮->setEnabled(true);
                    界面_验证码按钮->setText("获取验证码");
                } else {
                    界面_验证码按钮->setText(QString("%1s").arg(countdown));
                }
            });
            timer->start(1000);
            reply->deleteLater();
            manager->deleteLater();
        });
    }

    void Register::函数_注册(const QString& 形参_昵称, const QString& 形参_邮箱, const QString& 形参_验证码, const QString& 形参_密码) {
        if (成员变量_正在注册) {
            return;
        }
        成员变量_正在注册 = true;
        界面_注册按钮->setEnabled(false);
        界面_注册按钮->setText("注册中...");

        QJsonObject body;

        body["email"] = 形参_邮箱;
        body["varifycode"] = 形参_验证码;
        body["user"] = 形参_昵称;
        body["passwd"] = 形参_密码;
        body["avatar_url"] = avatarUrl;

        QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
        QUrl url(baseHttpUrl + "/api/register");
        QNetworkRequest request(url);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 创建网络管理器
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->post(request, payload);

        // 处理请求完成
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
            // 确保在函数结束时恢复按钮状态
            auto cleanup = [=, this]() {
                               成员变量_正在注册 = false;
                               界面_注册按钮->setEnabled(true);
                               界面_注册按钮->setText("注册");
                           };
            QByteArray responseData = reply->readAll();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // 处理网络错误，但排除400状态码
            if ((reply->error() != QNetworkReply::NoError) && (statusCode != 400)) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("注册失败：网络错误 - %1").arg(reply->errorString()));
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 解析响应数据
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error != QJsonParseError::NoError) {
                NotificationManager::instance().showMessage(this, NotificationManager::Error, "注册失败：返回数据格式错误");
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            QJsonObject obj = doc.object();

            // 处理错误响应
            if ((statusCode == 400) || obj.contains("error")) {
                QString errorMsg = obj["error"].toString();
                NotificationManager::instance().showMessage(this, NotificationManager::Error, QString("注册失败：%1").arg(errorMsg));
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 注册成功
            NotificationManager::instance().showMessage(this, NotificationManager::Success, "注册成功");

            // 保存当前输入的邮箱和密码
            成员变量_邮箱 = 界面_邮箱输入框->text().trimmed();
            成员变量_密码 = 界面_密码输入框->text();

            // 2秒后关闭窗口并发送信号
            QTimer::singleShot(2000, this, [=, this]() {
                emit sig_registerSuccess(成员变量_邮箱, 成员变量_密码);
                this->close();
            });
            cleanup();
            reply->deleteLater();
            manager->deleteLater();
        });
    }
}
