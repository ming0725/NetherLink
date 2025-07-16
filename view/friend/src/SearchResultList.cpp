#include "CurrentUser.h"
#include "NetworkConfig.h"
#include "NotificationManager.h"
#include "SearchFriendWindow.h"
#include "SearchResultList.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPainter>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

SearchResultList::SearchResultList(QWidget* parent) : CustomScrollArea(parent), searchTimer(new QTimer(this)), networkManager(new QNetworkAccessManager(this)) {
    // 创建用户分组标题
    userSeparator = new QLabel(contentWidget);
    userSeparator->setFixedHeight(SEPARATOR_HEIGHT);
    userSeparator->setStyleSheet("background-color: #E0E0E0;");
    userSeparator->hide();
    userTitle = new QLabel("用户", contentWidget);
    userTitle->setFixedHeight(TITLE_HEIGHT);

    QFont titleFont = userTitle->font();

    titleFont.setPixelSize(13);
    userTitle->setFont(titleFont);
    userTitle->setStyleSheet("color: #666666;");
    userTitle->hide();
    userMore = new QLabel("更多", contentWidget);
    userMore->setFixedHeight(TITLE_HEIGHT);
    userMore->setFont(titleFont);
    userMore->setStyleSheet("color: #0099FF;");
    userMore->setCursor(Qt::PointingHandCursor);
    userMore->hide();

    // 创建群组分组标题
    groupSeparator = new QLabel(contentWidget);
    groupSeparator->setFixedHeight(SEPARATOR_HEIGHT);
    groupSeparator->setStyleSheet("background-color: #E0E0E0;");
    groupSeparator->hide();
    groupTitle = new QLabel("群聊", contentWidget);
    groupTitle->setFixedHeight(TITLE_HEIGHT);
    groupTitle->setFont(titleFont);
    groupTitle->setStyleSheet("color: #666666;");
    groupTitle->hide();
    groupMore = new QLabel("更多", contentWidget);
    groupMore->setFixedHeight(TITLE_HEIGHT);
    groupMore->setFont(titleFont);
    groupMore->setStyleSheet("color: #0099FF;");
    groupMore->setCursor(Qt::PointingHandCursor);
    groupMore->hide();

    // 设置搜索延时
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(500);  // 500ms延时
    connect(searchTimer, &QTimer::timeout, this, &SearchResultList::onSearchTimeout);

    // 连接"更多"点击事件
    connect(userMore, &QLabel::linkActivated, this, &SearchResultList::onMoreUsersClicked);
    connect(groupMore, &QLabel::linkActivated, this, &SearchResultList::onMoreGroupsClicked);
}

void SearchResultList::setSearchText(const QString& text) {
    if (currentSearchText != text) {
        currentSearchText = text;
        searchTimer->start();
    }
}

void SearchResultList::onSearchTimeout() {
    if (currentSearchText.isEmpty()) {
        clearResults();

        return;
    }

    // 清除现有结果
    clearResults();

    // 根据当前搜索类型发送请求
    switch (currentSearchType) {
        case SearchType::All:
            searchUsers(currentSearchText);
            searchGroups(currentSearchText);
            break;
        case SearchType::Users:
            searchUsers(currentSearchText);
            break;
        case SearchType::Groups:
            searchGroups(currentSearchText);
            break;
    }
}

void SearchResultList::setSearchType(SearchType type) {
    if (currentSearchType != type) {
        currentSearchType = type;
        showAllUsers = false;
        showAllGroups = false;

        // 清除现有结果
        clearResults();

        // 如果当前有搜索文本，立即执行搜索
        if (!currentSearchText.isEmpty()) {
            searchTimer->stop();  // 停止可能正在进行的计时
            onSearchTimeout(); // 立即执行搜索
        }
    }
}

