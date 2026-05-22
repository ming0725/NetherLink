#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class AiChatRepository : public QObject {
public:
    static AiChatRepository& instance();

    QVector<AiChatListEntry> requestAiChatList(const AiChatListRequest& query = {}) const;
    QVector<AiChatMessage> requestAiChatMessages(const QString& conversationId) const;
    AiChatContextUsage requestAiChatContextUsage(const AiChatContextUsageRequest& request) const;
    QString createAiChatConversation(const QString& title,
                                     const QDateTime& time = QDateTime::currentDateTime());
    AiChatMessage addAiChatMessage(const QString& conversationId,
                                   const QString& text,
                                   bool isFromUser,
                                   const QDateTime& time = QDateTime::currentDateTime());
    bool updateAiChatMessageText(const QString& conversationId,
                                 const QString& messageId,
                                 const QString& text,
                                 const QDateTime& time = QDateTime::currentDateTime());
    bool removeAiChatMessage(const QString& conversationId, const QString& messageId);
    bool renameAiChatConversation(const QString& conversationId, const QString& title);
    bool removeAiChatConversation(const QString& conversationId);

private:
    explicit AiChatRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(AiChatRepository)

    void appendInitialMessages(const AiChatListEntry& entry, int sampleIndex) const;
    AiChatContextUsage buildContextUsageLocked(const QString& conversationId) const;

    mutable QMutex m_mutex;
    QVector<AiChatListEntry> m_entries;
    mutable QHash<QString, QVector<AiChatMessage>> m_messages;
    mutable QSet<QString> m_seededMessageConversationIds;
    mutable QHash<QString, AiChatContextUsage> m_contextUsages;
    int m_nextConversationId = 1;
    mutable int m_nextMessageId = 1;
};
