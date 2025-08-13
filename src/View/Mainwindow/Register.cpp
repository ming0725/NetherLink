/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QBoxLayout>
#include <QFileDialog>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include "Network/NetworkConfig.h"
#include "View/Mainwindow/NotificationManager.h"
#include "View/Mainwindow/Register.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// 定义静态成员变量
const QRegularExpression Register::邮箱正则表达式("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

Register::Register(QWidget* parent) : FramelessWindow(parent) {
    // 固定窗口大小
    this->setFixedSize(320, 580);
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
    this->closeButton = new QLabel(this);
    this->closeButton->setFixedSize(50, 50);
    this->closeButton->setPixmap(QIcon(":/icon/close.png").pixmap(30, 30));
    this->closeButton->setCursor(Qt::PointingHandCursor);
    this->closeButton->installEventFilter(this);
    top_hbox->addWidget(this->closeButton, 0, Qt::AlignTop | Qt::AlignRight);
    main_vbox->addLayout(top_hbox);

    // --------------------------
    // 2. 用户头像
    // --------------------------
    this->userHead = new QLabel(this);
    this->userHead->setFixedSize(80, 80);
    this->userHead->setScaledContents(true);
    this->userHead->setCursor(Qt::PointingHandCursor);

    // 设置默认头像并使其圆形
    QPixmap defaultAvatar(":/icon/icon.png");

    defaultAvatar = defaultAvatar.scaled(QSize(this->userHead->size()) * 1.5, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    this->userHead->setPixmap(createRoundedPixmap(defaultAvatar));

    // 为头像标签安装事件过滤器
    this->userHead->installEventFilter(this);

    // --------------------------
    // 3. 输入框和按钮
    // --------------------------
    this->usernameEdit = new LineEditComponent(this);
    this->verificationCodeEdit = new LineEditComponent(this);
    this->nicknameEdit = new LineEditComponent(this);
    this->passwordEdit = new LineEditComponent(this);
    this->confirmPasswordEdit = new LineEditComponent(this);
    this->usernameEdit->setIcon(QPixmap(":/icon/skull.png"));
    this->verificationCodeEdit->setIcon(QPixmap(":/icon/lock.png"));
    this->nicknameEdit->setIcon(QPixmap(":/icon/skull.png"));
    this->passwordEdit->setIcon(QPixmap(":/icon/lock.png"));
    this->confirmPasswordEdit->setIcon(QPixmap(":/icon/lock.png"));

    QFont font;

    font.setPixelSize(14);

    // 设置输入框样式
    for (auto* edit : {usernameEdit, verificationCodeEdit, nicknameEdit, passwordEdit, confirmPasswordEdit}) {
        edit->setIconSize(QSize(20, 20));
        edit->getLineEdit()->setAlignment(Qt::AlignCenter);
        edit->setFixedHeight(40);
        edit->getLineEdit()->setFont(font);
        edit->setFocusPolicy(Qt::StrongFocus);  // 确保LineEditComponent可以接收焦点
    }
    usernameEdit->getLineEdit()->setPlaceholderText("输入邮箱");
    verificationCodeEdit->getLineEdit()->setPlaceholderText("输入验证码");
    nicknameEdit->getLineEdit()->setPlaceholderText("输入用户名");
    passwordEdit->getLineEdit()->setPlaceholderText("输入密码");
    confirmPasswordEdit->getLineEdit()->setPlaceholderText("确认密码");
    passwordEdit->getLineEdit()->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit->getLineEdit()->setEchoMode(QLineEdit::Password);

    // 验证码按钮
    this->getCodeButton = new CustomPushButton("获取验证码", this);
    this->getCodeButton->setFixedSize(100, 40);
    this->getCodeButton->setCursor(Qt::PointingHandCursor);
    this->getCodeButton->setFocusPolicy(Qt::NoFocus);  // 禁止获取焦点
    this->getCodeButton->setPrimaryStyle();

    // 注册按钮
    this->registerButton = new CustomPushButton("注册", this);
    this->registerButton->setFixedHeight(40);
    this->registerButton->setCursor(Qt::PointingHandCursor);
    this->registerButton->setFocusPolicy(Qt::NoFocus);  // 禁止获取焦点
    this->registerButton->setPrimaryStyle();

    // 验证码输入框和按钮的水平布局
    QHBoxLayout* code_hbox = new QHBoxLayout;

    code_hbox->setSpacing(10);
    code_hbox->addWidget(this->verificationCodeEdit);
    code_hbox->addWidget(this->getCodeButton);

    // 主布局
    QVBoxLayout* vbox_1 = new QVBoxLayout;

    vbox_1->setContentsMargins(20, 20, 20, 11);
    vbox_1->addWidget(this->usernameEdit);
    vbox_1->addSpacing(10);
    vbox_1->addLayout(code_hbox);
    vbox_1->addSpacing(10);
    vbox_1->addWidget(this->nicknameEdit);
    vbox_1->addSpacing(10);
    vbox_1->addWidget(this->passwordEdit);
    vbox_1->addSpacing(10);
    vbox_1->addWidget(this->confirmPasswordEdit);
    vbox_1->addSpacing(30);
    vbox_1->addWidget(this->registerButton);
    main_vbox->addSpacing(10);
    main_vbox->addWidget(this->userHead, 0, Qt::AlignCenter);
    main_vbox->addSpacing(45);
    main_vbox->addLayout(vbox_1);
    main_vbox->addStretch();

    // 连接信号槽
    connect(this->getCodeButton, &CustomPushButton::clicked, this, [=, this]() {
        QString email = this->usernameEdit->currentText().trimmed();

        if (email.isEmpty()) {
            NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);

            return;
        }

        if (!函数_是否为有效邮箱(email)) {
            NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);

            return;
        }
        requestVerificationCode(email);
    });
    connect(this->registerButton, &CustomPushButton::clicked, this, [=, this]() {
        if (isRegistering)
            return;
        QString email = this->usernameEdit->currentText().trimmed();
        QString code = this->verificationCodeEdit->currentText().trimmed();
        QString nickname = this->nicknameEdit->currentText().trimmed();
        QString 密码 = this->passwordEdit->currentText();
        QString confirmPassword = this->confirmPasswordEdit->currentText();

        // 检查所有输入是否为空
        if (email.isEmpty()) {
            NotificationManager::instance().showMessage("请输入邮箱", NotificationManager::Error, this);

            return;
        }

        if (code.isEmpty()) {
            NotificationManager::instance().showMessage("请输入验证码", NotificationManager::Error, this);

            return;
        }

        if (nickname.isEmpty()) {
            NotificationManager::instance().showMessage("请输入用户名", NotificationManager::Error, this);

            return;
        }

        if (密码.isEmpty()) {
            NotificationManager::instance().showMessage("请输入密码", NotificationManager::Error, this);

            return;
        }

        if (confirmPassword.isEmpty()) {
            NotificationManager::instance().showMessage("请确认密码", NotificationManager::Error, this);

            return;
        }

        // 检查头像是否上传
        if (avatarUrl.isEmpty()) {
            NotificationManager::instance().showMessage("请上传头像", NotificationManager::Error, this);

            return;
        }

        // 检查两次密码是否一致
        if (密码 != confirmPassword) {
            NotificationManager::instance().showMessage("两次输入的密码不一致", NotificationManager::Error, this);

            return;
        }

        // 检查邮箱格式
        if (!函数_是否为有效邮箱(email)) {
            NotificationManager::instance().showMessage("邮箱格式不正确", NotificationManager::Error, this);

            return;
        }

        // 验证通过，发送注册请求
        doRegister(email, code, 密码, nickname);
    });

    // 设置Tab顺序
    QWidget::setTabOrder(usernameEdit, verificationCodeEdit);
    QWidget::setTabOrder(verificationCodeEdit, nicknameEdit);
    QWidget::setTabOrder(nicknameEdit, passwordEdit);
    QWidget::setTabOrder(passwordEdit, confirmPasswordEdit);
    QWidget::setTabOrder(confirmPasswordEdit, usernameEdit);

    // 禁用所有按钮的Tab焦点
    closeButton->setFocusPolicy(Qt::NoFocus);
    getCodeButton->setFocusPolicy(Qt::NoFocus);
    registerButton->setFocusPolicy(Qt::NoFocus);

    // 回车键处理
    connect(usernameEdit->getLineEdit(), &QLineEdit::returnPressed, this, [=, this]() {
        verificationCodeEdit->getLineEdit()->setFocus();
    });
    connect(verificationCodeEdit->getLineEdit(), &QLineEdit::returnPressed, this, [=, this]() {
        nicknameEdit->getLineEdit()->setFocus();
    });
    connect(nicknameEdit->getLineEdit(), &QLineEdit::returnPressed, this, [=, this]() {
        passwordEdit->getLineEdit()->setFocus();
    });
    connect(passwordEdit->getLineEdit(), &QLineEdit::returnPressed, this, [=, this]() {
        confirmPasswordEdit->getLineEdit()->setFocus();
    });
    connect(confirmPasswordEdit->getLineEdit(), &QLineEdit::returnPressed, this, [=, this]() {
        if (registerButton->isEnabled()) {
            registerButton->click();
        }
    });
    QTimer::singleShot(50, this, [=, this]() {
        usernameEdit->getLineEdit()->clearFocus();
        verificationCodeEdit->getLineEdit()->clearFocus();
    });
}

