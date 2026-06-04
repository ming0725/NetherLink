#include "AiChatRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryFunctionOperation.h"

#include <QJsonObject>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace {

QString normalizedMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QString normalizedAiMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QJsonObject aiChatEntryToJson(const AiChatListEntry& entry)
{
    return {
            {QStringLiteral("conversationId"), entry.conversationId},
            {QStringLiteral("title"), entry.title},
            {QStringLiteral("time"), entry.time.toString(Qt::ISODateWithMs)},
            {QStringLiteral("hasUnreadDot"), entry.hasUnreadDot}
    };
}

AiChatListEntry aiChatEntryFromJson(const QJsonObject& object)
{
    return {
            object.value(QStringLiteral("conversationId")).toString(),
            object.value(QStringLiteral("title")).toString(),
            QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs),
            object.value(QStringLiteral("hasUnreadDot")).toBool(false)
    };
}

QJsonObject aiChatMessageToJson(const AiChatMessage& message)
{
    return {
            {QStringLiteral("messageId"), message.messageId},
            {QStringLiteral("conversationId"), message.conversationId},
            {QStringLiteral("text"), message.text},
            {QStringLiteral("isFromUser"), message.isFromUser},
            {QStringLiteral("time"), message.time.toString(Qt::ISODateWithMs)}
    };
}

AiChatMessage aiChatMessageFromJson(const QJsonObject& object)
{
    return {
            object.value(QStringLiteral("messageId")).toString(),
            object.value(QStringLiteral("conversationId")).toString(),
            object.value(QStringLiteral("text")).toString(),
            object.value(QStringLiteral("isFromUser")).toBool(false),
            QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs)
    };
}

} // namespace

AiChatRepository::AiChatRepository(QObject* parent)
    : QObject(parent)
{
    LocalDataStore& store = LocalDataStore::instance();
    if (store.hasDomain(QStringLiteral("ai_chat_entries"))) {
        int maxConversationSerial = 0;
        int maxMessageSerial = 0;
        for (const QJsonObject& object : store.values(QStringLiteral("ai_chat_entries"))) {
            const AiChatListEntry entry = aiChatEntryFromJson(object);
            if (!entry.conversationId.isEmpty()) {
                m_entries.push_back(entry);
                bool ok = false;
                const int serial = entry.conversationId.mid(QStringLiteral("ai-chat-").size()).toInt(&ok);
                if (ok) {
                    maxConversationSerial = qMax(maxConversationSerial, serial);
                }
            }
        }
        for (const QJsonObject& object : store.values(QStringLiteral("ai_chat_messages"))) {
            const AiChatMessage message = aiChatMessageFromJson(object);
            if (!message.conversationId.isEmpty() && !message.messageId.isEmpty()) {
                m_messages[message.conversationId].push_back(message);
                bool ok = false;
                const int serial = message.messageId.mid(QStringLiteral("ai-message-").size()).toInt(&ok);
                if (ok) {
                    maxMessageSerial = qMax(maxMessageSerial, serial);
                }
            }
        }
        m_nextConversationId = maxConversationSerial + 1;
        m_nextMessageId = maxMessageSerial + 1;
    }
}

AiChatRepository& AiChatRepository::instance()
{
    static AiChatRepository repo;
    return repo;
}

QVector<AiChatListEntry> AiChatRepository::requestAiChatList(const AiChatListRequest& query) const
{
    auto handler = [this](const AiChatListRequest& request) {
        QMutexLocker locker(&m_mutex);
        if (request.limit <= 0 || request.offset < 0 || request.offset >= m_entries.size()) {
            return QVector<AiChatListEntry>{};
        }

        QVector<AiChatListEntry> sortedEntries = m_entries;
        std::sort(sortedEntries.begin(), sortedEntries.end(), [](const AiChatListEntry& lhs, const AiChatListEntry& rhs) {
            return lhs.time > rhs.time;
        });

        const int offset = qBound(0, request.offset, sortedEntries.size());
        const int limit = qMax(0, request.limit);
        return sortedEntries.mid(offset, limit);
    };

    return RepositoryFunctionOperation<AiChatListRequest, QVector<AiChatListEntry>, decltype(handler)>(handler)
            .request(query);
}

QVector<AiChatMessage> AiChatRepository::requestAiChatMessages(const AiChatMessagesRequest& query) const
{
    auto handler = [this](const AiChatMessagesRequest& request) {
        QMutexLocker locker(&m_mutex);
        return m_messages.value(request.conversationId);
    };

    return RepositoryFunctionOperation<AiChatMessagesRequest, QVector<AiChatMessage>, decltype(handler)>(handler)
            .request(query);
}

QVector<AiChatMessage> AiChatRepository::requestAiChatMessages(const QString& conversationId) const
{
    return requestAiChatMessages({conversationId});
}

AiChatContextUsage AiChatRepository::requestAiChatContextUsage(const AiChatContextUsageRequest& request) const
{
    auto handler = [this](const AiChatContextUsageRequest& query) {
        QMutexLocker locker(&m_mutex);
        if (query.conversationId.isEmpty()) {
            return AiChatContextUsage{};
        }

        const AiChatContextUsage usage = buildContextUsageLocked(query.conversationId);
        if (usage.available) {
            m_contextUsages.insert(query.conversationId, usage);
        } else {
            m_contextUsages.remove(query.conversationId);
        }
        return usage;
    };

    return RepositoryFunctionOperation<AiChatContextUsageRequest, AiChatContextUsage, decltype(handler)>(handler)
            .request(request);
}

