#pragma once
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <functional>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/PlusButton.h"
#include "features/chat/ui/MessageListWidget.h"
#include "app/frame/DefaultPage.h"
#include "features/chat/ui/ChatArea.h"

class MessageApplication : public QWidget {
    Q_OBJECT
public:
    explicit MessageApplication(QWidget* parent = nullptr);
    void handleGlobalMousePress(const QPoint& globalPos);
    void setSystemFloatingBarsSuppressed(bool suppressed);
    void setAppBarActive(bool active);

public slots:
    void openConversationFromContact(const QString& conversationId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private slots:
    void onMessageClicked(const QString& conversationId);
    void onCurrentConversationDeleted();
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr);
        MessageListWidget* messageList() const { return m_msgList; }
        IconLineEdit* searchInput() const { return m_searchInput; }
        void setCreatedGroupCallback(std::function<void(const QString&)> callback);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void applyTheme();

        IconLineEdit* m_searchInput;
        PlusButton* m_addButton;
        MessageListWidget* m_msgList;
        std::function<void(const QString&)> m_createdGroupCallback;
    };

    void ensureChatArea();
    void applyLoadedConversation(int token, const ConversationThreadData& conversation);
    void applyVisibleConversationState(bool visible);
    QString activeConversationId() const;
    LeftPane*            m_leftPane;
    QSplitter*          m_splitter;
    QStackedWidget*     m_rightStack;
    DefaultPage*        m_defaultPage;
    ChatArea*           m_chatArea = nullptr;
    bool m_systemFloatingBarsSuppressed = false;
    bool m_appBarActive = true;
    int m_openConversationLoadToken = 0;
    bool m_openConversationLoadPending = false;
    QString m_openConversationRequestId;
    QString m_openDirectConversationRequestId;
    QString m_openGroupConversationRequestId;
};
