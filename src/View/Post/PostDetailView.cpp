/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPainterPath>

#include "Data/AvatarLoader.h"
#include "Data/CurrentUser.h"
#include "Network/NetworkConfig.h"
#include "View/Mainwindow/MainWindow.h"
#include "View/Mainwindow/NotificationManager.h"
#include "View/Post/PostDetailView.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

void PostDetailScrollArea::layoutContent() {
    if (!contentWidget || !m_titleLabel || !m_contentLabel)
        return;
    const int margin = 20;

    const int spacing = 20;
    int currentY = margin;

    // 设置标题
    QFontMetrics titleMetrics(m_titleLabel->font());
    int titleWidth = viewport()->width() - 2 * margin;
    int titleHeight = titleMetrics.boundingRect(0, 0, titleWidth, 0, Qt::TextWordWrap, m_titleLabel->text()).height();

    m_titleLabel->setGeometry(margin, currentY, titleWidth, titleHeight);
    currentY += titleHeight + spacing;

    // 设置内容
    QFontMetrics contentMetrics(m_contentLabel->font());
    int contentWidth = viewport()->width() - 2 * margin;
    int contentHeight = contentMetrics.boundingRect(0, 0, contentWidth, 0, Qt::TextWordWrap, m_contentLabel->text()).height();

    m_contentLabel->setGeometry(margin, currentY, contentWidth, contentHeight);
    currentY += contentHeight + spacing;

    // 设置评论
    for (QWidget* commentWidget : m_commentWidgets) {
        if (commentWidget && (commentWidget->parent() == contentWidget)) {
            commentWidget->setGeometry(margin, currentY, viewport()->width() - 2 * margin, 80);
            commentWidget->show();  // 确保评论控件可见
            currentY += 80 + spacing;
        }
    }

    // 设置内容区域大小
    contentWidget->setFixedSize(viewport()->width(), currentY);
}

void PostDetailScrollArea::addCommentWidget(QWidget* commentWidget) {
    if (commentWidget) {
        commentWidget->setParent(contentWidget);  // 确保父窗口设置正确
        m_commentWidgets.append(commentWidget);
        layoutContent();
    }
}

PostDetailScrollArea::PostDetailScrollArea(QWidget* parent) : CustomScrollArea(parent) {
    contentWidget->setObjectName("PostDetailContentWidget");
}

void PostDetailScrollArea::setLabels(QLabel* titleLabel, QLabel* contentLabel) {
    m_titleLabel = titleLabel;
    m_contentLabel = contentLabel;
}

PostDetailView::PostDetailView(QWidget* parent) : QWidget(parent) {
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
    m_loadedImage = QPixmap();
}

void PostDetailView::setupUI() {
    // 1. 创建所有控件
    m_authorAvatar = new AvatarLabel(this);
    m_authorAvatar->setAvatarSize(40);
    commentLineEdit = new LineEditComponent(this);
    commentLineEdit->setIcon(QPixmap(":/icon/selected_message.png"));
    commentLineEdit->getLineEdit()->setPlaceholderText("说点什么吧...");
    m_authorName = new QLabel(this);

    QFont nameFont;

    nameFont.setStyleStrategy(QFont::PreferAntialias);
    nameFont.setFamily("微软雅黑");
    nameFont.setPointSize(13);
    nameFont.setBold(true);
    m_authorName->setFont(nameFont);
    m_followBtn = new QPushButton("关注", this);
    m_followBtn->setFixedSize(80, 32);
    m_followBtn->setCursor(Qt::PointingHandCursor);
    m_contentArea = new PostDetailScrollArea(this);
    m_contentWidget = m_contentArea->getContentWidget();
    m_contentArea->setObjectName("ContentArea");
    m_contentArea->setStyleSheet("#ContentArea { border-top: 2px solid #f5f5f5; border-bottom: 2px solid #f5f5f5; border-left: none; border-right: none; }");
    m_titleLabel = new QLabel(m_contentWidget);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont titleFont;

    titleFont.setStyleStrategy(QFont::PreferAntialias);
    titleFont.setFamily("微软雅黑");
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_contentLabel = new QLabel(m_contentWidget);
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont contentFont;

    contentFont.setStyleStrategy(QFont::PreferAntialias);
    contentFont.setFamily("微软雅黑");
    contentFont.setPointSize(13);
    m_contentLabel->setFont(contentFont);

    // 设置 PostDetailScrollArea 的标签引用
    static_cast <PostDetailScrollArea*>(m_contentArea)->m_titleLabel = m_titleLabel;
    static_cast <PostDetailScrollArea*>(m_contentArea)->m_contentLabel = m_contentLabel;
    m_likeBtn = new QPushButton(this);
    m_likeBtn->setFixedSize(32, 32);
    m_likeBtn->setCursor(Qt::PointingHandCursor);
    m_likeBtn->setFlat(true);
    m_likeBtn->setIcon(QIcon(":/icon/heart.png"));
    m_likeBtn->setStyleSheet(QString("QPushButton:pressed {""  background: none;""  border: none;""}"));
    m_likeCount = new QLabel("666", this);
    m_commentBtn = new QPushButton(this);
    m_commentBtn->setFixedSize(32, 32);
    m_commentBtn->setCursor(Qt::PointingHandCursor);
    m_commentBtn->setIcon(QIcon(":/icon/selected_message.png"));
    m_commentBtn->setFlat(true);
    m_commentBtn->setStyleSheet(QString("QPushButton:pressed {""  background: none;""  border: none;""}"));
    m_commentCount = new QLabel("91", this);

    // 3. 连接信号槽
    connect(m_followBtn, &QPushButton::clicked, this, [this] () {
        m_isFollowed = !m_isFollowed;
        m_followBtn->setText(m_isFollowed ? "已关注" : "关注");
        emit followClicked(m_isFollowed);
    });
    connect(m_likeBtn, &QPushButton::clicked, this, [this] () {
        m_isLiked = !m_isLiked;
        m_likeBtn->setIcon(QIcon(m_isLiked ? ":/icon/full_heart.png" : ":/icon/heart.png"));
        m_likes += m_isLiked ? 1 : -1;
        m_likeCount->setText(QString::number(m_likes));
        emit likeClicked(m_isLiked);
    });
    connect(m_commentBtn, &QPushButton::clicked, this, &PostDetailView::commentClicked);

    // 连接评论输入框的回车信号
    connect(commentLineEdit->getLineEdit(), &QLineEdit::returnPressed, this, [this] () {
        QString content = commentLineEdit->getLineEdit()->text().trimmed();

        if (!content.isEmpty()) {
            addComment(content);
            sendComment(content);
            commentLineEdit->getLineEdit()->clear();
        }
    });
}

