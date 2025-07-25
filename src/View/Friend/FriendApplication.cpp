/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Network/NetworkManager.h"
#include "View/Friend/FriendApplication.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// FriendLeftPane implementation
FriendLeftPane::FriendLeftPane(QWidget* parent) : QWidget(parent), m_topSearch(new TopSearchWidget(this)), m_friendNotification(new NotificationItem("好友通知", this)), m_groupNotification(new NotificationItem("群通知", this)), m_content(new FriendListWidget(this)) {
    setMinimumWidth(144);
    setMaximumWidth(305);
    m_content->setStyleSheet("border-width:0px;border-style:solid;");

    // 初始化状态为未选中
    m_friendNotification->setSelected(false);
    m_groupNotification->setSelected(false);
    connect(m_friendNotification, &NotificationItem::clicked, this, &FriendLeftPane::friendNotificationClicked);
    connect(m_groupNotification, &NotificationItem::clicked, this, &FriendLeftPane::groupNotificationClicked);

    // 连接好友请求信号
    connect(&NetworkManager::instance(), &NetworkManager::friendRequestReceived, this, [this] (int, const QString&, const QString&, const QString&, const QString&, const QString&) {
        // 如果当前不在好友通知页面，增加未读计数
        if (!m_friendNotification->isSelected()) {
            int currentCount = m_friendNotification->getUnreadCount();
            m_friendNotification->setUnreadCount(currentCount + 1);
        }
    });
}

void FriendLeftPane::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);

    int lw = width();
    int lh = height();
    int topH = m_topSearch->height();
    int notificationH = m_friendNotification->height();

    m_topSearch->setGeometry(0, 0, lw, topH);
    m_friendNotification->setGeometry(0, topH, lw, notificationH);
    m_groupNotification->setGeometry(0, topH + notificationH, lw, notificationH);
    m_content->setGeometry(0, topH + notificationH * 2, lw, lh - topH - notificationH * 2);
}

// FriendApplication implementation
FriendApplication::FriendApplication(QWidget* parent) : QWidget(parent) {
    // 1) 左侧面板：内部已创建 TopSearchWidget + FriendListWidget
    m_leftPane = new FriendLeftPane(this);

    // 2) 创建堆叠窗口和页面
    m_stackedWidget = new QStackedWidget(this);
    m_defaultPage = new DefaultPage(this);
    m_notificationPage = new NotificationPage(this);
    m_stackedWidget->addWidget(m_defaultPage);
    m_stackedWidget->addWidget(m_notificationPage);

    // 3) 分割器：将左、右面板加入
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_stackedWidget);
    m_splitter->setHandleWidth(0);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    // 设置初始左侧宽度
    m_splitter->setSizes({200, width() - 200});
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    // 连接通知点击信号
    connect(m_leftPane, &FriendLeftPane::friendNotificationClicked, this, &FriendApplication::onFriendNotificationClicked);
    connect(m_leftPane, &FriendLeftPane::groupNotificationClicked, this, &FriendApplication::onGroupNotificationClicked);

    // 去除窗口自带标题栏，若需要无边框效果
    setWindowFlag(Qt::FramelessWindowHint);
}

void FriendApplication::resizeEvent(QResizeEvent* /*event*/) {
    m_splitter->setGeometry(0, 0, width(), height());
}

void FriendApplication::paintEvent(QPaintEvent*) {
    QPainter p(this);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawRect(rect());
}

void FriendApplication::onFriendNotificationClicked() {
    m_stackedWidget->setCurrentWidget(m_notificationPage);
    m_notificationPage->switchToType(NotificationPage::Type::Friend);
    m_leftPane->m_friendNotification->setSelected(true);
    m_leftPane->m_groupNotification->setSelected(false);

    // 清除未读消息计数
    m_leftPane->m_friendNotification->clearUnreadCount();
}

void FriendApplication::onGroupNotificationClicked() {
    m_stackedWidget->setCurrentWidget(m_notificationPage);
    m_notificationPage->switchToType(NotificationPage::Type::Group);
    m_leftPane->m_friendNotification->setSelected(false);
    m_leftPane->m_groupNotification->setSelected(true);

    // 清除未读消息计数
    m_leftPane->m_groupNotification->clearUnreadCount();
}
