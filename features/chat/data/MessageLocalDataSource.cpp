#include "MessageLocalDataSource.h"

#include "shared/data/LocalDataStore.h"

#include <QJsonObject>
#include <QtGlobal>

namespace {

constexpr auto kConversationsDomain = "conversations";
constexpr auto kMessagesDomain = "chat_messages";
constexpr auto kMessageDeletionDomain = "chat_message_deletions";
constexpr auto kConversationClearDomain = "chat_conversation_clears";

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

QDateTime firstDateTime(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString text = object.value(key).toString();
        if (text.isEmpty()) {
            continue;
        }
        QDateTime value = QDateTime::fromString(text, Qt::ISODateWithMs);
        if (!value.isValid()) {
            value = QDateTime::fromString(text, Qt::ISODate);
        }
        if (value.isValid()) {
            return value;
        }
    }
    return {};
}

QString conversationIdFromMessageObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("conversationId"),
                                QStringLiteral("conversationID"),
                                QStringLiteral("chatId"),
                                QStringLiteral("roomId")});
}

QString messageIdFromObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("messageId"),
                                QStringLiteral("id"),
                                QStringLiteral("clientMessageId")});
}

QString messageCacheKey(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return {};
    }
    return conversationId + QLatin1Char(':') + messageId;
}

QString messageCacheKeyFromObject(const QJsonObject& object)
{
    return messageCacheKey(conversationIdFromMessageObject(object), messageIdFromObject(object));
}

QString messageDeletionKey(const QString& conversationId, const QString& messageId)
{
    return messageCacheKey(conversationId, messageId);
}

} // namespace

MessageLocalDataSource& MessageLocalDataSource::instance()
{
    static MessageLocalDataSource dataSource;
    return dataSource;
}

QString MessageLocalDataSource::conversationsDomain()
{
    return QString::fromLatin1(kConversationsDomain);
}

QString MessageLocalDataSource::messagesDomain()
{
    return QString::fromLatin1(kMessagesDomain);
}

QVector<QJsonObject> MessageLocalDataSource::conversations() const
{
    return LocalDataStore::instance().values(conversationsDomain());
}

QJsonObject MessageLocalDataSource::conversation(const QString& conversationId) const
{
    return LocalDataStore::instance().value(conversationsDomain(), conversationId);
}

bool MessageLocalDataSource::upsertConversation(const QString& conversationId,
                                                const QJsonObject& conversation)
{
    return LocalDataStore::instance().upsertValue(conversationsDomain(), conversationId, conversation);
}

bool MessageLocalDataSource::removeConversation(const QString& conversationId)
{
    return LocalDataStore::instance().removeValue(conversationsDomain(), conversationId);
}

QVector<QJsonObject> MessageLocalDataSource::messages() const
{
    return LocalDataStore::instance().values(messagesDomain());
}

bool MessageLocalDataSource::hasMessagesForConversation(const QString& conversationId) const
{
    if (conversationId.isEmpty()) {
        return false;
    }
    for (const QJsonObject& object : messages()) {
        if (conversationIdFromMessageObject(object) == conversationId) {
            return true;
        }
    }
    return false;
}

bool MessageLocalDataSource::upsertMessage(const QString& key, const QJsonObject& message)
{
    return LocalDataStore::instance().upsertValue(messagesDomain(), key, message);
}

bool MessageLocalDataSource::removeMessage(const QString& key)
{
    return LocalDataStore::instance().removeValue(messagesDomain(), key);
}

void MessageLocalDataSource::removeMessagesForConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    for (const QJsonObject& object : messages()) {
        if (conversationIdFromMessageObject(object) != conversationId) {
            continue;
        }

        const QString key = messageCacheKeyFromObject(object);
        if (!key.isEmpty()) {
            removeMessage(key);
        }
    }
}

QDateTime MessageLocalDataSource::conversationClearTime(const QString& conversationId) const
{
    if (conversationId.isEmpty()) {
        return {};
    }

    const QJsonObject marker = LocalDataStore::instance().value(
            QString::fromLatin1(kConversationClearDomain),
            conversationId);
    return firstDateTime(marker, {QStringLiteral("clearedAt"),
                                  QStringLiteral("deletedAt")});
}

int MessageLocalDataSource::conversationClearedThroughSeq(const QString& conversationId) const
{
    if (conversationId.isEmpty()) {
        return 0;
    }

    const QJsonObject marker = LocalDataStore::instance().value(
            QString::fromLatin1(kConversationClearDomain),
            conversationId);
    return qMax(0, marker.value(QStringLiteral("clearedThroughSeq")).toInt());
}

bool MessageLocalDataSource::hasMessageDeletionMarker(const QString& conversationId,
                                                      const QStringList& messageIds) const
{
    if (conversationId.isEmpty()) {
        return false;
    }

    for (const QString& messageId : messageIds) {
        const QString key = messageDeletionKey(conversationId, messageId);
        if (!key.isEmpty() &&
            !LocalDataStore::instance()
                    .value(QString::fromLatin1(kMessageDeletionDomain), key)
                    .isEmpty()) {
            return true;
        }
    }
    return false;
}

bool MessageLocalDataSource::persistConversationClearMarker(const QString& conversationId,
                                                            const QDateTime& clearedAt,
                                                            int clearedThroughSeq)
{
    if (conversationId.isEmpty() || !clearedAt.isValid()) {
        return false;
    }

    return LocalDataStore::instance().upsertValue(
            QString::fromLatin1(kConversationClearDomain),
            conversationId,
            {{QStringLiteral("conversationId"), conversationId},
             {QStringLiteral("clearedAt"), clearedAt.toUTC().toString(Qt::ISODateWithMs)},
             {QStringLiteral("clearedThroughSeq"), qMax(0, clearedThroughSeq)}});
}

void MessageLocalDataSource::persistMessageDeletionMarkers(const QString& conversationId,
                                                           const QStringList& messageIds)
{
    if (conversationId.isEmpty() || messageIds.isEmpty()) {
        return;
    }

    const QString deletedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for (const QString& messageId : messageIds) {
        const QString key = messageDeletionKey(conversationId, messageId);
        if (key.isEmpty()) {
            continue;
        }
        LocalDataStore::instance().upsertValue(QString::fromLatin1(kMessageDeletionDomain),
                                               key,
                                               {{QStringLiteral("conversationId"), conversationId},
                                                {QStringLiteral("messageId"), messageId},
                                                {QStringLiteral("deletedAt"), deletedAt}});
    }
}