void PostDetailView::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    updateLayout();
}

void PostDetailView::paintEvent(QPaintEvent*) {
    QPainter p(this);

    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // 圆角白色背景
    QPainterPath path;

    path.addRoundedRect(rect(), 12, 12);
    p.fillPath(path, Qt::white);

    // 左侧图片区域
    int w = width();
    int h = height();
    int imgW = w * 0.6;
    QRect imageRect(0, 0, imgW, h);
    QPainterPath imgPath;

    imgPath.addRoundedRect(imageRect, 12, 12);
    p.setClipPath(imgPath);

    if (!m_loadedImage.isNull()) {
        // 保持图片比例并居中绘制
        QSize scaledSize = m_loadedImage.size();

        scaledSize.scale(imgW, h, Qt::KeepAspectRatio);

        int x = (imgW - scaledSize.width()) / 2;
        int y = (h - scaledSize.height()) / 2;

        // 先补灰底
        p.fillRect(imageRect, QColor(0xf5f5f5));

        // 再画图
        p.drawPixmap(x, y, m_loadedImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // 没有图片时补灰色背景
        p.fillRect(imageRect, QColor(0xf5f5f5));
    }
}

void PostDetailView::updateLayout() {
    const int w = width();
    const int h = height();
    const int imgW = w * 0.6;  // 图片区域占60%宽度

    // 1. 左侧图片区域
    m_authorAvatar->setGeometry(0, 0, imgW, h);

    // 2. 右侧区域
    const int rightX = imgW;
    const int rightW = w - imgW;

    // 2.1 顶部作者信息区域
    const int topH = 60;
    const int avatarX = rightX + 20;
    const int avatarY = 10;

    m_authorAvatar->setGeometry(avatarX, avatarY, 40, 40);
    m_authorName->setGeometry(avatarX + 50, avatarY, 200, 40);
    m_followBtn->setGeometry(rightX + rightW - 100, avatarY + 4, 80, 32);

    // 2.2 底部操作区域
    const int bottomH = 60;
    const int bottomY = h - bottomH;
    const int btnY = bottomY + (bottomH - 32) / 2;
    int x = rightX + 10;

    commentLineEdit->setGeometry(x, btnY, 160, 32);
    x += 160;
    m_likeBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_likeCount->setGeometry(x - 10, btnY, 50, 32);
    m_commentBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_commentCount->setGeometry(x - 10, btnY, 50, 32);
    m_contentArea->setGeometry(rightX, topH, rightW, h - topH - bottomH);
}

void PostDetailView::setPostData(const QJsonObject& data) {
    m_postId = QString::number(data["post_id"].toInt());

    // 1. 更新作者信息
    QJsonObject author = data["author"].toObject();

    m_authorId = author["user_id"].toString();
    m_authorName->setText(author["user_name"].toString());
    m_authorAvatar->loadAvatar(m_authorId, QUrl(author["avatar"].toString()));

    // 2. 更新内容
    m_titleLabel->setText(data["title"].toString());
    m_contentLabel->setText(data["content"].toString());

    // 3. 更新状态
    m_isLiked = data["is_liked"].toBool();
    m_likes = data["likes_count"].toInt();

    // 处理评论数组
    QJsonArray comments = data["comments"].toArray();

    m_comments = comments.size();  // 更新评论总数
    m_commentCount->setText(QString::number(m_comments));

    // 将评论数组转换为QList以便反向遍历
    QList <QJsonValue> commentsList;

    for (const QJsonValue &comment : comments) {
        commentsList.prepend(comment);  // 将新评论添加到列表开头
    }

    // 遍历评论列表，创建并添加评论部件
    for (const QJsonValue &commentVal : commentsList) {
        QJsonObject comment = commentVal.toObject();

        // 获取评论用户信息
        QString userName = comment["user_name"].toString();
        QString avatarUrl = comment["user_avatar"].toString();
        QString content = comment["content"].toString();
        QString timeStr = comment["created_at"].toString();
        QDateTime dateTime = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");

        // 创建评论部件并添加到滚动区域
        QWidget* commentWidget = createCommentWidget(userName, avatarUrl, content, dateTime.toString("MM-dd HH:mm"));

        static_cast <PostDetailScrollArea*>(m_contentArea)->addCommentWidget(commentWidget);
    }
    m_likeBtn->setIcon(QIcon(m_isLiked ? ":/icon/full_heart.png" : ":/icon/heart.png"));
    m_likeCount->setText(QString::number(m_likes));
    m_commentCount->setText(QString::number(m_comments));

    // 4. 更新布局
    updateLayout();
}

void PostDetailView::showEvent(QShowEvent* event) {
    if (m_isFirstShow && !m_initialGeometry.isNull()) {
        // 设置初始位置和大小
        setGeometry(m_initialGeometry);

        // 创建动画
        QPropertyAnimation* animation = new QPropertyAnimation(this, "geometry");

        animation->setDuration(300);  // 300毫秒的动画
        animation->setStartValue(m_initialGeometry);

        // 计算最终位置（居中）
        QRect finalGeometry;

        if (QWidget* parentWidget = this->parentWidget()) {
            int finalWidth = parentWidget->width() * 0.8;  // 80%的父窗口宽度
            int finalHeight = parentWidget->height() * 0.8; // 80%的父窗口高度
            int x = (parentWidget->width() - finalWidth) / 2;
            int y = (parentWidget->height() - finalHeight) / 2;

            finalGeometry = QRect(x, y, finalWidth, finalHeight);
        }
        animation->setEndValue(finalGeometry);
        animation->setEasingCurve(QEasingCurve::OutCubic);  // 使用缓出曲线使动画更自然
        // 开始动画
        animation->start(QAbstractAnimation::DeleteWhenStopped);
        m_isFirstShow = false;
    }
    QWidget::showEvent(event);
}

QWidget* PostDetailView::createCommentWidget(const QString& userName, const QString& avatarPath, const QString& content) {
    QWidget* commentWidget = new QWidget(m_contentWidget);

    commentWidget->setObjectName("CommentWidget");
    commentWidget->setFixedHeight(80);  // 设置固定高度
    commentWidget->setStyleSheet("background-color: white;"); // 添加背景色以便于调试

    // 创建头像
    AvatarLabel* avatar = new AvatarLabel(commentWidget);

    avatar->setAvatarSize(32);
    avatar->loadAvatar(CurrentUser::instance().getUserId(), QUrl(avatarPath));

    // 创建用户名
    QLabel* nameLabel = new QLabel(userName, commentWidget);
    QFont nameFont;

    nameFont.setFamily("微软雅黑");
    nameFont.setPointSize(12);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);

    // 创建评论内容
    QLabel* contentLabel = new QLabel(content, commentWidget);

    contentLabel->setWordWrap(true);

    QFont contentFont;

    contentFont.setFamily("微软雅黑");
    contentFont.setPointSize(12);
    contentLabel->setFont(contentFont);

    // 创建时间标签
    QLabel* timeLabel = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), commentWidget);
    QFont timeFont;

    timeFont.setFamily("微软雅黑");
    timeFont.setPointSize(10);
    timeLabel->setFont(timeFont);

    // 设置布局
    avatar->setGeometry(0, 0, 32, 32);
    nameLabel->setGeometry(40, 0, 200, 20);
    contentLabel->setGeometry(40, 20, commentWidget->width() - 50, 40);
    timeLabel->setGeometry(40, 60, 200, 20);

    // 确保所有子控件都可见
    avatar->show();
    nameLabel->show();
    contentLabel->show();
    timeLabel->show();
    commentWidget->show();

    return (commentWidget);
}

