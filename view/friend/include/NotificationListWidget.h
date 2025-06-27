#pragma once

#include <QWidget>
#include <QVector>
#include "NotificationListItem.h"
#include "CustomScrollArea.h"

class NotificationListWidget : public CustomScrollArea {
    Q_OBJECT
public:
    explicit NotificationListWidget(QWidget* parent = nullptr);
    void addNotification(int requestId,
                        const QString& userId,
                        const QString& name,
                        const QString& avatarUrl,
                        const QString& message,
                        const QString& date,
                        NotificationListItem::Type type);

protected:
    void layoutContent() override;
    using CustomScrollArea::contentWidget;

private:
    QVector<NotificationListItem*> m_items;
}; 