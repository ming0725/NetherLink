#include "features/aichat/ui/AiChatSessionController.h"

#include "features/aichat/data/AiChatRepository.h"
#include "features/aichat/data/AiChatStreamClient.h"
#include "features/aichat/data/AiChatTitleClient.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

AiChatSessionController::AiChatSessionController(QObject* parent)
    : QObject(parent)
    , m_streamClient(new AiChatStreamClient(this))
    , m_titleClient(new AiChatTitleClient(this))
{
    connect(m_streamClient, &AiChatStreamClient::chunkReceived,
            this, &AiChatSessionController::onAiReplyChunkReceived);
    connect(m_streamClient, &AiChatStreamClient::finished,
            this, &AiChatSessionController::onAiReplyFinished);
    connect(&AiChatRepository::instance(), &AiChatRepository::unreadDotStateChanged,
            this, [this]() {
                emit unreadDotStateChanged();
                emit conversationsChanged();
            });
}

QVector<AiChatListEntry> AiChatSessionController::loadConversations(const AiChatListRequest& query) const
{
    return AiChatRepository::instance().requestAiChatList(query);
}

QVector<AiChatMessage> AiChatSessionController::loadMessages(const QString& conversationId) const
{
    return AiChatRepository::instance().requestAiChatMessages(conversationId);
}

AiChatContextUsage AiChatSessionController::loadContextUsage(const AiChatContextUsageRequest& request) const
{
    return AiChatRepository::instance().requestAiChatContextUsage(request);
}

int AiChatSessionController::loadConversationsAsync(const AiChatListRequest& query)
{
    const int requestId = m_nextAsyncRequestId++;
    auto* watcher = new QFutureWatcher<QVector<AiChatListEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<AiChatListEntry>>::finished, this, [this, watcher, requestId, query]() {
        const QVector<AiChatListEntry> entries = watcher->result();
        watcher->deleteLater();
        emit conversationsLoaded(requestId, query, entries);
    });
    watcher->setFuture(QtConcurrent::run([query]() {
        return AiChatRepository::instance().requestAiChatList(query);
    }));
    return requestId;
}

int AiChatSessionController::loadMessagesAsync(const QString& conversationId)
{
    const int requestId = m_nextAsyncRequestId++;
    auto* watcher = new QFutureWatcher<QVector<AiChatMessage>>(this);
    connect(watcher, &QFutureWatcher<QVector<AiChatMessage>>::finished, this, [this, watcher, requestId, conversationId]() {
        const QVector<AiChatMessage> messages = watcher->result();
        watcher->deleteLater();
        emit messagesLoaded(requestId, conversationId, messages);
    });
    watcher->setFuture(QtConcurrent::run([conversationId]() {
        return AiChatRepository::instance().requestAiChatMessages(conversationId);
    }));
    return requestId;
}

int AiChatSessionController::loadContextUsageAsync(const AiChatContextUsageRequest& request)
{
    const int requestId = m_nextAsyncRequestId++;
    auto* watcher = new QFutureWatcher<AiChatContextUsage>(this);
    connect(watcher, &QFutureWatcher<AiChatContextUsage>::finished, this, [this, watcher, requestId, request]() {
        const AiChatContextUsage usage = watcher->result();
        watcher->deleteLater();
        emit contextUsageLoaded(requestId, request, usage);
    });
    watcher->setFuture(QtConcurrent::run([request]() {
        return AiChatRepository::instance().requestAiChatContextUsage(request);
    }));
    return requestId;
}

AiChatListEntry AiChatSessionController::createConversationFromFirstMessage(const QString& firstUserMessage)
{
    const QDateTime createdAt = QDateTime::currentDateTime();
    const QString title = m_titleClient
            ? m_titleClient->generateTitle(firstUserMessage)
            : QStringLiteral("新对话");
    const QString conversationId = AiChatRepository::instance().createAiChatConversation(title, createdAt);
    if (!conversationId.isEmpty()) {
        emit conversationsChanged();
    }
    return {conversationId, title, createdAt};
}

AiChatMessage AiChatSessionController::submitUserMessage(const QString& conversationId, const QString& text)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return {};
    }

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            conversationId,
            text,
            true);
    if (message.messageId.isEmpty()) {
        return {};
    }

    startAiReplyStream(conversationId, text);
    return message;
}

