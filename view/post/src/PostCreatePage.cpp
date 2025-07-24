
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "PostCreatePage.h"

#include <QFileDialog>

#include <QHttpMultiPart>

#include <QJsonObject>
#include <QMessageBox>

#include <QMimeDatabase>

#include <QNetworkReply>

#include "CurrentUser.h"

#include "MainWindow.h"

#include "NetworkConfig.h"

#include "NotificationManager.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

PostCreatePage::PostCreatePage(QWidget* parent) : QWidget(parent) {
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
}

void PostCreatePage::setupUI() {
    // 创建标题输入框
    m_titleEdit = new LineEditComponent(this);
    m_titleEdit->setIcon(QPixmap(":/icon/blazer.png"));
    m_titleEdit->setIconSize(QSize(20, 20));
    m_titleEdit->getLineEdit()->setPlaceholderText("请输入标题");
    m_titleEdit->setFixedHeight(40);

    // 创建内容输入框
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setPlaceholderText("请输入内容...");
    m_contentEdit->setStyleSheet("QTextEdit {""   background-color: #F5F5F5;""   border: none;""   border-radius: 10px;""   padding: 10px;""   font-size: 14px;""}""QTextEdit:focus {""   background-color: #FFFFFF;""   border: 2px solid #0099FF;""}");

    // 创建图片按钮
    m_imageButton = new QPushButton(this);
    m_imageButton->setText("+");
    m_imageButton->setStyleSheet("QPushButton {""   background-color: #F5F5F5;""   border: 2px dashed #CCCCCC;""   border-radius: 10px;""   font-size: 24px;""   color: #999999;""}""QPushButton:hover {""   background-color: #EBEBEB;""   border-color: #999999;""}");
    m_imageButton->setFixedSize(100, 100);
    m_imageButton->setCursor(Qt::PointingHandCursor);

    // 创建图片预览标签
    m_imagePreview = new QLabel(this);
    m_imagePreview->setFixedSize(100, 100);
    m_imagePreview->setScaledContents(true);
    m_imagePreview->hide();

    // 创建发送按钮
    m_sendButton = new QPushButton("发布", this);
    m_sendButton->setStyleSheet("QPushButton {""   background-color: #0099FF;""   color: white;""   border: none;""   border-radius: 5px;""   padding: 8px 20px;""   font-size: 14px;""}""QPushButton:hover {""   background-color: #0088EE;""}""QPushButton:pressed {""   background-color: #0077DD;""}");
    m_sendButton->setCursor(Qt::PointingHandCursor);

    // 连接信号槽
    connect(m_imageButton, &QPushButton::clicked, this, &PostCreatePage::onImageButtonClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &PostCreatePage::onSendButtonClicked);
}

void PostCreatePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    int w = width();
    int h = height() - 32;

    // 标题输入框
    m_titleEdit->setGeometry(MARGIN, MARGIN, w - 2 * MARGIN, 40);

    // 内容输入框
    int contentY = MARGIN + 40 + SPACING;

    m_contentEdit->setGeometry(MARGIN, contentY, w - 2 * MARGIN, h - contentY - 100 - 2 * MARGIN);

    // 图片按钮和预览
    int imageY = h - MARGIN - 100;

    m_imageButton->setGeometry(MARGIN, imageY, 100, 100);
    m_imagePreview->setGeometry(MARGIN, imageY, 100, 100);

    // 发送按钮
    int sendX = w - MARGIN - 100;
    int sendY = h - MARGIN - 40;

    m_sendButton->setGeometry(sendX, sendY, 100, 40);
}

void PostCreatePage::onImageButtonClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp)");

    if (!filePath.isEmpty()) {
        handleImageSelected(filePath);
    }
}

void PostCreatePage::handleImageSelected(const QString& path) {
    m_selectedImagePath = path;

    QPixmap pixmap(path);

    m_imagePreview->setPixmap(pixmap);
    m_imagePreview->show();
    m_imageButton->hide();
}

void PostCreatePage::onSendButtonClicked() {
    sendPost();
}

void PostCreatePage::sendPost() {
    QString title = m_titleEdit->currentText();
    QString content = m_contentEdit->toPlainText();

    if (title.isEmpty() || content.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("标题和内容不能为空"));

        return;
    }

    // 准备 JSON 数据
    QJsonObject requestData;

    requestData["title"] = title;
    requestData["content"] = content;

    QByteArray jsonBytes = QJsonDocument(requestData).toJson(QJsonDocument::Compact);

    // 创建 multipart/form-data
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 添加 JSON 数据 Part
    QHttpPart jsonPart;

    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"data\""));
    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    jsonPart.setBody(jsonBytes);
    multiPart->append(jsonPart);

    // 添加图片 Part（如果有）
    if (!m_selectedImagePath.isEmpty()) {
        QFile* file = new QFile(m_selectedImagePath);

        if (file->open(QIODevice::ReadOnly)) {
            QHttpPart imagePart;
            const QFileInfo info(*file);
            QString fileName = info.fileName();

            imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"images\"; filename=\"" + fileName + "\""));
            imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeTypeForFile(info.filePath())));
            imagePart.setBodyDevice(file);
            file->setParent(multiPart);
            multiPart->append(imagePart);
        }
    }

    // 创建网络请求
    QString baseUrl = NetworkConfig::instance().getHttpAddress();
    QNetworkRequest request(QUrl(baseUrl + "/api/posts"));

    // 设置 Authorization 头部
    QString token = CurrentUser::instance().getToken();

    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    // 注意：不要手动设置 Content-Type，QHttpMultiPart 会自动带上 boundary
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkReply* reply = manager->post(request, multiPart);

    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [=, this] () {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);

            if (doc.isObject() && (doc.object()["code"].toInt() == 0)) {
                emit postCreated();
                m_titleEdit->getLineEdit()->clear();
                m_contentEdit->clear();
                m_selectedImagePath.clear();
                m_imagePreview->hide();
                m_imageButton->show();
            } else {
                QJsonObject obj = doc.object();
                NotificationManager::instance().showMessage(tr("发布失败: %1").arg(obj.value("message").toString()), NotificationManager::Error, MainWindow::getInstance());
            }
        } else {
            NotificationManager::instance().showMessage(tr("网络错误: %1").arg(reply->errorString()), NotificationManager::Error, MainWindow::getInstance());
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

QString PostCreatePage::mimeTypeForFile(const QString& filePath) {
    QString mime = QMimeDatabase().mimeTypeForFile(filePath).name();

    return (mime);
}
