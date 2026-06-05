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

bool AiChatRepository::setAiChatListPage(const AiChatListRequest& query,
                                         const QVector<AiChatListEntry>& entries)
{
    if (query.offset < 0 || query.limit <= 0) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    QHash<QString, bool> unreadDots;
    for (const AiChatListEntry& entry : std::as_const(m_entries)) {
        if (!entry.conversationId.isEmpty()) {
            unreadDots.insert(entry.conversationId, entry.hasUnreadDot);
        }
    }

    if (query.offset == 0) {
        m_entries.clear();
        LocalDataStore::instance().clearDomain(QStringLiteral("ai_chat_entries"));
    }

    for (AiChatListEntry entry : entries) {
        if (entry.conversationId.isEmpty() || entry.title.trimmed().isEmpty() || !entry.time.isValid()) {
            continue;
        }

        entry.title = entry.title.trimmed();
        if (unreadDots.contains(entry.conversationId)) {
            entry.hasUnreadDot = unreadDots.value(entry.conversationId);
        }

        const auto existingIt = std::find_if(m_entries.begin(), m_entries.end(), [&entry](const AiChatListEntry& existing) {
            return existing.conversationId == entry.conversationId;
        });
        if (existingIt == m_entries.end()) {
            m_entries.push_back(entry);
        } else {
            *existingIt = entry;
        }
        if (!m_messages.contains(entry.conversationId)) {
            m_messages.insert(entry.conversationId, {});
        }
        LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                               entry.conversationId,
                                               aiChatEntryToJson(entry));
    }
    m_contextUsages.clear();
    return true;
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
    const AiChatMessagesRequest request {conversationId};
    return requestAiChatMessages(request);
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

QString AiChatRepository::createAiChatConversation(const QString& conversationId,
                                                   const QString& title,
                                                   const QDateTime& time)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty() || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const auto existingIt = std::find_if(m_entries.cbegin(), m_entries.cend(), [&conversationId](const AiChatListEntry& entry) {
        return entry.conversationId == conversationId;
    });
    if (existingIt != m_entries.cend()) {
        return {};
    }

    const AiChatListEntry entry {conversationId, trimmedTitle, time, false};
    m_entries.push_back(entry);
    m_messages.insert(conversationId, {});
    LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                           conversationId,
                                           aiChatEntryToJson(entry));
    return conversationId;
}

bool AiChatRepository::replaceAiChatConversationId(const QString& previousConversationId,
                                                   const QString& conversationId)
{
    if (previousConversationId.isEmpty() ||
            conversationId.isEmpty() ||
            previousConversationId == conversationId) {
        return false;
    }

    bool hadUnreadDot = false;
    {
        QMutexLocker locker(&m_mutex);
        const auto previousIt = std::find_if(m_entries.begin(), m_entries.end(), [&previousConversationId](const AiChatListEntry& entry) {
            return entry.conversationId == previousConversationId;
        });
        if (previousIt == m_entries.end()) {
            return false;
        }

        const auto duplicateIt = std::find_if(m_entries.cbegin(), m_entries.cend(), [&conversationId](const AiChatListEntry& entry) {
            return entry.conversationId == conversationId;
        });
        if (duplicateIt != m_entries.cend()) {
            return false;
        }

        AiChatListEntry entry = *previousIt;
        hadUnreadDot = entry.hasUnreadDot;
        entry.conversationId = conversationId;
        *previousIt = entry;

        QVector<AiChatMessage> messages = m_messages.take(previousConversationId);
        for (AiChatMessage& message : messages) {
            message.conversationId = conversationId;
            LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_messages"),
                                                   message.messageId,
                                                   aiChatMessageToJson(message));
        }
        m_messages.insert(conversationId, messages);

        const AiChatContextUsage usage = m_contextUsages.take(previousConversationId);
        if (usage.available) {
            m_contextUsages.insert(conversationId, {conversationId, usage.usedTokens, usage.maxTokens, usage.available});
        }

        LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_entries"), previousConversationId);
        LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_entries"),
                                               conversationId,
                                               aiChatEntryToJson(entry));
    }

    if (hadUnreadDot) {
        emit unreadDotStateChanged();
    }
    return true;
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

bool AiChatRepository::setAiChatMessages(const QString& conversationId,
                                         const QVector<AiChatMessage>& messages)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    if (std::none_of(m_entries.cbegin(), m_entries.cend(), [&conversationId](const AiChatListEntry& entry) {
            return entry.conversationId == conversationId;
        })) {
        return false;
    }

    const QVector<AiChatMessage> previousMessages = m_messages.value(conversationId);
    for (const AiChatMessage& message : previousMessages) {
        LocalDataStore::instance().removeValue(QStringLiteral("ai_chat_messages"), message.messageId);
    }

    QVector<AiChatMessage> normalizedMessages;
    normalizedMessages.reserve(messages.size());
    for (AiChatMessage message : messages) {
        if (message.conversationId != conversationId ||
                message.messageId.isEmpty() ||
                !message.time.isValid() ||
                (!message.isFromUser && message.text.isNull())) {
            continue;
        }

        message.text = message.isFromUser
                ? normalizedMessageText(message.text)
                : normalizedAiMessageText(message.text);
        normalizedMessages.push_back(message);
        LocalDataStore::instance().upsertValue(QStringLiteral("ai_chat_messages"),
                                               message.messageId,
                                               aiChatMessageToJson(message));
    }

    m_messages.insert(conversationId, normalizedMessages);
    m_contextUsages.remove(conversationId);
    return true;
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