bool AiChatSessionController::regenerateAiReply(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return false;
    }

    if (hasActiveAiReplyStream() &&
            (conversationId != m_streamConversationId || messageId != m_streamMessageId)) {
        return false;
    }

    const QVector<AiChatMessage> messages = AiChatRepository::instance().requestAiChatMessages(conversationId);
    int replyRow = -1;
    for (int row = 0; row < messages.size(); ++row) {
        if (messages.at(row).messageId == messageId) {
            replyRow = row;
            break;
        }
    }

    if (replyRow < 0 ||
            replyRow != messages.size() - 1 ||
            messages.at(replyRow).isFromUser) {
        return false;
    }

    QString prompt;
    for (int row = replyRow - 1; row >= 0; --row) {
        if (messages.at(row).isFromUser) {
            prompt = messages.at(row).text;
            break;
        }
    }
    if (prompt.trimmed().isEmpty()) {
        return false;
    }

    if (conversationId == m_streamConversationId && messageId == m_streamMessageId) {
        if (m_streamClient) {
            m_streamClient->cancel();
        }
        resetActiveAiReplyStream();
    }

    if (!AiChatRepository::instance().removeAiChatMessage(conversationId, messageId)) {
        return false;
    }

    emit aiReplyMessageRemoved(conversationId, messageId);
    emit conversationsChanged();
    startAiReplyStream(conversationId, prompt);
    return true;
}

bool AiChatSessionController::renameConversation(const QString& conversationId, const QString& title)
{
    const bool renamed = AiChatRepository::instance().renameAiChatConversation(conversationId, title);
    if (renamed) {
        emit conversationsChanged();
    }
    return renamed;
}

bool AiChatSessionController::deleteConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    if (conversationId == m_streamConversationId) {
        cancelActiveAiReplyStream();
    }

    const bool removed = AiChatRepository::instance().removeAiChatConversation(conversationId);
    if (removed) {
        emit conversationsChanged();
    }
    return removed;
}

bool AiChatSessionController::clearConversationUnreadDot(const QString& conversationId)
{
    return AiChatRepository::instance().setConversationUnreadDot(conversationId, false);
}

int AiChatSessionController::unreadConversationDotCount() const
{
    return AiChatRepository::instance().unreadDotCount();
}

bool AiChatSessionController::hasActiveAiReplyStream() const
{
    return !m_streamConversationId.isEmpty() || (m_streamClient && m_streamClient->isRunning());
}

QString AiChatSessionController::activeStreamConversationId() const
{
    return m_streamConversationId;
}

QString AiChatSessionController::activeStreamMessageId() const
{
    return m_streamMessageId;
}

void AiChatSessionController::cancelActiveAiReplyStream()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    if (m_streamClient) {
        m_streamClient->cancel();
    }
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        emit aiReplyCanceled(conversationId, messageId);
    }
}

void AiChatSessionController::onAiReplyChunkReceived(const QString& chunk)
{
    if (chunk.isEmpty() || m_streamConversationId.isEmpty()) {
        return;
    }

    m_streamVisibleText += chunk;
    if (m_streamMessageId.isEmpty()) {
        const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
                m_streamConversationId,
                m_streamVisibleText,
                false);
        if (message.messageId.isEmpty()) {
            cancelActiveAiReplyStream();
            return;
        }

        m_streamMessageId = message.messageId;
        emit aiReplyMessageAdded(message);
        return;
    }

    AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                         m_streamMessageId,
                                                         m_streamVisibleText);
    emit aiReplyMessageUpdated(m_streamConversationId, m_streamMessageId, m_streamVisibleText);
}

void AiChatSessionController::onAiReplyFinished()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        AiChatRepository::instance().setConversationUnreadDot(conversationId, true);
        emit conversationsChanged();
        emit aiReplyFinished(conversationId, messageId);
    }
}

void AiChatSessionController::startAiReplyStream(const QString& conversationId, const QString& prompt)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return;
    }

    m_streamConversationId = conversationId;
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    emit aiReplyStarted(conversationId);

    m_streamClient->start(prompt);
    if (!m_streamClient->isRunning()) {
        cancelActiveAiReplyStream();
    }
}

void AiChatSessionController::resetActiveAiReplyStream()
{
    m_streamConversationId.clear();
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
}
