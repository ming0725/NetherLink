#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class AiChatRepository : public QObject {
    Q_OBJECT

public:
    static AiChatRepository& instance();

    QVector<AiChatListEntry> requestAiChatList(const AiChatListRequest& query = {}) const;
    AiChatListEntry requestAiChatConversation(const QString& conversationId) const;
    bool setAiChatListPage(const AiChatListRequest& query,
                           const QVector<AiChatListEntry>& entries);
    QVector<AiChatMessage> requestAiChatMessages(const AiChatMessagesRequest& query) const;
    QVector<AiChatMessage> requestAiChatMessages(const QString& conversationId) const;
    AiChatContextUsage requestAiChatContextUsage(const AiChatContextUsageRequest& request) const;
    QString createAiChatConversation(const QString& title,
                                     const QDateTime& time = QDateTime::currentDateTime());
    QString createAiChatConversation(const QString& conversationId,
                                     const QString& title,
                                     const QDateTime& time = QDateTime::currentDateTime());
    bool replaceAiChatConversationId(const QString& previousConversationId,
                                     const QString& conversationId);
    AiChatMessage addAiChatMessage(const QString& conversationId,
                                   const QString& text,
                                   bool isFromUser,
                                   const QDateTime& time = QDateTime::currentDateTime());
    bool updateAiChatMessageText(const QString& conversationId,
                                 const QString& messageId,
                                 const QString& text,
                                 const QDateTime& time = QDateTime::currentDateTime());
    bool setAiChatMessages(const QString& conversationId,
                           const QVector<AiChatMessage>& messages);
    bool replaceAiChatMessage(const QString& conversationId,
                              const QString& messageId,
                              const AiChatMessage& replacement);
    bool setConversationUnreadDot(const QString& conversationId, bool unread);
    int unreadDotCount() const;
    bool removeAiChatMessage(const QString& conversationId, const QString& messageId);
    bool renameAiChatConversation(const QString& conversationId, const QString& title);
    bool removeAiChatConversation(const QString& conversationId);

signals:
    void unreadDotStateChanged();

private:
    explicit AiChatRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(AiChatRepository)

    AiChatContextUsage buildContextUsageLocked(const QString& conversationId) const;

    mutable QMutex m_mutex;
    QVector<AiChatListEntry> m_entries;
    mutable QHash<QString, QVector<AiChatMessage>> m_messages;
    mutable QHash<QString, AiChatContextUsage> m_contextUsages;
    int m_nextConversationId = 1;
    mutable int m_nextMessageId = 1;
};