QString AiChatRepository::createAiChatConversation(const QString& title, const QDateTime& time)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty() || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const QString conversationId = QStringLiteral("ai-chat-%1").arg(m_nextConversationId++);
    const AiChatListEntry entry {conversationId, trimmedTitle, time, false};
    m_entries.push_back(entry);
    m_messages.insert(conversationId, {});
    LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                           conversationId,
                                           aiChatEntryToJson(entry));
    return conversationId;
}

AiChatMessage AiChatRepository::addAiChatMessage(const QString& conversationId,
                                                 const QString& text,
                                                 bool isFromUser,
                                                 const QDateTime& time)
{
    const QString messageText = isFromUser ? normalizedMessageText(text) : normalizedAiMessageText(text);
    if (conversationId.isEmpty() || (isFromUser && messageText.isEmpty()) || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
        return entry.conversationId == conversationId;
    });
    if (entryIt == m_entries.end()) {
        return {};
    }

    AiChatMessage message {
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            conversationId,
            messageText,
            isFromUser,
            time
    };
    m_messages[conversationId].push_back(message);
    m_contextUsages.remove(conversationId);
    LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_messages"),
                                           message.messageId,
                                           aiChatMessageToJson(message));
    return message;
}

bool AiChatRepository::updateAiChatMessageText(const QString& conversationId,
                                               const QString& messageId,
                                               const QString& text,
                                               const QDateTime& time)
{
    const QString messageText = normalizedAiMessageText(text);
    if (conversationId.isEmpty() || messageId.isEmpty() || !time.isValid()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    for (AiChatMessage& message : messagesIt.value()) {
        if (message.messageId == messageId) {
            message.text = messageText;
            message.time = time;
            m_contextUsages.remove(conversationId);
            LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_messages"),
                                                   message.messageId,
                                                   aiChatMessageToJson(message));
            return true;
        }
    }

    return false;
}

bool AiChatRepository::replaceAiChatMessage(const QString& conversationId,
                                            const QString& messageId,
                                            const AiChatMessage& replacement)
{
    if (conversationId.isEmpty() || messageId.isEmpty() ||
            replacement.messageId.isEmpty() ||
            replacement.conversationId != conversationId ||
            !replacement.time.isValid()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    QVector<AiChatMessage>& messages = messagesIt.value();
    for (int row = 0; row < messages.size(); ++row) {
        if (messages.at(row).messageId != messageId) {
            continue;
        }

        const QString oldMessageId = messages.at(row).messageId;
        messages[row] = replacement;
        m_contextUsages.remove(conversationId);
        if (oldMessageId != replacement.messageId) {
            LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_messages"), oldMessageId);
        }
        LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_messages"),
                                               replacement.messageId,
                                               aiChatMessageToJson(replacement));
        return true;
    }

    return false;
}

bool AiChatRepository::setConversationUnreadDot(const QString& conversationId, bool unread)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        for (AiChatListEntry& entry : m_entries) {
            if (entry.conversationId != conversationId) {
                continue;
            }

            if (entry.hasUnreadDot == unread) {
                return false;
            }

            entry.hasUnreadDot = unread;
            LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                                   entry.conversationId,
                                                   aiChatEntryToJson(entry));
            changed = true;
            break;
        }
    }

    if (changed) {
        emit unreadDotStateChanged();
    }
    return changed;
}

int AiChatRepository::unreadDotCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const AiChatListEntry& entry : m_entries) {
        if (entry.hasUnreadDot) {
            ++count;
        }
    }
    return count;
}

bool AiChatRepository::removeAiChatMessage(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    QVector<AiChatMessage>& messages = messagesIt.value();
    for (int row = 0; row < messages.size(); ++row) {
        if (messages.at(row).messageId != messageId) {
            continue;
        }

        messages.removeAt(row);
        m_contextUsages.remove(conversationId);
        LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_messages"), messageId);
        return true;
    }

    return false;
}

bool AiChatRepository::renameAiChatConversation(const QString& conversationId, const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    for (AiChatListEntry& entry : m_entries) {
        if (entry.conversationId == conversationId) {
            entry.title = trimmedTitle;
            LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                                   entry.conversationId,
                                                   aiChatEntryToJson(entry));
            return true;
        }
    }
    return false;
}

bool AiChatRepository::removeAiChatConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    bool hadUnreadDot = false;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
            return entry.conversationId == conversationId;
        });
        if (it == m_entries.end()) {
            return false;
        }

        hadUnreadDot = it->hasUnreadDot;
        m_entries.erase(it);
        const QVector<AiChatMessage> removedMessages = m_messages.take(conversationId);
        m_contextUsages.remove(conversationId);
        LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_entries"), conversationId);
        for (const AiChatMessage& message : removedMessages) {
            LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_messages"), message.messageId);
        }
    }

    if (hadUnreadDot) {
        emit unreadDotStateChanged();
    }
    return true;
}

AiChatContextUsage AiChatRepository::buildContextUsageLocked(const QString& conversationId) const
{
    const QVector<AiChatMessage> messages = m_messages.value(conversationId);
    if (messages.isEmpty()) {
        return {conversationId, 0, 128000, false};
    }

    int characterCount = 0;
    int userMessageCount = 0;
    int assistantMessageCount = 0;
    for (const AiChatMessage& message : messages) {
        characterCount += message.text.size();
        if (message.isFromUser) {
            ++userMessageCount;
        } else {
            ++assistantMessageCount;
        }
    }

    const int maxTokens = 128000;
    const int structuralTokens = 900 + messages.size() * 96 + userMessageCount * 34 + assistantMessageCount * 56;
    const int simulatedContentTokens = qCeil(static_cast<qreal>(characterCount) / 2.8);
    const int usedTokens = qBound(1, structuralTokens + simulatedContentTokens, maxTokens);
    return {conversationId, usedTokens, maxTokens, true};
}
