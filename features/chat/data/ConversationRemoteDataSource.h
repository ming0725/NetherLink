#pragma once

#include "shared/network/NetworkTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

class ConversationRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static ConversationRemoteDataSource& instance();

    QString setPinned(const QString& conversationId, bool pinned);
    QString setDoNotDisturb(const QString& conversationId, bool enabled);
    QString hideConversation(const QString& conversationId);
    QString markRead(const QString& conversationId);
    QString markUnread(const QString& conversationId);
    QString clearMessages(const QString& conversationId);
    QString openDirectConversation(const QString& peerUserUuid);
    QString openGroupConversation(const QString& groupId);
    QString fetchConversation(const QString& conversationId);

signals:
    void conversationFetched(const QString& requestId, const QString& conversationId);
    void directConversationOpened(const QString& requestId,
                                  const QString& peerUserUuid,
                                  const QString& conversationId);
    void groupConversationOpened(const QString& requestId,
                                 const QString& groupId,
                                 const QString& conversationId);
    void pinnedUpdated(const QString& requestId, const QString& conversationId, bool pinned);
    void doNotDisturbUpdated(const QString& requestId, const QString& conversationId, bool enabled);
    void conversationHidden(const QString& requestId, const QString& conversationId);
    void conversationMarkedRead(const QString& requestId, const QString& conversationId);
    void conversationMarkedUnread(const QString& requestId, const QString& conversationId);
    void messagesCleared(const QString& requestId, const QString& conversationId);
    void operationFailed(const QString& requestId,
                         const QString& conversationId,
                         const QString& operation,
                         const NetworkError& error);

private:
    enum class Action {
        SetPinned,
        SetDoNotDisturb,
        HideConversation,
        MarkRead,
        MarkUnread,
        ClearMessages,
        OpenDirect,
        OpenGroup,
        FetchConversation
    };

    struct PendingOperation {
        Action action = Action::SetPinned;
        QString conversationId;
        bool value = false;
    };

    explicit ConversationRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(ConversationRemoteDataSource)

    QString sendSettingsOperation(Action action,
                                  const QString& conversationId,
                                  const QJsonObject& body,
                                  bool value = false);
    QString sendSimpleOperation(Action action,
                                HttpMethod method,
                                const QString& path,
                                const QString& conversationId,
                                const QJsonObject& body = {});
    QString operationName(Action action) const;
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, PendingOperation> m_pendingOperations;
    QHash<QString, QString> m_markReadRequestByConversation;
    QSet<QString> m_markReadAgainAfterPending;
};
