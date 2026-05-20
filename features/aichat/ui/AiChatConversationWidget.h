#pragma once

#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class AiChatMessageListModel;
class AiChatMessageListView;
class AiChatFloatingInputBar;
class AiChatSessionController;
class NewMessageNotifier;
class PaintedLabel;

class AiChatConversationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatConversationWidget(QWidget* parent = nullptr);
    void setController(AiChatSessionController* controller);

public slots:
    void openConversation(const AiChatListEntry& entry);
    void closeConversation();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSendText(const QString& text);
    void onStopStreamingRequested();
    void onRegenerateAiReplyRequested(const QString& conversationId, const QString& messageId);
    void onAiReplyStarted(const QString& conversationId);
    void onAiReplyMessageAdded(const AiChatMessage& message);
    void onAiReplyMessageUpdated(const QString& conversationId,
                                 const QString& messageId,
                                 const QString& text);
    void onAiReplyMessageRemoved(const QString& conversationId, const QString& messageId);
    void onAiReplyFinished(const QString& conversationId, const QString& messageId);
    void onAiReplyCanceled(const QString& conversationId, const QString& messageId);
    void onConversationMessagesLoaded(int requestId,
                                      const QString& conversationId,
                                      const QVector<AiChatMessage>& messages);

private:
    void updateLayout();
    void updateBottomSpace();
    void updateHeader();
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    bool shouldShowNewMessageNotifier() const;
    bool isMessageViewAtBottom() const;
    void cancelActiveAiReplyStream();
    bool hasActiveAiReplyStream() const;

    AiChatSessionController* m_controller = nullptr;
    AiChatMessageListView* m_messageView = nullptr;
    AiChatMessageListModel* m_messageModel = nullptr;
    AiChatFloatingInputBar* m_inputBar = nullptr;
    PaintedLabel* m_titleLabel = nullptr;
    QWidget* m_headerDivider = nullptr;
    QWidget* m_bottomGapGradientOverlay = nullptr;
    NewMessageNotifier* m_newMessageNotifier = nullptr;
    PaintedLabel* m_emptyLabel = nullptr;
    AiChatListEntry m_currentConversation;
    int m_pendingMessagesRequestId = 0;
    QString m_pendingMessagesConversationId;
    int m_unreadAiReplyCount = 0;
    bool m_newMessageNotifierRevealedByDownScroll = false;
    bool m_streamingNotifierHeld = false;

    static constexpr int kHeaderHeight = 62;
    static constexpr int kHeaderTitleHeight = 28;
    static constexpr int kHeaderTitleLeft = 20;
    static constexpr int kHeaderTitleRight = 18;
    static constexpr int kHeaderTitleBottomMargin = 10;
    static constexpr int kInputBarHeight = 195;
    static constexpr int kInputBarSideMargin = 20;
    static constexpr int kInputBarBottomMargin = 18;
    static constexpr int kListBottomPadding = 10;
    static constexpr int kNewMessageNotifierInputGap = 10;
    static constexpr int kNewMessageNotifierMinBottomDistance = 220;
    static constexpr int kNewMessageNotifierViewportDistanceDivisor = 2;
};
