#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class MessageRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    struct FetchResult {
        bool completed = false;
        bool hasMoreAfter = false;
        QVector<QJsonObject> messages;
    };

    static MessageRemoteDataSource& instance();

    FetchResult fetchLatestMessagesBlocking(const QString& conversationId, int limit);
    FetchResult fetchOlderMessagesBlocking(const QString& conversationId, int beforeMessageSeq, int limit);
    FetchResult fetchNewerMessagesBlocking(const QString& conversationId, int afterMessageSeq, int limit);

private:
    explicit MessageRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(MessageRemoteDataSource)

    FetchResult fetchMessagesBlocking(const QString& conversationId,
                                      int beforeMessageSeq,
                                      int afterMessageSeq,
                                      int limit);
};
