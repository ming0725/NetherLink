
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef AICHATWINDOW_H
#define AICHATWINDOW_H

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QBoxLayout>

#include "AiChatListModel.h"

#include "AiChatListView.h"
#include "AiChatWebSocket.h"
#include "FloatingInputBar.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatWindow : public QWidget {
    Q_OBJECT

    public:
        explicit AiChatWindow(QWidget* parent = nullptr);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private slots:
        void onSendMessage(const QString& content);

        void onMessageContent(const QString& content);

        void onConversationStarted(const QString& conversationId);

        void onMessageEnded();

        void onConnectionEstablished();

        void onConnectionError(const QString& error);

        void onRequestError(const QString& errorMessage);

    private:
        void setupUI();

        void updateInputBarPosition();

        QVBoxLayout* m_mainLayout;
        AiChatListView* m_chatView;
        FloatingInputBar* m_inputBar;
        AiChatWebSocket* m_webSocket;
        AiChatListModel* m_model;
        QString m_currentConversationId;
        static constexpr int INPUT_BAR_MARGIN = 20; // 输入栏与边缘的距离
};

#endif // AICHATWINDOW_H
