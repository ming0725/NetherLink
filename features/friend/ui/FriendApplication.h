#pragma once
#include <QColor>
#include <QFont>
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/PlusButton.h"
#include "features/friend/ui/FriendListWidget.h"
#include "features/friend/ui/GroupListWidget.h"
#include "app/frame/DefaultPage.h"

class QPushButton;
class FriendSessionController;
class FriendDetailPage;
class GroupDetailPage;
class FriendNotificationPage;
class GroupNotificationPage;

class FriendApplication : public QWidget {
    Q_OBJECT
public:
    explicit FriendApplication(QWidget* parent = nullptr);

signals:
    void requestOpenConversation(const QString& conversationId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr);
        FriendListWidget* friendList() const { return m_content; }
        GroupListWidget* groupList() const { return m_groupList; }
        IconLineEdit* searchInput() const { return m_searchInput; }
        void setFriendModeBadgeCount(int count);
        void setGroupModeBadgeCount(int count);

    protected:
        void resizeEvent(QResizeEvent* ev) override;

    private:
        void updateContentMode(bool animate = true);
        void applyTheme();

        IconLineEdit* m_searchInput;
        PlusButton* m_addButton;
        QWidget* m_modeBar;
        QPushButton* m_friendModeButton;
        QPushButton* m_groupModeButton;
        FriendListWidget* m_content;
        GroupListWidget* m_groupList;
    };

    FriendSessionController* m_friendController = nullptr;
    LeftPane*    m_leftPane = nullptr;     // 左侧面板
    DefaultPage* m_defaultPage = nullptr;  // 右侧默认页
    FriendDetailPage* m_detailPage = nullptr;
    GroupDetailPage* m_groupDetailPage = nullptr;
    FriendNotificationPage* m_notificationPage = nullptr;
    GroupNotificationPage* m_groupNotificationPage = nullptr;
    QStackedWidget* m_rightStack = nullptr;
    QSplitter*   m_splitter = nullptr;     // 中间分隔器

    void showNotificationPage();
    void hideNotificationPage();
    void populateNotificationData();
    void showGroupNotificationPage();
    void hideGroupNotificationPage();
    void populateGroupNotificationData();
    FriendDetailPage* ensureFriendDetailPage();
    GroupDetailPage* ensureGroupDetailPage();
    FriendNotificationPage* ensureFriendNotificationPage();
    GroupNotificationPage* ensureGroupNotificationPage();
};
