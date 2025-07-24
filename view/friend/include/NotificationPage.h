
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QStackedWidget>

#include "NotificationListWidget.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NotificationPage : public QWidget {
    Q_OBJECT

    public:
        enum class Type {
            Friend,
            Group
        };

        explicit NotificationPage(QWidget* parent = nullptr);

        void switchToType(Type type);

    protected:
        void paintEvent(QPaintEvent*) override;

        void resizeEvent(QResizeEvent* event) override;

    private slots:
        void onFriendRequestReceived(int requestId, const QString& fromUid, const QString& fromName, const QString& fromAvatar, const QString& message, const QString& createdAt);

    private:
        QLabel* titleLabel;
        NotificationListWidget* friendListWidget;
        NotificationListWidget* groupListWidget;
        QStackedWidget* contentStack;

        void setupUI();

};
