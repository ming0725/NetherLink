/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_FRIEND_APPLICATION

#define INCLUDE_VIEW_FRIEND_FRIEND_APPLICATION

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QSplitter>

#include "Components/TopSearchWidget.h"
#include "View/Friend/FriendListWidget.h"
#include "View/Friend/NotificationItem.h"
#include "View/Friend/NotificationPage.h"
#include "View/Mainwindow/DefaultPage.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

// 前置声明
class FriendApplication;

// 左侧面板类
class FriendLeftPane : public QWidget {
    Q_OBJECT

    public:
        explicit FriendLeftPane(QWidget* parent = nullptr);

    protected:
        void resizeEvent(QResizeEvent* ev) override;

    signals:
        void friendNotificationClicked();

        void groupNotificationClicked();

    private:
        TopSearchWidget* m_topSearch;
        NotificationItem* m_friendNotification;
        NotificationItem* m_groupNotification;
        FriendListWidget* m_content;

        friend class FriendApplication;
};

class FriendApplication : public QWidget {
    Q_OBJECT

    public:
        explicit FriendApplication(QWidget* parent = nullptr);

    protected:
        void resizeEvent(QResizeEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

    private slots:
        void onFriendNotificationClicked();

        void onGroupNotificationClicked();

    private:
        FriendLeftPane* m_leftPane; // 左侧面板
        QStackedWidget* m_stackedWidget; // 右侧堆叠窗口
        DefaultPage* m_defaultPage; // 默认页面
        NotificationPage* m_notificationPage; // 通知页面
        QSplitter* m_splitter; // 中间分隔器
};

#endif /* INCLUDE_VIEW_FRIEND_FRIEND_APPLICATION */
