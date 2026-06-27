#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class AiChatMessageListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        MessageIdRole = Qt::UserRole + 1,
        ConversationIdRole,
        TextRole,
        IsFromUserRole,
        TimeRole,
        IsBottomSpaceRole,
        BottomSpaceHeightRole,
        IsThinkingRole,
        IsThinkingActiveRole,
        TraceExpansionProgressRole,
        IsTraceDetailRole
    };

    explicit AiChatMessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setMessages(QVector<AiChatMessage> messages);
    void appendMessage(const AiChatMessage& message);
    bool insertMessageBefore(const AiChatMessage& message, const QString& beforeMessageId);
    bool removeMessage(const QString& messageId);
    bool removeMessageWithAdjacentTransientRows(const QString& messageId);
    bool replaceMessage(const QString& messageId, const AiChatMessage& replacement);
    bool updateMessageText(const QString& messageId, const QString& text);
    void setTransientMessages(const QString& messageIdPrefix,
                              const QVector<AiChatMessage>& messages,
                              const QSet<QString>& activeMessageIds,
                              const QString& beforeMessageId = {},
                              qreal traceExpansionProgress = 1.0);
    void setTraceExpansionProgress(const QString& messageIdPrefix, qreal progress);
    qreal traceExpansionProgress(const QString& messageIdPrefix) const;
    bool setThinkingMessageActive(const QString& messageId, bool active);
    void deactivateAllThinkingMessages();
    bool hasActiveThinkingMessages() const;
    bool isBottomSpace(int row) const;
    void setBottomSpaceHeight(int height);
    AiChatMessage messageAt(const QModelIndex& index) const;
    AiChatMessage messageById(const QString& messageId) const;
    void clear();

private:
    QVector<AiChatMessage> m_messages;
    QSet<QString> m_activeThinkingMessageIds;
    QHash<QString, qreal> m_traceExpansionProgress;
    int m_bottomSpaceHeight = 0;
};