QWidget* PostDetailView::createCommentWidget(const QString& userName, const QString& avatarPath, const QString& content, const QString& timeStr) {
    QWidget* commentWidget = new QWidget(m_contentWidget);

    commentWidget->setObjectName("CommentWidget");
    commentWidget->setFixedHeight(80);  // 设置固定高度
    commentWidget->setStyleSheet("background-color: white;"); // 添加背景色以便于调试

    // 创建头像
    AvatarLabel* avatar = new AvatarLabel(commentWidget);

    avatar->setAvatarSize(32);
    avatar->loadAvatar(CurrentUser::instance().getUserId(), QUrl(avatarPath));

    // 创建用户名
    QLabel* nameLabel = new QLabel(userName, commentWidget);
    QFont nameFont;

    nameFont.setFamily("微软雅黑");
    nameFont.setPointSize(12);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);

    // 创建评论内容
    QLabel* contentLabel = new QLabel(content, commentWidget);

    contentLabel->setWordWrap(true);

    QFont contentFont;

    contentFont.setFamily("微软雅黑");
    contentFont.setPointSize(12);
    contentLabel->setFont(contentFont);

    // 创建时间标签
    QLabel* timeLabel = new QLabel(timeStr, commentWidget);
    QFont timeFont;

    timeFont.setFamily("微软雅黑");
    timeFont.setPointSize(10);
    timeLabel->setFont(timeFont);

    // 设置布局
    avatar->setGeometry(0, 0, 32, 32);
    nameLabel->setGeometry(40, 0, 200, 20);
    contentLabel->setGeometry(40, 20, commentWidget->width() - 50, 40);
    timeLabel->setGeometry(40, 60, 200, 20);

    // 确保所有子控件都可见
    avatar->show();
    nameLabel->show();
    contentLabel->show();
    timeLabel->show();
    commentWidget->show();

    return (commentWidget);
}

