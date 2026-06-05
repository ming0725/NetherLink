#pragma once

#include <QHash>
#include <QObject>

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
    AiChatMessage submitUserMessage(const QString& conversationId, const QString& text);
    bool regenerateAiReply(const QString& conversationId, const QString& messageId);
    bool renameConversation(const QString& conversationId, const QString& title);
    bool deleteConversation(const QString& conversationId);
    bool clearConversationUnreadDot(const QString& conversationId);
    int unreadConversationDotCount() const;

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
    void aiReplyMessageAdded(const AiChatMessage& message);
    void aiReplyMessageUpdated(const QString& conversationId,
                               const QString& messageId,
                               const QString& text);
    void aiReplyMessageReplaced(const QString& conversationId,
                                const QString& messageId,
                                const AiChatMessage& replacement);
    void aiReplyMessageRemoved(const QString& conversationId, const QString& messageId);
    void aiReplyFinished(const QString& conversationId, const QString& messageId);
    void aiReplyCanceled(const QString& conversationId, const QString& messageId);
    void aiReplyFailed(const QString& conversationId,
                       const QString& messageId,
                       const QString& message);
    void conversationIdChanged(const QString& previousConversationId,
                               const AiChatListEntry& entry);
    void conversationTitleChanged(const QString& conversationId,
                                  const QString& title);
    void conversationDeleted(const QString& conversationId);
    void unreadDotStateChanged();

private slots:
    void onAiReplyChunkReceived(const QString& chunk);
    void onGeneratedTitleReceived(const QString& title);
    void onAssistantMessageReceived(const QString& messageId,
                                    const QString& text,
                                    const QDateTime& time);
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
    void startAiReplyStream(const QString& conversationId, const QString& prompt);
    void resetActiveAiReplyStream();
    void discardMessageHistoryRequestsForConversation(const QString& conversationId);

    AiChatStreamClient* m_streamClient = nullptr;
    int m_nextAsyncRequestId = 1;
    QString m_streamConversationId;
    QString m_streamMessageId;
    QString m_streamVisibleText;
    bool m_streamFailed = false;
    QHash<QString, int> m_conversationListRequestIds;
    QHash<QString, AiChatListRequest> m_conversationListQueries;
    QHash<QString, int> m_messageHistoryRequestIds;
    QHash<QString, QString> m_messageHistoryConversationIds;
};