Register::~Register() {}

void Register::paintEvent(QPaintEvent* event) {
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

void Register::requestVerificationCode(const QString& 邮箱) {
    getCodeButton->setEnabled(false);

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
            getCodeButton->setEnabled(true);
            reply->deleteLater();
            manager->deleteLater();

            return;
        }

        // 解析响应数据
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonError);

        if (jsonError.error != QJsonParseError::NoError) {
            NotificationManager::instance().showMessage("获取验证码失败：返回数据格式错误", NotificationManager::Error, this);
            getCodeButton->setEnabled(true);
            reply->deleteLater();
            manager->deleteLater();

            return;
        }
        QJsonObject obj = doc.object();

        // 处理错误响应
        if ((statusCode == 400) || obj.contains("error")) {
            QString errorMsg = obj["error"].toString();
            NotificationManager::instance().showMessage(QString("获取验证码失败：%1").arg(errorMsg), NotificationManager::Error, this);
            getCodeButton->setEnabled(true);
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
                getCodeButton->setEnabled(true);
                getCodeButton->setText("获取验证码");
            } else {
                getCodeButton->setText(QString("%1s").arg(countdown));
            }
        });
        timer->start(1000);
        reply->deleteLater();
        manager->deleteLater();
    });
}

void Register::doRegister(const QString& 邮箱, const QString& code, const QString& 密码, const QString& nickname) {
    if (isRegistering)
        return;
    isRegistering = true;
    registerButton->setEnabled(false);
    registerButton->setText("注册中...");

    QJsonObject body;

    body["email"] = 邮箱;
    body["varifycode"] = code;
    body["user"] = nickname;
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
                           isRegistering = false;
                           registerButton->setEnabled(true);
                           registerButton->setText("注册");
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
        QString currentEmail = usernameEdit->currentText().trimmed();
        QString currentPassword = passwordEdit->currentText();

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

bool Register::eventFilter(QObject* target, QEvent* event) {
    if (target == this->closeButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            QTimer::singleShot(150, this, &Register::close);

            return (true);
        }
    }

    if ((target == this->userHead) && (event->type() == QEvent::MouseButtonPress)) {
        QString filePath = QFileDialog::getOpenFileName(this, "选择头像", QString(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");

        if (!filePath.isEmpty()) {
            uploadAvatar(filePath);
        }
        return (true);
    }
    return (QWidget::eventFilter(target, event));
}

void Register::uploadAvatar(const QString& filePath) {
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
        file->remove();  // 删除临时文件
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

            if (!newAvatar.isNull()) {
                this->userHead->setPixmap(createRoundedPixmap(newAvatar));
            }
            NotificationManager::instance().showMessage("头像上传成功", NotificationManager::Success, this);
        }
    });
}

QPixmap Register::createRoundedPixmap(const QPixmap& source) {
    if (source.isNull())
        return (QPixmap());
    QPixmap scaled = source.scaled(this->userHead->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap rounded(scaled.size());

    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;

    path.addEllipse(rounded.rect());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);

    return (rounded);
}

void Register::mousePressEvent(QMouseEvent* event) {
    QWidget* fw = QApplication::focusWidget();

    if (qobject_cast <LineEditComponent*>(fw) || qobject_cast <QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    FramelessWindow::mousePressEvent(event);
}

bool Register::函数_是否为有效邮箱(const QString& 邮箱) const {
    return (邮箱正则表达式.match(邮箱).hasMatch());
}
