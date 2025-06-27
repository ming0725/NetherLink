#include "PostPreviewItem.h"
#include "ClickableLabel.h"
#include <QResizeEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include "Post.h"
#include "AvatarLoader.h"
#include "CurrentUser.h"
#include "NotificationManager.h"
#include "NetworkConfig.h"

PostPreviewItem::PostPreviewItem(const Post& post,
                                 QWidget* parent)
        : QWidget(parent)
        , m_titleText(post.title)
        , m_likeCount(post.likes)
        , m_liked(post.isLiked)
        , m_authorID(post.authorID)
        , m_postImageUrl(post.firstImage)
        , m_postID(post.postID)
{
    // 子控件
    m_imageLabel      = new ClickableLabel(this);
    m_titleLabel      = new ClickableLabel(this);
    m_avatarLabel     = new AvatarLabel(this);
    m_authorLabel     = new ClickableLabel(this);
    m_likeIconLabel   = new ClickableLabel(this);
    m_likeCountLabel  = new ClickableLabel(this);
    m_authorName = post.authorName;
    m_avatarLabel->loadAvatar(post.authorID, post.authorAvaUrl);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_authorLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_likeCountLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_avatarLabel->setFixedSize(AvatarR, AvatarR);

    QFont font = m_avatarLabel->font();
    font.setPointSize(10);
    m_titleLabel->setFont(font);
    font.setPointSize(9);
    m_likeCountLabel->setFont(font);
    m_authorLabel->setFont(font);
    m_likeIconLabel->setFixedSize(20,20);

    setupUI();
    downloadAndSetupImages();

    // —— 其它信号槽连接 —— //
    connect(m_imageLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewPost);
    connect(m_titleLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewPost);
    connect(m_authorLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewAuthor);
    connect(m_likeIconLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onClickLike);
    connect(m_likeCountLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onClickLike);
}

void PostPreviewItem::downloadAndSetupImages() {
    // 下载帖子图片
    QNetworkRequest request;
    request.setUrl(QUrl(m_postImageUrl));
    QNetworkReply* reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap origPostImage;
            if (!origPostImage.loadFromData(data)) {
                qWarning() << "Failed to load image data";
                reply->deleteLater();
                return;
            }
            m_originalImage = origPostImage.copy();
            // 裁剪图片
            int w0 = MinWidth;
            double ratio = double(origPostImage.width()) / double(origPostImage.height());
            int h0 = int(w0 / ratio);
            QPixmap tmp = origPostImage.scaled(w0,
                                               h0,
                                               Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
            if (h0 > MaxImgH) {
                // 中心裁掉上下
                int yoff = (h0 - MaxImgH) / 2;
                m_croppedPostImage = tmp.copy(0, yoff, w0, MaxImgH);
            } else {
                m_croppedPostImage = tmp;
            }

            m_imageLabel->setRoundedPixmap(m_croppedPostImage, 12);
            // 触发重新布局
            resizeEvent(nullptr);
            show();
            emit loadFinished();
        }
        reply->deleteLater();
    });
}

