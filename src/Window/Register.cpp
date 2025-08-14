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
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

#include "Network/NetworkConfig.h"
#include "ui_Register.h"
#include "Util/Validator.h"
#include "View/Mainwindow/NotificationManager.h"
#include "Window/Register.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/**//* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

    Register::Register(QWidget* parent) : QMainWindow(parent), ui(new Ui::Register) {
        ui->setupUi(this);

        // 禁用按钮的Tab焦点
        ui->userHead->setFocusPolicy(Qt::NoFocus);
        ui->getCodeButton->setFocusPolicy(Qt::NoFocus);
        ui->registerButton->setFocusPolicy(Qt::NoFocus);
        ui->loginButton->setFocusPolicy(Qt::NoFocus);

        // 设置Tab键顺序
        QWidget::setTabOrder(ui->nicknameEdit, ui->usernameEdit);
        QWidget::setTabOrder(ui->usernameEdit, ui->verificationCodeEdit);
        QWidget::setTabOrder(ui->verificationCodeEdit, ui->passwordEdit);
        QWidget::setTabOrder(ui->passwordEdit, ui->confirmPasswordEdit);
        QWidget::setTabOrder(ui->confirmPasswordEdit, ui->nicknameEdit);

        // 设置焦点
        ui->nicknameEdit->setFocus();

        // 回车键处理
        connect(ui->nicknameEdit, &QLineEdit::returnPressed, this, [=, this]() {
            ui->usernameEdit->setFocus();
        });
        connect(ui->usernameEdit, &QLineEdit::returnPressed, this, [=, this]() {
            ui->verificationCodeEdit->setFocus();
            ui->getCodeButton->click();
        });
        connect(ui->verificationCodeEdit, &QLineEdit::returnPressed, this, [=, this]() {
            ui->passwordEdit->setFocus();
        });
        connect(ui->passwordEdit, &QLineEdit::returnPressed, this, [=, this]() {
            ui->confirmPasswordEdit->setFocus();
        });
        connect(ui->confirmPasswordEdit, &QLineEdit::returnPressed, this, [=, this]() {
            if (ui->registerButton->isEnabled()) {
                ui->registerButton->click();
            }
        });

        // 设置头像
        connect(ui->userHead, &QPushButton::clicked, this, [=, this] {
            QString filePath = QFileDialog::getOpenFileName(this, "选择头像", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");

            if (!filePath.isEmpty()) {
                函数_上传头像(filePath);
            }
        });

        // 获取验证码按钮
        connect(ui->getCodeButton, &QPushButton::clicked, this, [=, this]() {
            邮箱 = ui->usernameEdit->text().trimmed();

            if (邮箱.isEmpty()) {
                NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);

                return;
            }

            if (!Util::Validator::函数_是否为有效邮箱(邮箱)) {
                NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);

                return;
            }
            requestVerificationCode(邮箱);
        });

        // 注册按钮
        connect(ui->registerButton, &QPushButton::clicked, this, [=, this]() {
            if (正在注册) {
                return;
            }
            昵称 = ui->nicknameEdit->text().trimmed();
            邮箱 = ui->usernameEdit->text().trimmed();
            验证码 = ui->verificationCodeEdit->text().trimmed();
            密码 = ui->passwordEdit->text();
            确认密码 = ui->confirmPasswordEdit->text();

            if (函数_检查输入框信息合法性(昵称, 邮箱, 验证码, 密码, 确认密码)) {
                函数_注册(昵称, 邮箱, 验证码, 密码);
            }
        });

        // 登录按钮
        connect(ui->loginButton, &QPushButton::clicked, this, [=, this]() {
            this->close();
        });
    }

    Register::~Register() {
        delete ui;
    }

    // QPixmap Register::createRoundedPixmap(const QPixmap& source) {

    // if (source.isNull())

    // return (QPixmap());

    // QPixmap scaled = source.scaled(ui->userHead->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // QPixmap rounded(scaled.size());

    // rounded.fill(Qt::transparent);

    // QPainter painter(&rounded);

    // painter.setRenderHint(QPainter::Antialiasing);

    // painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // QPainterPath path;

    // path.addEllipse(rounded.rect());

    // painter.setClipPath(path);

    // painter.drawPixmap(0, 0, scaled);

    // return (rounded);

    // }

    void Register::函数_上传头像(const QString& filePath) {
        // 先将图片处理成正方形
        QPixmap originalPixmap(filePath);

        if (originalPixmap.isNull()) {
            NotificationManager::instance().showMessage("图片加载失败", NotificationManager::Error, this);

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
            NotificationManager::instance().showMessage("图片处理失败", NotificationManager::Error, this);

            return;
        }

        QFile* file = new QFile(tempPath);

        if (!file->open(QIODevice::ReadOnly)) {
            file->remove(); // 删除临时文件
            delete file;
            NotificationManager::instance().showMessage("文件处理失败", NotificationManager::Error, this);

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
                NotificationManager::instance().showMessage("上传失败：网络错误", NotificationManager::Error, this);

                return;
            }
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);

            if (!doc.isObject()) {
                NotificationManager::instance().showMessage("上传失败：响应格式错误", NotificationManager::Error, this);

                return;
            }
            QJsonObject obj = doc.object();

            // 检查是否有错误
            if (obj.contains("error")) {
                NotificationManager::instance().showMessage(QString("上传失败：%1").arg(obj["error"].toString()), NotificationManager::Error, this);

                return;
            }

            // 成功响应中获取url
            if (obj.contains("url")) {
                // 保存头像URL
                this->avatarUrl = obj["url"].toString();

                // 更新头像显示
                QPixmap newAvatar(tempPath); // 使用处理后的正方形图片
                // if (!newAvatar.isNull()) {
                // ui->userHead->setIcon(createRoundedPixmap(newAvatar));
                // }
                NotificationManager::instance().showMessage("头像上传成功", NotificationManager::Success, this);
            }
        });
    }

    void Register::requestVerificationCode(const QString& 邮箱) {
        ui->getCodeButton->setEnabled(false);

        QJsonObject body;

        body["email"] = 邮箱;

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
                NotificationManager::instance().showMessage(QString("获取验证码失败：网络错误 - %1").arg(reply->errorString()), NotificationManager::Error, this);
                ui->getCodeButton->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 解析响应数据
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error != QJsonParseError::NoError) {
                NotificationManager::instance().showMessage("获取验证码失败：返回数据格式错误", NotificationManager::Error, this);
                ui->getCodeButton->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            QJsonObject obj = doc.object();

            // 处理错误响应
            if ((statusCode == 400) || obj.contains("error")) {
                QString errorMsg = obj["error"].toString();
                NotificationManager::instance().showMessage(QString("获取验证码失败：%1").arg(errorMsg), NotificationManager::Error, this);
                ui->getCodeButton->setEnabled(true);
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            NotificationManager::instance().showMessage("获取验证码成功", NotificationManager::Success, this);

            // 倒计时60秒
            int countdown = 60;
            QTimer* timer = new QTimer(this);
            connect(timer, &QTimer::timeout, this, [this, timer, countdown]() mutable {
                if (--countdown <= 0) {
                    timer->stop();
                    timer->deleteLater();
                    ui->getCodeButton->setEnabled(true);
                    ui->getCodeButton->setText("获取验证码");
                } else {
                    ui->getCodeButton->setText(QString("%1s").arg(countdown));
                }
            });
            timer->start(1000);
            reply->deleteLater();
            manager->deleteLater();
        });
    }

    bool Register::函数_检查输入框信息合法性(QString 昵称, QString 邮箱, QString 验证码, QString 密码, QString 确认密码) {
        if (昵称.isEmpty()) {
            NotificationManager::instance().showMessage("请输入用户名", NotificationManager::Error, this);

            return (false);
        } else if (邮箱.isEmpty()) {
            NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);

            return (false);
        } else if (!Util::Validator::函数_是否为有效邮箱(邮箱)) {
            NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);

            return (false);
        } else if (验证码.isEmpty()) {
            NotificationManager::instance().showMessage("请输入验证码", NotificationManager::Error, this);

            return (false);
        } else if (密码.isEmpty()) {
            NotificationManager::instance().showMessage("请输入密码", NotificationManager::Error, this);

            return (false);
        } else if (确认密码.isEmpty()) {
            NotificationManager::instance().showMessage("请确认密码", NotificationManager::Error, this);

            return (false);
        } else if (密码 != 确认密码) {
            NotificationManager::instance().showMessage("两次输入的密码不一致", NotificationManager::Error, this);

            return (false);
        } else if (avatarUrl.isEmpty()) {
            NotificationManager::instance().showMessage("请上传头像", NotificationManager::Error, this);

            return (false);
        } else {
            return (true);
        }
    }

    void Register::函数_注册(const QString& 昵称, const QString& 邮箱, const QString& 验证码, const QString& 密码) {
        if (正在注册) {
            return;
        }
        正在注册 = true;
        ui->registerButton->setEnabled(false);
        ui->registerButton->setText("注册中...");

        QJsonObject body;

        body["email"] = 邮箱;
        body["varifycode"] = 验证码;
        body["user"] = 昵称;
        body["passwd"] = 密码;
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
                               正在注册 = false;
                               ui->registerButton->setEnabled(true);
                               ui->registerButton->setText("注册");
                           };
            QByteArray responseData = reply->readAll();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // 处理网络错误，但排除400状态码
            if ((reply->error() != QNetworkReply::NoError) && (statusCode != 400)) {
                NotificationManager::instance().showMessage(QString("注册失败：网络错误 - %1").arg(reply->errorString()), NotificationManager::Error, this);
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 解析响应数据
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error != QJsonParseError::NoError) {
                NotificationManager::instance().showMessage("注册失败：返回数据格式错误", NotificationManager::Error, this);
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }
            QJsonObject obj = doc.object();

            // 处理错误响应
            if ((statusCode == 400) || obj.contains("error")) {
                QString errorMsg = obj["error"].toString();
                NotificationManager::instance().showMessage(QString("注册失败：%1").arg(errorMsg), NotificationManager::Error, this);
                cleanup();
                reply->deleteLater();
                manager->deleteLater();

                return;
            }

            // 注册成功
            NotificationManager::instance().showMessage("注册成功", NotificationManager::Success, this);

            // 保存当前输入的邮箱和密码
            QString currentEmail = ui->usernameEdit->text().trimmed();
            QString currentPassword = ui->passwordEdit->text();

            // 2秒后关闭窗口并发送信号
            QTimer::singleShot(2000, this, [this, currentEmail, currentPassword]() {
                emit registerSuccess(currentEmail, currentPassword);
                this->close();
            });
            cleanup();
            reply->deleteLater();
            manager->deleteLater();
        });
    }
}