void SearchResultList::searchUsers(const QString& keyword) {
    QString baseUrl = NetworkConfig::instance().getHttpAddress();
    QUrl url(baseUrl + "/api/search/users");
    QUrlQuery query;

    query.addQueryItem("keyword", keyword);
    url.setQuery(query);

    QNetworkRequest request(url);

    // 添加token认证
    QString token = CurrentUser::instance().getToken();

    if (token.isEmpty()) {
        NotificationManager::instance().showMessage("未登录，请先登录", NotificationManager::Error, searchWindow);

        return;
    }
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    QNetworkReply* reply = networkManager->get(request);

    reply->setProperty("type", "users");
    connect(reply, &QNetworkReply::finished, this, &SearchResultList::onNetworkReplyFinished);
}

void SearchResultList::searchGroups(const QString& keyword) {
    QString baseUrl = NetworkConfig::instance().getHttpAddress();
    QUrl url(baseUrl + "/api/search/groups");
    QUrlQuery query;

    query.addQueryItem("keyword", keyword);
    url.setQuery(query);

    QNetworkRequest request(url);

    // 添加token认证
    QString token = CurrentUser::instance().getToken();

    if (token.isEmpty()) {
        NotificationManager::instance().showMessage("未登录，请先登录", NotificationManager::Error, searchWindow);

        return;
    }
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    QNetworkReply* reply = networkManager->get(request);

    reply->setProperty("type", "groups");
    connect(reply, &QNetworkReply::finished, this, &SearchResultList::onNetworkReplyFinished);
}

void SearchResultList::onNetworkReplyFinished() {
    QNetworkReply* reply = qobject_cast <QNetworkReply*>(sender());

    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        handleNetworkError(reply);

        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        NotificationManager::instance().showMessage("服务器返回数据格式错误", NotificationManager::Error, searchWindow);

        return;
    }

    QJsonObject response = doc.object();

    if (response.contains("error")) {
        NotificationManager::instance().showMessage(response["error"].toString(), NotificationManager::Error, searchWindow);

        return;
    }

    QString type = reply->property("type").toString();

    if (type == "users") {
        // 处理用户搜索结果
        if (response.contains("users")) {
            processUserResults(response["users"].toArray());
        }
    } else if (type == "groups") {
        // 处理群组搜索结果
        if (response.contains("groups")) {
            processGroupResults(response["groups"].toArray());
        }
    }

    // 根据当前搜索类型更新布局
    updateLayout();
}

void SearchResultList::handleNetworkError(QNetworkReply* reply) {
    QString errorString = reply->errorString();

    NotificationManager::instance().showMessage(QString("网络请求失败: %1").arg(errorString), NotificationManager::Error, searchWindow);
}

void SearchResultList::processUserResults(const QJsonArray& users) {
    int count = qMin(users.size(), showAllUsers ? 20 : 5);

    for (int i = 0; i < count; ++i) {
        QJsonObject userObj = users[i].toObject();
        User user;

        user.id = userObj["id"].toString();
        user.nick = userObj["name"].toString();
        user.avatarPath = userObj["avatar_url"].toString();
        user.signature = userObj["signature"].toString();
        user.status = static_cast <UserStatus>(userObj["status"].toInt());

        QString uid = userObj["uid"].toString();
        auto* item = new SearchResultItem(user, uid, contentWidget);

        userItems.append(item);
        item->show();
    }
}

void SearchResultList::processGroupResults(const QJsonArray& groups) {
    int count = qMin(groups.size(), showAllGroups ? 20 : 5);

    for (int i = 0; i < count; ++i) {
        QJsonObject groupObj = groups[i].toObject();
        QString groupId = QString::number(groupObj["gid"].toInt());
        QString groupName = groupObj["name"].toString();
        QString avatarUrl = groupObj["avatar"].toString();
        int memberCount = groupObj["member_count"].toInt();
        auto* item = new SearchResultItem(groupId, groupName, memberCount, avatarUrl, contentWidget);

        groupItems.append(item);
        item->show();
    }
}

void SearchResultList::clearResults() {
    // 清除用户结果
    qDeleteAll(userItems);
    userItems.clear();

    // 清除群组结果
    qDeleteAll(groupItems);
    groupItems.clear();

    // 隐藏所有标题和分隔线
    userSeparator->hide();
    userTitle->hide();
    userMore->hide();
    groupSeparator->hide();
    groupTitle->hide();
    groupMore->hide();

    // 重新布局
    layoutContent();
}

