#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

struct GroupBotCreateRequest {
    QString groupId;
    QString name;
    QString avatarLocalPath;
    QString avatarFileId;
    QString basePrompt;
    QString model;
    QStringList allowedTools;
    QStringList allowedDomains;
    bool requireConfirmationForSideEffects = true;
    int cooldownSeconds = 0;
    QString clientOperationId;
};

struct GroupBotAgent {
    QString agentId;
    QString botUserId;
    QString ownerUserId;
    QString kind;
    QString visibility;
    QString name;
    QString avatarFileId;
    QString basePrompt;
    QString model;
    QString status;
    int version = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};
