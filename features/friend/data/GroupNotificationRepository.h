#pragma once

#include <QObject>
#include <QVector>

#include "shared/types/GroupNotification.h"

struct GroupNotificationListRequest {
    int offset = 0;
    int limit = 20;
};

class GroupNotificationRepository : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(GroupNotificationRepository)

public:
    static GroupNotificationRepository& instance();

    QVector<GroupNotification> requestNotificationList(const GroupNotificationListRequest& query = {}) const;
    int unreadCount() const;
    int notificationCount() const;
    void markAllRead();
    bool acceptJoinRequest(const QString& notificationId,
                           const QString& remark = QString(),
                           const QString& categoryId = QStringLiteral("gg_joined"),
                           const QString& categoryName = QStringLiteral("我加入的群聊"));
    bool rejectJoinRequest(const QString& notificationId);

signals:
    void notificationListChanged();

private:
    explicit GroupNotificationRepository(QObject* parent = nullptr);
    ~GroupNotificationRepository() override = default;

    void ensureLoaded() const;

    mutable bool m_loaded = false;
    QVector<GroupNotification> m_notifications;
};
