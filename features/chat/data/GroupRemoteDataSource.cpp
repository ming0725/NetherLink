#include "GroupRemoteDataSource.h"

#include "shared/network/HttpClient.h"

#include <QJsonObject>
#include <QUuid>

namespace {

QString newClientOperationId(const QString& prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject bodyWithClientOperationId(const QString& clientOperationId)
{
    return {
            {QStringLiteral("clientOperationId"), clientOperationId}
    };
}

QString normalizedListGroupId(const Group& group)
{
    return group.listGroupId.isEmpty() ? QStringLiteral("gg_joined") : group.listGroupId;
}

QString normalizedListGroupName(const Group& group)
{
    return group.listGroupName.isEmpty() ? QStringLiteral("我加入的群聊") : group.listGroupName;
}

} // namespace

GroupRemoteDataSource& GroupRemoteDataSource::instance()
{
    static GroupRemoteDataSource dataSource;
    return dataSource;
}

GroupRemoteDataSource::GroupRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &GroupRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &GroupRemoteDataSource::handleRequestFailed);
}

QString GroupRemoteDataSource::updateGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_update")));
    body.insert(QStringLiteral("name"), group.groupName);
    body.insert(QStringLiteral("introduction"), group.introduction);
    body.insert(QStringLiteral("announcement"), group.announcement);

    PendingOperation pending;
    pending.action = Action::UpdateGroup;
    pending.groupId = group.groupId;
    pending.group = group;
    return sendOperation(Action::UpdateGroup,
                         QStringLiteral("/groups/%1").arg(group.groupId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::updateMySettings(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return {};
    }

    QJsonObject body = bodyWithClientOperationId(newClientOperationId(QStringLiteral("op_group_settings")));
    body.insert(QStringLiteral("remark"), group.remark);
    body.insert(QStringLiteral("listGroupId"), normalizedListGroupId(group));
    body.insert(QStringLiteral("listGroupName"), normalizedListGroupName(group));
    body.insert(QStringLiteral("isDnd"), group.isDnd);

    PendingOperation pending;
    pending.action = Action::UpdateMySettings;
    pending.groupId = group.groupId;
    pending.group = group;
    return sendOperation(Action::UpdateMySettings,
                         QStringLiteral("/groups/%1/my-settings").arg(group.groupId),
                         body,
                         pending);
}

QString GroupRemoteDataSource::leaveGroup(const QString& groupId, const QString& currentUserUuid)
{
    if (groupId.isEmpty() || currentUserUuid.isEmpty()) {
        return {};
    }

    const QString clientOperationId = newClientOperationId(QStringLiteral("op_group_leave"));
    PendingOperation pending;
    pending.action = Action::LeaveGroup;
    pending.groupId = groupId;

    QVariantMap query;
    query.insert(QStringLiteral("clientOperationId"), clientOperationId);
    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Delete,
            QStringLiteral("/groups/%1/members/%2").arg(groupId, currentUserUuid),
            {},
            query);
    request.headers.insert("Idempotency-Key", clientOperationId.toUtf8());
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

QString GroupRemoteDataSource::sendOperation(Action action,
                                             const QString& path,
                                             const QJsonObject& body,
                                             PendingOperation pending,
                                             HttpMethod method,
                                             const QVariantMap& query)
{
    pending.action = action;
    NetworkRequest request = NetworkRequest::json(method, path, body, query);
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingOperations.insert(requestId, pending);
    return requestId;
}

void GroupRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse&)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::UpdateGroup:
        emit groupUpdated(requestId, pending.group);
        break;
    case Action::UpdateMySettings:
        emit groupMySettingsUpdated(requestId, pending.group);
        break;
    case Action::LeaveGroup:
        emit groupLeft(requestId, pending.groupId);
        break;
    }
}

void GroupRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingOperations.contains(requestId)) {
        return;
    }

    const PendingOperation pending = m_pendingOperations.take(requestId);
    switch (pending.action) {
    case Action::UpdateGroup:
        emit groupUpdateFailed(requestId, pending.groupId, error);
        break;
    case Action::UpdateMySettings:
        emit groupMySettingsUpdateFailed(requestId, pending.groupId, error);
        break;
    case Action::LeaveGroup:
        emit groupLeaveFailed(requestId, pending.groupId, error);
        break;
    }
}