void PostDetailView::addComment(const QString& content) {
    QString userName = CurrentUser::instance().getUserName();
    QString avatarPath = CurrentUser::instance().getAvatarPath();
    QWidget* commentWidget = createCommentWidget(userName, avatarPath, content);

    static_cast <PostDetailScrollArea*>(m_contentArea)->addCommentWidget(commentWidget);

    // 更新评论数
    m_comments++;
    m_commentCount->setText(QString::number(m_comments));
}

void PostDetailView::sendComment(const QString &content) {
    // 获取token和主窗口
    QString token = CurrentUser::instance().getToken();
    QWidget* mainWindow = MainWindow::getInstance();

    // 构建请求URL
    QString baseUrl = NetworkConfig::instance().getHttpAddress();
    QUrl url(QString(baseUrl + "/api/posts/%1/comments").arg(m_postId));

    // 创建网络请求
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    // 构建请求体
    QJsonObject jsonObj;

    jsonObj["content"] = content;

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson();

    // 创建网络管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 发送POST请求
    QNetworkReply* reply = manager->post(request, jsonData);

    // 处理响应
    connect(reply, &QNetworkReply::finished, this, [=] () {
        reply->deleteLater();
        manager->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            // 请求成功
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);

            if (doc.isObject()) {
                // 评论成功
                NotificationManager::instance().showMessage("评论发布成功", NotificationManager::Success, mainWindow);

                // 这里可以添加刷新评论列表的代码
                // emit commentAdded(); // 如果你有这样的信号
            } else {
                NotificationManager::instance().showMessage("评论发布失败：返回数据格式错误", NotificationManager::Error, mainWindow);
            }
        } else {
            // 请求失败，解析错误信息
            QByteArray responseData = reply->readAll();
            QJsonDocument errorDoc = QJsonDocument::fromJson(responseData);
            QString errorMessage = "评论发布失败";

            if (errorDoc.isObject()) {
                QJsonObject errorObj = errorDoc.object();

                if (errorObj.contains("error")) {
                    errorMessage = errorObj["error"].toString();
                }
            }
            NotificationManager::instance().showMessage(errorMessage, NotificationManager::Error, mainWindow);
        }
    });

    // 处理网络错误
    connect(reply, &QNetworkReply::errorOccurred, this, [=] (QNetworkReply::NetworkError code) {
        QString errorMessage = QString("网络错误：%1").arg(code);
        NotificationManager::instance().showMessage(errorMessage, NotificationManager::Error, mainWindow);
    });
}
