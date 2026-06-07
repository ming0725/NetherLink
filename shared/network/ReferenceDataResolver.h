#pragma once

#include "NetworkTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>

class ReferenceDataResolver final : public QObject
{
    Q_OBJECT

public:
    static ReferenceDataResolver& instance();

    void consumePayload(const QJsonObject& payload);
    void consumeIncluded(const QJsonObject& included);
    void consumeRefs(const QJsonObject& refs);
    void upsertUserObject(const QJsonObject& object);
    void upsertGroupMemberObject(const QJsonObject& object);

private:
    struct UserRefreshItem {
        QString userUuid;
        int knownVersion = 0;
    };

    struct GroupMemberRefreshItem {
        QString groupId;
        QString userUuid;
        int knownVersion = 0;
    };

    explicit ReferenceDataResolver(QObject* parent = nullptr);
    Q_DISABLE_COPY(ReferenceDataResolver)

    void scheduleUserRefresh(const QString& userUuid, int knownVersion);
    void scheduleGroupMemberRefresh(const QString& groupId, const QString& userUuid, int knownVersion);
    void flushUserRefreshQueue();
    void flushGroupMemberRefreshQueue();
    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QTimer m_userRefreshTimer;
    QTimer m_groupMemberRefreshTimer;
    QHash<QString, UserRefreshItem> m_pendingUsers;
    QHash<QString, GroupMemberRefreshItem> m_pendingGroupMembers;
    QSet<QString> m_inFlightUserIds;
    QSet<QString> m_inFlightGroupMemberKeys;
    QSet<QString> m_userBatchRequests;
    QHash<QString, QString> m_groupMemberBatchRequests;
};
