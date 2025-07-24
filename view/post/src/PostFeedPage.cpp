
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "PostFeedPage.h"

#include <QJsonArray>

#include <QJsonObject>
#include <QNetworkReply>

#include "CurrentUser.h"
#include "NetworkConfig.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

PostFeedPage::PostFeedPage(QWidget* parent) : CustomScrollArea(parent) {
// connect(this, &CustomScrollArea::reachedBottom, this, [this]() {
// QTimer::singleShot(100, this, &PostFeedPage::loadMore);
// });
    setStyleSheet("border-width:0px;border-style:solid;");
}

void PostFeedPage::setPosts(const QVector <Post>& posts) {
    qDeleteAll(m_items);
    m_items.clear();
    m_data = posts;

    for (int i = 0; i < m_data.size(); ++i) {
        const auto& pd = m_data[i];

        // 传入 title 字段
        auto*item = new PostPreviewItem(pd, contentWidget);

        connect(item, &PostPreviewItem::viewPostWithGeometry, this, &PostFeedPage::postClickedWithGeometry);
        connect(item, &PostPreviewItem::loadFinished, this, [this] () {
            CustomScrollArea::resizeEvent(nullptr);
        });
        m_items.append(item);
    }
}

void PostFeedPage::layoutContent() {
    int W = viewport()->width();
    int availableW = W - 2 * margin;
    int cols = std::max(1, (availableW + hgap) / (minItemW + hgap));
    double itemW = (availableW - (cols - 1) * hgap) / double(cols);
    QVector <int> colH(cols, topMargin);

    for (auto*it : m_items) {
        int h = it->scaledHeightFor(itemW);
        int col = std::min_element(colH.begin(), colH.end()) - colH.begin();
        int x = margin + col * (itemW + hgap);
        int y = colH[col];

        it->setGeometry(x, y, int(itemW), h);
        colH[col] += h + vgap;
    }

    int maxH = *std::max_element(colH.begin(), colH.end());

    contentWidget->resize(W, maxH + margin);
}

void PostFeedPage::loadPosts() {
    // 创建网络管理器和请求对象
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QString baseHttpUrl = NetworkConfig::instance().getHttpAddress();
    QNetworkRequest request(QUrl(baseHttpUrl + "/api/posts"));

    // 设置请求头
    QString token = CurrentUser::instance().getToken();

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    // 发送GET请求
    QNetworkReply* reply = manager->get(request);

    // 处理响应
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager] () {
        // 设置自动删除
        reply->deleteLater();
        manager->deleteLater();

        // 检查是否有错误
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "网络请求错误:" << reply->errorString();

            return;
        }

        // 读取响应数据
        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);

        if ((err.error != QJsonParseError::NoError) || !doc.isObject()) {
            qWarning() << "帖子列表解析失败";

            return;
        }
        QJsonObject obj = doc.object();
        QJsonArray postsArray = obj["posts"].toArray();
        qDebug() << "已获取帖子列表，正在加载...";

        // 转换为Post对象
        QVector <Post> posts;

        for (const QJsonValue& postVal : postsArray) {
            QJsonObject postObj = postVal.toObject();
            Post post;
            post.authorName = postObj["user_name"].toString();
            post.postID = QString::number(postObj["post_id"].toInt());
            post.title = postObj["title"].toString();
            post.authorID = postObj["user_id"].toString();
            post.firstImage = postObj["first_image"].toString();
            post.likes = postObj["likes_count"].toInt();
            post.isLiked = postObj["is_liked"].toBool();
            post.authorAvaUrl = postObj["user_avatar"].toString();
            posts.append(post);
        }

        // 设置帖子数据
        setPosts(posts);
        qDebug() << "已获取帖子列表，加载完成，帖子共计" << posts.size() << "个";
        m_needReload = false;
    });
}

void PostFeedPage::reloadData() {
    m_needReload = true;

    if (isVisible()) {
        loadPosts();
    }
}

void PostFeedPage::showEvent(QShowEvent*event) {
    if (m_isFirstShow || m_needReload) {
        loadPosts();
        m_isFirstShow = false;
    } else {
        layoutContent();
    }
    QWidget::showEvent(event);
}
