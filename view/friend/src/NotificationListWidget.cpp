#include "NotificationListWidget.h"
#include <QPainter>

NotificationListWidget::NotificationListWidget(QWidget* parent) : CustomScrollArea(parent) {
    // 设置背景色
    setStyleSheet("border-width:0px;border-style:solid;");
    contentWidget->setObjectName("NotificationListContent");
    contentWidget->setStyleSheet(" #NotificationListContent {""    background-color: #F2F2F2;""}");
}

void NotificationListWidget::addNotification(int requestId, const QString& userId, const QString& name, const QString& avatarUrl, const QString& message, const QString& date, NotificationListItem::Type type) {
    auto* item = new NotificationListItem(requestId, userId, name, avatarUrl, message, date, type, contentWidget);

    m_items.append(item);
    layoutContent();
}

void NotificationListWidget::layoutContent() {
    if (m_items.isEmpty()) {
        contentWidget->resize(width(), height());

        return;
    }

    const int padding = 24;  // 左右边距
    const int spacing = 18; // 项目间距
    const int itemWidth = width() - padding * 2;
    int y = 8;

    for (auto* item : m_items) {
        item->setFixedWidth(itemWidth);
        item->move(padding, y);
        y += item->height() + spacing;
    }

    // 设置内容区域的大小（加上底部padding）
    int h = height();
    int totalH = y + padding - spacing;

    contentWidget->resize(width(), totalH < h ? h : totalH);
}