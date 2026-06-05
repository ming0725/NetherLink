#include "ConversationRemoteDataSource.h"

#include "shared/network/HttpClient.h"

#include <QJsonObject>

ConversationRemoteDataSource& ConversationRemoteDataSource::instance()
{
    static ConversationRemoteDataSource dataSource;
    return dataSource;
}

ConversationRemoteDataSource::ConversationRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &ConversationRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &ConversationRemoteDataSource::handleRequestFailed);
}

QString ConversationRemoteDataSource::setPinned(const QString& conversationId, bool pinned)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSettingsOperation(Action::SetPinned,
                                 conversationId,
                                 {{QStringLiteral("isPinned"), pinned}},
                                 pinned);
}

QString ConversationRemoteDataSource::setDoNotDisturb(const QString& conversationId, bool enabled)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSettingsOperation(Action::SetDoNotDisturb,
                                 conversationId,
                                 {{QStringLiteral("isDnd"), enabled}},
                                 enabled);
}

QString ConversationRemoteDataSource::hideConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSettingsOperation(Action::HideConversation,
                                 conversationId,
                                 {{QStringLiteral("hidden"), true}},
                                 true);
}

QString ConversationRemoteDataSource::markRead(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSimpleOperation(Action::MarkRead,
                               HttpMethod::Post,
                               QStringLiteral("/conversations/%1/read").arg(conversationId),
                               conversationId);
}

QString ConversationRemoteDataSource::markUnread(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSimpleOperation(Action::MarkUnread,
                               HttpMethod::Post,
                               QStringLiteral("/conversations/%1/unread").arg(conversationId),
                               conversationId);
}

QString ConversationRemoteDataSource::clearMessages(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return sendSimpleOperation(Action::ClearMessages,
                               HttpMethod::Delete,
                               QStringLiteral("/conversations/%1/messages").arg(conversationId),
                               conversationId);
}

QString ConversationRemoteDataSource::sendSettingsOperation(Action action,
                                                           const QString& conversationId,
                                                           const QJsonObject& body,
                                                           bool)
{
    return sendSimpleOperation(action,
                               HttpMethod::Patch,
                               QStringLiteral("/conversations/%1/settings").arg(conversationId),
                               conversationId,
                               body.isEmpty() ? QJsonObject{} : body);
}

QString ConversationRemoteDataSource::sendSimpleOperation(Action action,
                                                          HttpMethod method,
                                                          const QString& path,
                                                          const QString& conversationId,
                                                          const QJsonObject& body)
{
    NetworkRequest request = NetworkRequest::json(method, path, body);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = action;
    pending.conversationId = conversationId;
    if (body.contains(QStringLiteral("isPinned"))) {
        pending.value = body.value(QStringLiteral("isPinned")).toBool();
    } else if (body.contains(QStringLiteral("isDnd"))) {
        pending.value = body.value(QStringLiteral("isDnd")).toBool();
    } else if (body.contains(QStringLiteral("hidden"))) {
        pending.value = body.value(QStringLiteral("hidden")).toBool();
    }
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

QString ConversationRemoteDataSource::operationName(Action action) const
{
    switch (action) {
    case Action::SetPinned:
        return QStringLiteral("setPinned");
    case Action::SetDoNotDisturb:
        return QStringLiteral("setDoNotDisturb");
    case Action::HideConversation:
        return QStringLiteral("hideConversation");
    case Action::MarkRead:
        return QStringLiteral("markRead");
    case Action::MarkUnread:
        return QStringLiteral("markUnread");
    case Action::ClearMessages:
        return QStringLiteral("clearMessages");
    }
    return {};
}

void ConversationRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse&)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::SetPinned:
        emit pinnedUpdated(requestId, pending.conversationId, pending.value);
        break;
    case Action::SetDoNotDisturb:
        emit doNotDisturbUpdated(requestId, pending.conversationId, pending.value);
        break;
    case Action::HideConversation:
        emit conversationHidden(requestId, pending.conversationId);
        break;
    case Action::MarkRead:
        emit conversationMarkedRead(requestId, pending.conversationId);
        break;
    case Action::MarkUnread:
        emit conversationMarkedUnread(requestId, pending.conversationId);
        break;
    case Action::ClearMessages:
        emit messagesCleared(requestId, pending.conversationId);
        break;
    }
}

void ConversationRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    emit operationFailed(requestId,
                         pending.conversationId,
                         operationName(pending.action),
                         error);
}
