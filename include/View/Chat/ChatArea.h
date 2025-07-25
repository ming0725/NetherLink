/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_CHAT_CHAT_AREA
#define INCLUDE_VIEW_CHAT_CHAT_AREA

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Components/FloatingInputBar.h"
#include "View/Chat/ChatItemDelegate.h"
#include "View/Chat/ChatListModel.h"
#include "View/Chat/ChatListView.h"
#include "View/Chat/NewMessageNotifier.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ChatArea : public QWidget {
    Q_OBJECT using ChatMessagePtr = QSharedPointer <ChatMessage>;

    public:
        explicit ChatArea(QWidget*parent = nullptr);

        void initMessage(QVector <ChatMessagePtr>&);

        void addMessage(ChatMessagePtr message);

        void addImageMessage(QSharedPointer <ImageMessage> message, const QDateTime& timestamp = QDateTime::currentDateTime());

        void addTextMessage(QSharedPointer <TextMessage> message, const QDateTime& timestamp = QDateTime::currentDateTime());

        void setGroupMode(bool mode);

        void setMessageId(QString id);

        void clearAll();

    protected:
        void resizeEvent(QResizeEvent*event) override;

    private slots:
        void onScrollValueChanged(int value);

        void onNewMessageNotifierClicked();

        void onSendImage(const QString &path);

        void onSendText(const QString &text);

        void onMessageReceived(ChatMessagePtr message);

    private:
        ChatListView* chatView;
        ChatListModel* chatModel;
        ChatItemDelegate* chatDelegate;
        NewMessageNotifier* newMessageNotifier;
        FloatingInputBar* inputBar;
        QLabel* statusIcon;
        QLabel* nameLabel;
        int unreadMessageCount;
        bool isAtBottom;
        bool isGroupMode;
        QString messageId;

        void updateNewMessageNotifier();

        void updateNewMessageNotifierPosition();

        void scrollToBottom();

        bool isScrollAtBottom() const;

        bool isNearBottom() const;

        void adjustBottomSpace();

        void updateInputBarPosition();

};

#endif /* INCLUDE_VIEW_CHAT_CHAT_AREA */
