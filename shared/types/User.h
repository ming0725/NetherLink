#pragma once
#include <QString>

enum UserStatus {
    Online,
    Offline,
    Mining,
    Flying
};

struct User {
    QString id;
    QString nick;
    QString remark;
    QString avatarPath;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    UserStatus status;
    QString signature;
    bool isDnd = false;
    bool isFriend = false;
    QString friendGroupId = "default";
    QString friendGroupName = "默认分组";
    QString region;
};

QString statusText(UserStatus userStatus);
QString statusIconPath(UserStatus userStatus);
