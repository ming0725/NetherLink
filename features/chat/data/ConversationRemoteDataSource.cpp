#include "ConversationRemoteDataSource.h"

#include "shared/data/LocalDataStore.h"
#include "shared/network/HttpClient.h"
#include "shared/network/ReferenceDataResolver.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QStringList>
#include <QUuid>

namespace {

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QJsonObject conversationObjectFromResponse(QJsonObject object)
{
    if (object.value(QStringLiteral("conversation")).isObject()) {
        object = object.value(QStringLiteral("conversation")).toObject();
    } else if (object.value(QStringLiteral("data")).isObject()) {
        object = object.value(QStringLiteral("data")).toObject();
        if (object.value(QStringLiteral("conversation")).isObject()) {
            object = object.value(QStringLiteral("conversation")).toObject();
        }
    }
    return object;
}

QString conversationIdFromObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("conversationId"), QStringLiteral("id")});
}

bool isBackendConversationId(const QString& conversationId)
{
    return !QUuid::fromString(conversationId).isNull();
}

} // namespace

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
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    return sendSettingsOperation(Action::SetPinned,
                                 conversationId,
                                 {{QStringLiteral("isPinned"), pinned}},
                                 pinned);
}

QString ConversationRemoteDataSource::setDoNotDisturb(const QString& conversationId, bool enabled)
{
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    return sendSettingsOperation(Action::SetDoNotDisturb,
                                 conversationId,
                                 {{QStringLiteral("isDnd"), enabled}},
                                 enabled);
}

QString ConversationRemoteDataSource::hideConversation(const QString& conversationId)
{
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    return sendSettingsOperation(Action::HideConversation,
                                 conversationId,
                                 {{QStringLiteral("hidden"), true}},
                                 true);
}

QString ConversationRemoteDataSource::markRead(const QString& conversationId)
{
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    const QString pendingRequestId = m_markReadRequestByConversation.value(conversationId);
    if (!pendingRequestId.isEmpty()) {
        m_markReadAgainAfterPending.insert(conversationId);
        return pendingRequestId;
    }

    const QString requestId = sendSimpleOperation(Action::MarkRead,
                                                  HttpMethod::Post,
                                                  QStringLiteral("/conversations/%1/read").arg(conversationId),
                                                  conversationId,
                                                  {});
    if (!requestId.isEmpty()) {
        m_markReadRequestByConversation.insert(conversationId, requestId);
    }
    return requestId;
}

QString ConversationRemoteDataSource::markUnread(const QString& conversationId)
{
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    return sendSimpleOperation(Action::MarkUnread,
                               HttpMethod::Post,
                               QStringLiteral("/conversations/%1/unread").arg(conversationId),
                               conversationId,
                               {});
}

QString ConversationRemoteDataSource::clearMessages(const QString& conversationId)
{
    if (conversationId.isEmpty() || !isBackendConversationId(conversationId)) {
        return {};
    }

    return sendSimpleOperation(Action::ClearMessages,
                               HttpMethod::Delete,
                               QStringLiteral("/conversations/%1/messages").arg(conversationId),
                               conversationId);
}

QString ConversationRemoteDataSource::openDirectConversation(const QString& peerUserUuid)
{
    if (peerUserUuid.isEmpty()) {
        return {};
    }

    PendingOperation pending;
    pending.action = Action::OpenDirect;
    pending.conversationId = peerUserUuid;

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Post,
            QStringLiteral("/conversations/direct"),
            {{QStringLiteral("peerUserUuid"), peerUserUuid}});
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    if (requestId.isEmpty()) {
        return {};
    }

    m_pendingOperations.insert(requestId, pending);
    return requestId;
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
    if ((action == Action::MarkRead || action == Action::MarkUnread) &&
        method != HttpMethod::Get &&
        body.isEmpty()) {
        request.body = QJsonDocument(QJsonObject{});
        request.hasJsonBody = true;
    }
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
    case Action::OpenDirect:
        return QStringLiteral("openDirectConversation");
    }
    return {};
}

void ConversationRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    if (pending.action == Action::MarkRead) {
        m_markReadRequestByConversation.remove(pending.conversationId);
        if (m_markReadAgainAfterPending.remove(pending.conversationId) > 0) {
            QTimer::singleShot(0, this, [this, conversationId = pending.conversationId]() {
                markRead(conversationId);
            });
        }
    }
    switch (pending.action) {
    case Action::OpenDirect: {
        ReferenceDataResolver::instance().consumePayload(response.object());
        const QJsonObject conversation = conversationObjectFromResponse(response.object());
        const QString conversationId = conversationIdFromObject(conversation);
        if (!conversationId.isEmpty()) {
            LocalDataStore::instance().upsertValue(QStringLiteral("conversations"),
                                                   conversationId,
                                                   conversation);
        }
        emit directConversationOpened(requestId, pending.conversationId, conversationId);
        break;
    }
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
    if (pending.action == Action::MarkRead) {
        m_markReadRequestByConversation.remove(pending.conversationId);
        m_markReadAgainAfterPending.remove(pending.conversationId);
    }
    emit operationFailed(requestId,
                         pending.conversationId,
                         operationName(pending.action),
                         error);
}