void SearchResultList::updateLayout() {
    bool hasUsers = !userItems.isEmpty();
    bool hasGroups = !groupItems.isEmpty();

    // 根据当前搜索类型决定显示内容
    switch (currentSearchType) {
        case SearchType::All:
            // 显示所有内容
            userSeparator->setVisible(hasUsers);
            userTitle->setVisible(hasUsers);
            userMore->setVisible(hasUsers && userItems.size() > MAX_ITEMS_SHOW && !showAllUsers);
            groupSeparator->setVisible(hasGroups);
            groupTitle->setVisible(hasGroups);
            groupMore->setVisible(hasGroups && groupItems.size() > MAX_ITEMS_SHOW && !showAllGroups);
            break;
        case SearchType::Users:
            // 只显示用户
            userSeparator->setVisible(hasUsers);
            userTitle->setVisible(hasUsers);
            userMore->setVisible(hasUsers && userItems.size() > MAX_ITEMS_SHOW && !showAllUsers);
            groupSeparator->hide();
            groupTitle->hide();
            groupMore->hide();
            break;
        case SearchType::Groups:
            // 只显示群组
            userSeparator->hide();
            userTitle->hide();
            userMore->hide();
            groupSeparator->setVisible(hasGroups);
            groupTitle->setVisible(hasGroups);
            groupMore->setVisible(hasGroups && groupItems.size() > MAX_ITEMS_SHOW && !showAllGroups);
            break;
    }
    layoutContent();
}

void SearchResultList::layoutContent() {
    int y = 0;
    int w = contentWidget->width();

    // 用户部分
    if (!userItems.isEmpty()) {
        userSeparator->setGeometry(MARGIN, y, w - 2 * MARGIN, SEPARATOR_HEIGHT);
        y += SEPARATOR_HEIGHT;
        userTitle->setGeometry(MARGIN, y, 100, TITLE_HEIGHT);

        if (userMore->isVisible()) {
            userMore->setGeometry(w - MARGIN - 50, y, 50, TITLE_HEIGHT);
        }
        y += TITLE_HEIGHT;

        int count = showAllUsers ? userItems.size() : qMin(userItems.size(), MAX_ITEMS_SHOW);

        for (int i = 0; i < count; ++i) {
            auto* item = userItems[i];

            item->setVisible(true);
            item->setGeometry(0, y, w, item->sizeHint().height());
            y += item->height();
        }

        for (int i = count; i < userItems.size(); ++i) {
            userItems[i]->setVisible(false);
        }
    }

    // 群组部分
    if (!groupItems.isEmpty()) {
        groupSeparator->setGeometry(MARGIN, y, w - 2 * MARGIN, SEPARATOR_HEIGHT);
        y += SEPARATOR_HEIGHT;
        groupTitle->setGeometry(MARGIN, y, 100, TITLE_HEIGHT);

        if (groupMore->isVisible()) {
            groupMore->setGeometry(w - MARGIN - 50, y, 50, TITLE_HEIGHT);
        }
        y += TITLE_HEIGHT;

        int count = showAllGroups ? groupItems.size() : qMin(groupItems.size(), MAX_ITEMS_SHOW);

        for (int i = 0; i < count; ++i) {
            auto* item = groupItems[i];

            item->setVisible(true);
            item->setGeometry(0, y, w, item->sizeHint().height());
            y += item->height();
        }

        for (int i = count; i < groupItems.size(); ++i) {
            groupItems[i]->setVisible(false);
        }
    }
    contentWidget->resize(w, y);
}

void SearchResultList::onMoreUsersClicked() {
    showAllUsers = true;
    showAllGroups = false;
    searchUsers(currentSearchText);
}

void SearchResultList::onMoreGroupsClicked() {
    showAllUsers = false;
    showAllGroups = true;
    searchGroups(currentSearchText);
}