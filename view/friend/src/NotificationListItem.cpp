#include "NotificationListItem.h"
#include "TransparentMenu.h"
#include "NetworkManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

NotificationListItem::NotificationListItem(int requestId,
                                         const QString& userId,
                                         const QString& name,
                                         const QString& avatarUrl,
                                         const QString& message,
                                         const QString& date,
                                         Type type,
                                         QWidget* parent)
    : QWidget(parent)
    , requestId(requestId)
    , avatarLabel(new AvatarLabel(this))
    , nameLabel(new QLabel(name, this))
    , requestLabel(new QLabel(this))
    , dateLabel(new QLabel(date, this))
    , messageLabel(new QLabel(this))
    , actionButton(new QPushButton(this))
    , currentStatus(Status::Pending)
    , itemType(type)
{
    setFixedHeight(72);
    setupUI();
    createActionMenu();

    // 设置头像
    avatarLabel->setAvatarSize(48);
    avatarLabel->loadAvatar(userId, QUrl(avatarUrl));

    // 设置请求类型文本
    requestLabel->setText(type == Type::Friend ? "请求添加为好友" : "请求加入群聊");

    // 设置留言（如果有）
    if (!message.isEmpty()) {
        messageLabel->setText("留言：" + message);
        messageLabel->show();
    } else {
        messageLabel->hide();
    }
}

void NotificationListItem::setupUI()
{
    // 设置名字标签样式
    QFont nameFont = nameLabel->font();
    nameFont.setPixelSize(14);
    nameLabel->setFont(nameFont);
    nameLabel->setStyleSheet("color: #0066cc;");

    // 设置请求标签样式
    QFont requestFont = requestLabel->font();
    requestFont.setPixelSize(12);
    requestLabel->setFont(requestFont);

    // 设置日期标签样式
    QFont dateFont = dateLabel->font();
    dateFont.setPixelSize(12);
    dateLabel->setFont(dateFont);
    dateLabel->setStyleSheet("color: #666666;");

    // 设置留言标签样式
    QFont messageFont = messageLabel->font();
    messageFont.setPixelSize(12);
    messageLabel->setFont(messageFont);
    messageLabel->setStyleSheet("color: #333333;");

    // 设置操作按钮样式
    actionButton->setText("▼");
    actionButton->setFixedSize(60, 24);
    actionButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #ffffff;"
        "   border: 1px solid #cccccc;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #f0f0f0;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #e0e0e0;"
        "}"
    );
}

void NotificationListItem::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // 布局组件
    const int padding = 16;
    avatarLabel->move(padding, (height() - avatarLabel->height()) / 2);

    const int contentX = padding + avatarLabel->width() + 12;
    const int contentWidth = width() - contentX - padding - actionButton->width() - padding;

    // 计算第一行各元素宽度
    QFontMetrics nameFm(nameLabel->font());
    QFontMetrics requestFm(requestLabel->font());
    QFontMetrics dateFm(dateLabel->font());

    QString nameText = nameLabel->text();
    QString requestText = requestLabel->text();
    QString dateText = dateLabel->text();

    // 名字最多占30%
    int maxNameWidth = contentWidth * 0.3;
    nameText = nameFm.elidedText(nameText, Qt::ElideRight, maxNameWidth);
    nameLabel->setText(nameText);
    int actualNameWidth = nameFm.horizontalAdvance(nameText);

    // 请求文本固定宽度
    int requestWidth = requestFm.horizontalAdvance(requestText);

    // 日期占剩余空间
    int dateWidth = dateFm.horizontalAdvance(dateText);

    // 计算两行的垂直位置
    const int totalHeight = height();
    const int lineHeight = nameFm.height();
    const int totalContentHeight = lineHeight * 2 + 6; // 统一按两行高度计算（6是两行之间的间距）
    const int startY = (totalHeight - totalContentHeight) / 2;

    // 布局第一行
    nameLabel->setGeometry(contentX, startY, actualNameWidth, lineHeight);
    requestLabel->setGeometry(contentX + actualNameWidth + 8, startY, requestWidth, lineHeight);
    dateLabel->setGeometry(contentX + actualNameWidth + requestWidth + 16, startY, dateWidth, lineHeight);
    
    // 布局第二行
    messageLabel->setGeometry(contentX, startY + lineHeight + 6, contentWidth, lineHeight);
    // 如果没有留言，messageLabel会被隐藏，但位置仍然保留，保证视觉一致性

    // 布局操作按钮
    actionButton->move(width() - padding - actionButton->width(),
                      (height() - actionButton->height()) / 2);
}

void NotificationListItem::sendRequestResponse(bool accept)
{
    QJsonObject payload;
    payload["request_id"] = requestId;
    payload["action"] = accept ? "accept" : "reject";

    QJsonObject message;
    message["type"] = "friend_request_handle";
    message["payload"] = payload;

    QString jsonString = QJsonDocument(message).toJson(QJsonDocument::Compact);
    NetworkManager::instance().sendWsMessage(jsonString);
}

void NotificationListItem::createActionMenu()
{
    connect(actionButton, &QPushButton::clicked, [this]() {
        // 创建临时菜单
        auto* menu = new TransparentMenu(this);
        QIcon icon(":/resources/icon/friend_selected.png");
        
        menu->addAction(icon, "同意", [this]() {
            setStatus(Status::Accepted);
            sendRequestResponse(true);
            // 延迟1秒后再获取联系人列表，确保服务器有时间处理请求
            QTimer::singleShot(2000, this, []() {
                NetworkManager::instance().reloadContacts();
            });
            emit accepted();
        });
        menu->addAction(icon, "拒绝", [this]() {
            setStatus(Status::Rejected);
            sendRequestResponse(false);
            emit rejected();
        });

        // 显示菜单
        QPoint pos = actionButton->mapToGlobal(QPoint(0, actionButton->height() + 2));
        menu->popup(pos);

        // 菜单关闭后自动删除
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    });
}

void NotificationListItem::setStatus(Status status)
{
    currentStatus = status;
    updateStatusDisplay();
}

void NotificationListItem::updateStatusDisplay()
{
    actionButton->setVisible(currentStatus == Status::Pending);
    
    if (currentStatus != Status::Pending) {
        QLabel* statusLabel = new QLabel(this);
        statusLabel->setStyleSheet("color: #999999;");
        statusLabel->setText(currentStatus == Status::Accepted ? "已同意" : "已拒绝");
        
        // 计算状态标签的大小
        statusLabel->adjustSize();
        
        // 计算居中位置
        int x = actionButton->pos().x() + (actionButton->width() - statusLabel->width()) / 2;
        int y = actionButton->pos().y() + (actionButton->height() - statusLabel->height()) / 2;
        
        statusLabel->move(x, y);
        statusLabel->show();
        actionButton->hide();
    }
}

void NotificationListItem::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制白色背景和圆角
    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    painter.fillPath(path, Qt::white);
}

void NotificationListItem::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    update();
}

void NotificationListItem::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    update();
}

void NotificationListItem::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    update();
}

void NotificationListItem::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    update();
} 