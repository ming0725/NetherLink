#pragma once

#include <QObject>
#include <QMap>
#include <QHash>
#include <QVector>
#include <QMutex>
#include <QJsonObject>
#include <QSharedPointer>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/ChatMessage.h"

class MessageRepository : public QObject {
    Q_OBJECT
public:
    static MessageRepository& instance();

    QVector<ConversationSummary> requestConversationList(const ConversationListRequest& query = {}) const;
    ChatMessageList requestConversationMessages(const ConversationMessagesRequest& query) const;
    QSharedPointer<ChatMessage> requestMessageById(const QString& conversationId,
                                                   const QString& messageId) const;
    ConversationMeta requestConversationMeta(const ConversationMetaRequest& query) const;
    ConversationThreadData requestConversationThread(const ConversationThreadRequest& query) const;
    ConversationThreadData requestConversationThreadUntilMessage(
            const ConversationThreadUntilMessageRequest& query) const;
    QString requestConversationThreadAsync(const ConversationThreadRequest& query);
    void setActiveVisibleConversation(const QString& conversationId, bool visible);
    bool isActiveVisibleConversation(const QString& conversationId) const;

public slots:
    void touchConversation(const QString& conversationId,
                           const QDateTime& timestamp = QDateTime::currentDateTime());
    void markConversationRead(const QString& conversationId);
    void markConversationUnread(const QString& conversationId, int unreadCount = 1);
    void setConversationDoNotDisturb(const QString& conversationId, bool enabled);
    void setConversationPinned(const QString& conversationId, bool pinned);
    void clearConversationMessages(const QString& conversationId);
    void removeConversation(const QString& conversationId);
    void addMessage(const QString& conversationId,
                    QSharedPointer<ChatMessage> message);
    void persistMessage(const QString& conversationId,
                        const QSharedPointer<ChatMessage>& message);
    void refreshGroupMemberDisplayName(const QString& groupId,
                                       const QString& userId,
                                       const QString& displayName,
                                       GroupRole role);
    bool replaceMessage(const QString& conversationId,
                        const QSharedPointer<ChatMessage>& oldMessage,
                        QSharedPointer<ChatMessage> newMessage);
    void removeMessage(const QString& conversationId, int index);
    bool removeMessage(const QString& conversationId,
                       const QSharedPointer<ChatMessage>& message);

signals:
    // conversationId 对应的最后一条消息已更新（nullptr 表示已无消息）
    void lastMessageChanged(const QString& conversationId,
                            QSharedPointer<ChatMessage> lastMessage);
    void messageUpdated(const QString& conversationId,
                        QSharedPointer<ChatMessage> message);
    void conversationListChanged(const QString& conversationId);
    void conversationThreadReady(const QString& requestId,
                                 const ConversationThreadData& thread);

private:
    explicit MessageRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(MessageRepository)
    void reloadFromStore();
    void scheduleReloadFromStore();
    bool shouldIgnoreStoreChange(const QString& domain);
    void ignoreNextStoreChange(const QString& domain);
    bool localUnreadOverride(const QString& conversationId, int* unreadCount = nullptr) const;
    void applyConversationStateObject(const QJsonObject& conversation, bool emitChange = true);
    void cacheRemoteMessageObject(QJsonObject object,
                                  const QString& fallbackConversationId = QString(),
                                  bool updateUnread = true);
    bool fetchOlderMessagesBlocking(const QString& conversationId, int beforeMessageSeq, int limit);
    bool fetchLatestMessagesBlocking(const QString& conversationId, int limit);
    bool fetchNewerMessagesBlocking(const QString& conversationId, int afterMessageSeq, int limit);

    QMap<QString, QVector<QSharedPointer<ChatMessage>>> m_store;
    QMap<QString, ConversationSyncState> m_conversationStates;
    QMap<QString, QString> m_directConversationPeers;
    QString m_activeVisibleConversationId;
    bool m_hasActiveVisibleConversation = false;
    QHash<QString, int> m_ignoredStoreChangeCounts;
    QHash<QString, int> m_localUnreadOverrides;
    bool m_reloadFromStoreScheduled = false;
    mutable QMutex m_mutex;
};
