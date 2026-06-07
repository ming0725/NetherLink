#pragma once

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
    QString nickname;
    GroupMemberRoleValue role = GroupMemberRoleValue::Member;
    int version = 0;
};

struct Group {
    QString groupId;
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
    QString listGroupId = "gg_joined";
    QString listGroupName = "我加入的群聊";
};
