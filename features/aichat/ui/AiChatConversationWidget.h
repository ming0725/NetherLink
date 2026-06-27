#pragma once

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class AiChatMessageListModel;
class AiChatMessageListView;
class AiChatFloatingInputBar;
class AiChatSessionController;
class NewMessageNotifier;
class PaintedLabel;
class QTimer;
class QVariantAnimation;

class AiChatConversationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatConversationWidget(QWidget* parent = nullptr);
    void setController(AiChatSessionController* controller);

public slots:
    void openConversation(const AiChatListEntry& entry);
    void showStartPage();
    void closeConversation();

signals:
    void conversationCreatedFromStartPage(const QString& conversationId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSendText(const QString& text, const AiChatRequestOptions& options);
    void onStopStreamingRequested();
    void onRegenerateAiReplyRequested(const QString& conversationId, const QString& messageId);
    void onAiReplyStarted(const QString& conversationId);
    void onAiReplyMessageAdded(const AiChatMessage& message, bool isProgress);
    void onAiReplyMessageUpdated(const QString& conversationId,
                                 const QString& messageId,
                                 const QString& text,
                                 bool isProgress);
    void onAiReplyMessageReplaced(const QString& conversationId,
                                  const QString& messageId,
                                  const AiChatMessage& replacement);
    void onAiReplyMessageRemoved(const QString& conversationId, const QString& messageId);
    void onAiReplyProgressChanged(const QString& conversationId,
                                  const AiChatProgressSegment& progress);
    void onAiReplyThinkingChanged(const QString& conversationId, bool active);
    void onAiReplyStreamStatusChanged(const QString& conversationId,
                                      const AiChatStreamStatus& status);
    void onAiReplyFinished(const QString& conversationId, const QString& messageId);
    void onAiReplyCanceled(const QString& conversationId, const QString& messageId);
    void onAiReplyFailed(const QString& conversationId,
                         const QString& messageId,
                         const QString& message);
    void onConversationMessagesLoaded(int requestId,
                                      const QString& conversationId,
                                      const QVector<AiChatMessage>& messages);
    void onContextUsageLoaded(int requestId,
                              const AiChatContextUsageRequest& request,
                              const AiChatContextUsage& usage);
    void onTraceToggleRequested(const QString& messageId);

private:
    struct StreamWorkStep {
        QString stepId;
        int sequence = 0;
        int encounterOrder = 0;
        QStringList progressSegmentOrder;
        QHash<QString, AiChatProgressSegment> progressSegments;
        AiChatStreamStatus toolStatus;
        bool hasToolStatus = false;
    };

    void updateLayout();
    void updateBottomSpace();
    void updateHeader();
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    void requestContextUsage();
    void saveStartPageDraft();
    void scheduleThinkingPlaceholder(const QString& conversationId);
    void cancelScheduledThinkingPlaceholder();
    void showScheduledThinkingPlaceholder(const QString& conversationId);
    void showThinkingPlaceholder(const QString& conversationId, const QString& text);
    void showOrUpdateThinkingPlaceholder(const QString& conversationId,
                                         const QString& text,
                                         bool retain,
                                         bool active);
    void hideThinkingPlaceholder(const QString& conversationId);
    void finishThinkingPlaceholder(const QString& conversationId);
    bool hasThinkingPlaceholder() const;
    void presentStreamAnswer(const AiChatMessage& message, const QString& text);
    void rebuildStreamWorkRows();
    void clearStreamWorkRows();
    void installTraceRows(const AiChatMessage& message, qreal expansionProgress = 1.0);
    QVector<AiChatMessage> traceRows(const AiChatMessage& message) const;
    void stopTraceExpansionAnimations();
    void resetStreamPresentation();
    bool shouldShowNewMessageNotifier() const;
    bool isMessageViewAtBottom() const;
    bool isStartPage() const;
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
    int m_pendingContextUsageRequestId = 0;
    QString m_pendingContextUsageConversationId;
    int m_unreadAiReplyCount = 0;
    QString m_startPageDraft;
    bool m_newMessageNotifierRevealedByDownScroll = false;
    bool m_streamingNotifierHeld = false;
    QString m_thinkingMessageId;
    QTimer* m_thinkingAnimationTimer = nullptr;
    QString m_pendingThinkingConversationId;
    bool m_streamHasAnswerText = false;
    bool m_retainThinkingPlaceholder = false;
    QString m_streamAggregateAnswerText;
    QString m_streamCurrentAnswerMessageId;
    QString m_streamWorkPrefix;
    QHash<QString, StreamWorkStep> m_streamWorkSteps;
    QHash<QString, QString> m_progressSegmentStepIds;
    QHash<QString, AiChatTrace> m_messageTraces;
    QSet<QString> m_expandedTraceMessageIds;
    QSet<QString> m_collapsingTraceMessageIds;
    QHash<QString, QPointer<QVariantAnimation>> m_traceExpansionAnimations;
    int m_nextWorkEncounterOrder = 1;
    int m_streamActionSerial = 0;

    static constexpr int kHeaderHeight = 62;
    static constexpr int kHeaderTitleHeight = 28;
    static constexpr int kHeaderTitleLeft = 20;
    static constexpr int kHeaderTitleRight = 18;
    static constexpr int kHeaderTitleBottomMargin = 10;
    static constexpr int kInputBarHeight = 104;
    static constexpr int kInputBarSideMargin = 24;
    static constexpr int kInputBarBottomMargin = 18;
    static constexpr int kStartPageTitleHeight = 48;
    static constexpr int kStartPageTitleInputGap = 28;
    static constexpr int kStartPageMaxInputWidth = 880;
    static constexpr int kListBottomPadding = 10;
    static constexpr int kNewMessageNotifierInputGap = 10;
    static constexpr int kNewMessageNotifierMinBottomDistance = 220;
    static constexpr int kNewMessageNotifierViewportDistanceDivisor = 2;
    static constexpr int kThinkingAnimationFrameMs = 16;
    static constexpr int kTraceExpandAnimationDurationMs = 420;
};
