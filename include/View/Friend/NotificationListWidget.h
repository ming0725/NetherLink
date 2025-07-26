/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_WIDGET
#define INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Components/CustomScrollArea.h"
#include "View/Friend/NotificationListItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NotificationListWidget : public CustomScrollArea {
    Q_OBJECT

    public:
        explicit NotificationListWidget(QWidget* parent = nullptr);

        void addNotification(int requestId, const QString& userId, const QString& name, const QString& avatarUrl, const QString& message, const QString& date, NotificationListItem::Type type);

    protected:
        void layoutContent() override;

        using CustomScrollArea::contentWidget;

    private:
        QVector <NotificationListItem*> m_items;
};

#endif /* INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_WIDGET */
