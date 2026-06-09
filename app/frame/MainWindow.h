#pragma once

#include "DefaultPage.h"
#include "ApplicationBar.h"
#include "platform/SystemWindow.h"
#include "shared/network/RealtimeClient.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QRect>
#include <QStackedWidget>

class QAbstractButton;
class MessageApplication;
class FriendApplication;
class PostApplication;
class AiChatApplication;
class SettingsWindow;

class MainWindow : public SystemWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
signals:
    void logoutRequested();
protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;
    void showEvent(QShowEvent* event) override;
private slots:
    void onBarItemClicked(ApplicationBarItem* item);
private:
    QWidget* createPlaceholderPage() const;
    void replaceStackPage(int index, QWidget* widget);
    void ensureApplicationLoaded(int index);
    void connectFriendConversationRequests();
    void openConversationFromContacts(const QString& conversationId);
    void updateBackdropTheme();
    void openSettingsWindow();
    void openAppearanceSettingsWindow();
    void showSettingsWindow(bool openAppearancePage);
    void closeSettingsWindow();
    void setSystemFloatingBarsSuppressed(bool suppressed);
    void layoutWindow();
    bool restoreWindowPlacement();
    void saveWindowPlacement();
    void showRealtimeFailureNotice(const QString& message);
    void handleRealtimeStateChanged(RealtimeClient::State state);
    void handleSessionRevoked(const QString& accountId, const QString& message);

    ApplicationBar *appBar;
    QWidget *titleBar;
    QAbstractButton *btnMinimize;
    QAbstractButton *btnMaximize;
    QAbstractButton *btnClose;
    QStackedWidget* stack;
    MessageApplication* m_messageApp = nullptr;
    FriendApplication* m_friendApp = nullptr;
    PostApplication* m_postApp = nullptr;
    AiChatApplication* m_aiChatApp = nullptr;
    DefaultPage* m_defaultPage = nullptr;
    QPointer<SettingsWindow> m_settingsWindow;
    QPointer<QWidget> m_pendingFocusClear;
    QElapsedTimer m_realtimeNoticeClock;
    QString m_windowPlacementAccountKey;
    QRect m_restoredNormalGeometry;
    bool m_realtimeHadFailure = false;
    bool m_systemFloatingBarsSuppressed = false;
    bool m_sessionRevokedDialogVisible = false;
    bool m_restoredNormalGeometryApplied = false;
    bool m_restoreMaximized = false;
    bool m_restoreMaximizedApplied = false;
};