void PostPreviewItem::setupUI()
{
    m_authorLabel->setText(m_authorName);
    m_likeCountLabel->setText(QString::number(m_likeCount));
    m_titleLabel->setText(m_titleText);
    QPixmap fullHeart, emptyHeart;
    if (!QPixmapCache::find("full_heart", &fullHeart)) {
        fullHeart = QPixmap(":/resources/icon/full_heart.png")
                .scaled(20,
                        20,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
        QPixmapCache::insert("full_heart", fullHeart);
    }
    if (!QPixmapCache::find("empty_heart", &emptyHeart)) {
        emptyHeart = QPixmap(":/resources/icon/heart.png")
                .scaled(20,
                        20,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
        QPixmapCache::insert("empty_heart", emptyHeart);
    }

    if (m_liked) {
        m_likeIconLabel->setPixmap(fullHeart);
    }
    else {
        m_likeIconLabel->setPixmap(emptyHeart);
    }
}

void PostPreviewItem::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    int W = width();

    int currentY = 0;

    // 1) 图片部分
    int imgH = m_croppedPostImage.isNull() ? MinWidth : int(W / (double(m_croppedPostImage.width()) / double(m_croppedPostImage.height())));
    m_imageLabel->setGeometry(0, currentY, W, imgH);
    if (!m_croppedPostImage.isNull()) {
        m_imageLabel->setRoundedPixmap(m_croppedPostImage.scaled(W, imgH,
                                                                 Qt::KeepAspectRatio,
                                                                 Qt::SmoothTransformation), 12);
    }
    currentY += imgH;

    // 2) 标题部分
    currentY += Margin;
    QFontMetrics fm(m_titleLabel->font());
    int lineH = fm.lineSpacing();
    int maxTitleH = lineH * 2;
    QRect rText = fm.boundingRect(0, 0, W - 2*Margin,
                                  maxTitleH,
                                  Qt::TextWordWrap,
                                  m_titleText);
    int titleH = qMin(rText.height(), maxTitleH);
    m_titleLabel->setGeometry(Margin, currentY, W - 2*Margin, titleH + 3);
    currentY += titleH;

    // 3) 底部行：头像 / 作者名 / 点赞图标 / 点赞数
    currentY += Margin;
    m_avatarLabel->setGeometry(Margin, currentY, AvatarR, AvatarR);

    int iconW = m_likeIconLabel->width();
    int countW = fm.horizontalAdvance(m_likeCountLabel->text());
    int likeIconX = W - 1.2 * Margin - countW - iconW;
    m_likeIconLabel->setGeometry(likeIconX,
                                 currentY + (AvatarR-iconW)/2,
                                 iconW,
                                 iconW);
    m_likeCountLabel->setGeometry(W - Margin - countW,
                                  currentY,
                                  countW,
                                  AvatarR);

    int authorX = AvatarR + Margin + Margin;
    m_authorLabel->setGeometry(authorX,
                               currentY,
                               likeIconX - authorX - Margin,
                               AvatarR);
    currentY += AvatarR + Margin;

    setFixedHeight(currentY);
}

int PostPreviewItem::scaledHeightFor(double itemW) {
    QSize oldSize = size();
    resize(itemW, oldSize.height());
    int newH = height();
    resize(oldSize);
    return newH;
}


void PostPreviewItem::onViewPost() {
    // 获取当前项在全局坐标系中的位置和大小
    QRect globalGeometry = QRect(mapToGlobal(QPoint(0, 0)), size());
    emit viewPostWithGeometry(m_postID, globalGeometry, getOriginalImage());
}

void PostPreviewItem::onViewAuthor() {
    emit viewAuthor();
}

void PostPreviewItem::onClickLike() {
    // 保留原状态，以便出错时回滚
    bool oldLiked = m_liked;
    int oldCount = m_likeCount;

    // 切换本地状态并更新 UI
    m_liked = !oldLiked;
    m_likeCount = oldCount + (m_liked ? 1 : -1);
    setupUI();
    resizeEvent(nullptr);

    // 构造并发送网络请求
    QString baseUrl = NetworkConfig::instance().getHttpAddress();
    QNetworkRequest request(QUrl(QString(baseUrl + "/api/posts/%1/like").arg(m_postID)));

    // 设置 Authorization 头
    QString token = CurrentUser::instance().getToken();
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    QNetworkReply* reply = m_networkManager.post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, oldLiked, oldCount]() {
        if (reply->error() != QNetworkReply::NoError) {
            // 网络或服务器错误，回滚状态并通知
            m_liked = oldLiked;
            m_likeCount = oldCount;
            setupUI();
            resizeEvent(nullptr);
            NotificationManager::instance()
                    .showMessage(tr("点赞失败: %1").arg(reply->errorString()),
                                 NotificationManager::Error,
                                 this);
        }
        reply->deleteLater();
    });
}

void PostPreviewItem::showEvent(QShowEvent *ev) {
    QWidget::showEvent(ev);
}
