#pragma once

#include "User.h"

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVector>

enum class GroupMemberRoleValue {
    Member,
    Admin,
    Owner
};

struct GroupMemberProfile {
    QString groupId;
    QString userUuid;
    User user;
    QString nickname;
    GroupMemberRoleValue role = GroupMemberRoleValue::Member;
    bool isDnd = false;
    QDateTime joinedAt;
    int version = 0;
};

struct Group {
    QString groupId;
    QString groupPublicId;
    int version = 0;
    QString etag;
    QString groupName;
    int memberNum;
    QString ownerId;
    QString groupAvatarPath;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    bool isDnd = false;
    QVector<QString> adminsID;
    QString remark;
    QString introduction;
    QString announcement;
    QString currentUserNickname;
    QMap<QString, QString> memberNicknames;
    QVector<QString> membersID;
    QString listGroupId;
    QString listGroupName;
    QString role;
};
