#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class MessageLocalDataSource final
{
public:
    static MessageLocalDataSource& instance();

    static QString conversationsDomain();
    static QString messagesDomain();

    QVector<QJsonObject> conversations() const;
    QJsonObject conversation(const QString& conversationId) const;
    bool upsertConversation(const QString& conversationId, const QJsonObject& conversation);
    bool removeConversation(const QString& conversationId);

    QVector<QJsonObject> messages() const;
    bool hasMessagesForConversation(const QString& conversationId) const;
    bool upsertMessage(const QString& key, const QJsonObject& message);
    bool removeMessage(const QString& key);
    void removeMessagesForConversation(const QString& conversationId);

    QDateTime conversationClearTime(const QString& conversationId) const;
    bool hasMessageDeletionMarker(const QString& conversationId, const QStringList& messageIds) const;
    bool persistConversationClearMarker(const QString& conversationId, const QDateTime& clearedAt);
    void persistMessageDeletionMarkers(const QString& conversationId, const QStringList& messageIds);

private:
    MessageLocalDataSource() = default;
    Q_DISABLE_COPY(MessageLocalDataSource)
};
