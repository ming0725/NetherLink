/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>

#include "Network/NetworkManager.h"
#include "View/Friend/NotificationPage.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

NotificationPage::NotificationPage(QWidget* parent) : QWidget(parent), titleLabel(new QLabel(this)), friendListWidget(new NotificationListWidget(this)), groupListWidget(new NotificationListWidget(this)), contentStack(new QStackedWidget(this)) {
    setupUI();

    // 连接网络管理器的好友请求信号
    connect(&NetworkManager::instance(), &NetworkManager::friendRequestReceived, this, &NotificationPage::onFriendRequestReceived);
}

void NotificationPage::setupUI() {
    // 设置标题样式
    QFont titleFont = titleLabel->font();

    titleFont.setPixelSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    // 设置堆叠窗口
    contentStack->addWidget(friendListWidget);
    contentStack->addWidget(groupListWidget);

    // 默认显示好友通知
    switchToType(Type::Friend);
}

void NotificationPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // 布局
    titleLabel->setGeometry(24, 24, width() - 48, 30);
    contentStack->setGeometry(0, 62, width(), height() - 62);
}

void NotificationPage::switchToType(Type type) {
    if (type == Type::Friend) {
        titleLabel->setText("好友通知");
        contentStack->setCurrentWidget(friendListWidget);
    } else {
        titleLabel->setText("群聊通知");
        contentStack->setCurrentWidget(groupListWidget);
    }
}

void NotificationPage::onFriendRequestReceived(int requestId, const QString& fromUid, const QString& fromName, const QString& fromAvatar, const QString& message, const QString& createdAt) {
    // 添加新的好友请求通知到列表
    friendListWidget->addNotification(requestId, fromUid, fromName, fromAvatar, message, createdAt, NotificationListItem::Type::Friend);
}

void NotificationPage::paintEvent(QPaintEvent*) {
    QPainter p(this);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xf2f2f2));
    p.drawRect(rect());
}
