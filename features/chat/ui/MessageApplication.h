#pragma once
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/StatefulPushButton.h"
#include "features/chat/ui/MessageListWidget.h"
#include "app/frame/DefaultPage.h"
#include "features/chat/ui/ChatArea.h"

class MessageApplication : public QWidget {
    Q_OBJECT
public:
    explicit MessageApplication(QWidget* parent = nullptr);
    void handleGlobalMousePress(const QPoint& globalPos);
    void setSystemFloatingBarsSuppressed(bool suppressed);

public slots:
    void openConversationFromContact(const QString& conversationId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private slots:
    void onMessageClicked(const QString& conversationId);
    void onCurrentConversationDeleted();
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr);
        MessageListWidget* messageList() const { return m_msgList; }
        IconLineEdit* searchInput() const { return m_searchInput; }

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void applyTheme();

        IconLineEdit* m_searchInput;
        StatefulPushButton* m_addButton;
        MessageListWidget* m_msgList;
    };

    void ensureChatArea();
    void applyLoadedConversation(int token, const ConversationThreadData& conversation);
    LeftPane*            m_leftPane;
    QSplitter*          m_splitter;
    QStackedWidget*     m_rightStack;
    DefaultPage*        m_defaultPage;
    ChatArea*           m_chatArea = nullptr;
    bool m_systemFloatingBarsSuppressed = false;
    int m_openConversationLoadToken = 0;
    bool m_openConversationLoadPending = false;
    QString m_openConversationRequestId;
};
