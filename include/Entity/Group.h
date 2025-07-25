/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_ENTITY_GROUP
#define INCLUDE_ENTITY_GROUP

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QVector>

/* struct ----------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// 群成员信息结构
struct GroupMember {
    QString uid;
    QString name;
    QString avatarUrl;
    QString nickname;
    QString role;  // owner, admin, member
};

struct Group {
    QString groupId;
    QString groupName;
    int memberNum;
    QString ownerId;
    QString avatarUrl;  // 改名以保持一致性
    bool isDnd = false;
    QString remark;
    QString announcement;
    QVector <GroupMember> members; // 使用新的成员结构

    // 便捷方法
    QVector <QString> getAdminIds() const {
        QVector <QString> adminIds;

        for (const auto& member : members) {
            if (member.role == "admin") {
                adminIds.append(member.uid);
            }
        }
        return (adminIds);
    }

    QVector <QString> getMemberIds() const {
        QVector <QString> memberIds;

        for (const auto& member : members) {
            memberIds.append(member.uid);
        }
        return (memberIds);
    }

};

#endif /* INCLUDE_ENTITY_GROUP */
