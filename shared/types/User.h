#pragma once
#include <QDateTime>
#include <QString>

enum UserStatus {
    Online,
    Offline,
    Mining,
    Flying,
    Invisible
};

struct User {
    QString id;
    QString userUuid;
    QString userId;
    QString nick;
    QString remark;
    QString avatarPath;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    int version = 0;
    QString etag;
    UserStatus status = Offline;
    QDateTime lastSeenAt;
    QString signature;
    bool isDnd = false;
    bool isFriend = false;
    bool isAi = false;
    QString aiAgentId;
    QString aiKind;
    QString aiStatus;
    QString friendGroupId = "default";
    QString friendGroupName = "默认分组";
    QString region;
};

QString statusText(UserStatus userStatus);
QString statusIconPath(UserStatus userStatus);
