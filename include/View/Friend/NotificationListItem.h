/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_ITEM

#define INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_ITEM

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPushButton>

#include "Data/AvatarLoader.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NotificationListItem : public QWidget {
    Q_OBJECT

    public:
        enum class Type {
            Friend,
            Group
        };

        enum class Status {
            Pending,
            Accepted,
            Rejected
        };

        explicit NotificationListItem(int requestId, const QString& userId, const QString& name, const QString& avatarUrl, const QString& message, const QString& date, Type type, QWidget* parent = nullptr);

        void setStatus(Status status);

    protected:
        void paintEvent(QPaintEvent*) override;

        void enterEvent(QEnterEvent*) override;

        void leaveEvent(QEvent*) override;

        void mousePressEvent(QMouseEvent*) override;

        void mouseReleaseEvent(QMouseEvent*) override;

        void resizeEvent(QResizeEvent* event) override;

    private:
        int requestId;
        AvatarLabel* avatarLabel;
        QLabel* nameLabel;
        QLabel* requestLabel;
        QLabel* dateLabel;
        QLabel* messageLabel;
        QPushButton* actionButton;
        Status currentStatus;
        Type itemType;

        void setupUI();

        void createActionMenu();

        void updateStatusDisplay();

        void sendRequestResponse(bool accept);

    signals:
        void accepted();

        void rejected();
};

#endif /* INCLUDE_VIEW_FRIEND_NOTIFICATION_LIST_ITEM */
