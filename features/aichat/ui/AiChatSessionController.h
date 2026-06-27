#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QTimer>

#include "shared/network/NetworkTypes.h"
#include "shared/types/RepositoryTypes.h"

class AiChatStreamClient;

class AiChatSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AiChatSessionController(QObject* parent = nullptr);

    QVector<AiChatListEntry> loadConversations(const AiChatListRequest& query = {}) const;
    QVector<AiChatMessage> loadMessages(const QString& conversationId) const;
    AiChatContextUsage loadContextUsage(const AiChatContextUsageRequest& request) const;
    int loadConversationsAsync(const AiChatListRequest& query = {});
    int loadMessagesAsync(const QString& conversationId);
    int loadContextUsageAsync(const AiChatContextUsageRequest& request);
    AiChatListEntry createConversationFromFirstMessage(const QString& firstUserMessage);
    AiChatMessage submitUserMessage(const QString& conversationId,
                                    const QString& text,
                                    const AiChatRequestOptions& options = {});
    bool regenerateAiReply(const QString& conversationId,
                           const QString& messageId,
                           const AiChatRequestOptions& options = {});
    bool renameConversation(const QString& conversationId, const QString& title);
    bool deleteConversation(const QString& conversationId);
    bool clearConversationUnreadDot(const QString& conversationId);
    int unreadConversationDotCount() const;
    void setCurrentConversationId(const QString& conversationId);

    bool hasActiveAiReplyStream() const;
    QString activeStreamConversationId() const;
    QString activeStreamMessageId() const;

public slots:
    void cancelActiveAiReplyStream();

signals:
    void conversationsChanged();
    void conversationsLoaded(int requestId,
                             const AiChatListRequest& query,
                             const QVector<AiChatListEntry>& entries);
    void messagesLoaded(int requestId,
                        const QString& conversationId,
                        const QVector<AiChatMessage>& messages);
    void contextUsageLoaded(int requestId,
                            const AiChatContextUsageRequest& request,
                            const AiChatContextUsage& usage);
    void aiReplyStarted(const QString& conversationId);
    void aiReplyMessageAdded(const AiChatMessage& message, bool isProgress);
    void aiReplyMessageUpdated(const QString& conversationId,
                               const QString& messageId,
                               const QString& text,
                               bool isProgress);
    void aiReplyMessageReplaced(const QString& conversationId,
                                const QString& messageId,
                                const AiChatMessage& replacement);
    void aiReplyMessageRemoved(const QString& conversationId, const QString& messageId);
    void aiReplyProgressChanged(const QString& conversationId,
                                const AiChatProgressSegment& progress);
    void aiReplyThinkingChanged(const QString& conversationId, bool active);
    void aiReplyStreamStatusChanged(const QString& conversationId,
                                    const AiChatStreamStatus& status);
    void aiReplyFinished(const QString& conversationId, const QString& messageId);
    void aiReplyCanceled(const QString& conversationId, const QString& messageId);
    void aiReplyFailed(const QString& conversationId,
                       const QString& messageId,
                       const QString& message);
    void conversationIdChanged(const QString& previousConversationId,
                               const AiChatListEntry& entry);
    void conversationEntryChanged(const AiChatListEntry& entry);
    void conversationTitleChanged(const QString& conversationId,
                                  const QString& title);
    void conversationDeleted(const QString& conversationId);
    void unreadDotStateChanged();

private slots:
    void onAiReplyStreamStarted(const QString& streamId,
                                const QString& conversationId,
                                const QString& clientMessageId);
    void onAiReplyChunkReceived(const AiChatStreamChunk& chunk);
    void revealNextStreamCharacter();
    void revealNextProgressCharacter();
    void onAiReplyThinkingChanged(bool active);
    void onAiReplyStreamStatusChanged(const AiChatStreamStatus& status);
    void onGeneratedTitleReceived(const QString& title);
    void onAssistantMessageReceived(const AiChatMessage& message);
    void onAiReplyCanceled();
    void onAiReplyFailed(const NetworkError& error);
    void onAiReplyFinished();
    void onConversationListRequestSucceeded(const QString& httpRequestId,
                                            const NetworkResponse& response);
    void onConversationListRequestFailed(const QString& httpRequestId,
                                         const NetworkError& error);
    void onMessageHistoryRequestSucceeded(const QString& httpRequestId,
                                          const NetworkResponse& response);
    void onMessageHistoryRequestFailed(const QString& httpRequestId,
                                       const NetworkError& error);

private:
    struct QueuedStreamText {
        QString text;
    };

    struct QueuedProgressText {
        QString stepId;
        QString segmentId;
        QString text;
        bool segmentEnd = false;
    };

    void appendVisibleStreamText(const QString& text);
    void clearStreamCharacterQueue();
    void clearProgressCharacterQueue();
    void finishProgressAnimations();
    QString resolvedToolStepId(const AiChatStreamStatus& status);
    void startAiReplyStream(const QString& conversationId,
                            const QString& prompt,
                            const AiChatRequestOptions& options = {});
    void resetActiveAiReplyStream();
    void discardMessageHistoryRequestsForConversation(const QString& conversationId);

    AiChatStreamClient* m_streamClient = nullptr;
    int m_nextAsyncRequestId = 1;
    QString m_streamConversationId;
    QString m_streamId;
    QString m_streamMessageId;
    QString m_streamVisibleText;
    QString m_currentConversationId;
    QQueue<QueuedStreamText> m_streamCharacterQueue;
    QTimer m_streamCharacterTimer;
    QQueue<QueuedProgressText> m_streamProgressCharacterQueue;
    QTimer m_streamProgressCharacterTimer;
    QHash<QString, AiChatProgressSegment> m_streamProgressSegments;
    QHash<QString, QString> m_legacyToolStepIds;
    QString m_latestToolStepId;
    int m_nextLegacyToolStepId = 1;
    bool m_streamAnswerStarted = false;
    bool m_streamFailed = false;
    QHash<QString, int> m_conversationListRequestIds;
    QHash<QString, AiChatListRequest> m_conversationListQueries;
    QHash<QString, int> m_messageHistoryRequestIds;
    QHash<QString, QString> m_messageHistoryConversationIds;
};
