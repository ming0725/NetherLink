#include "CurrentUser.h"
#include "DatabaseManager.h"
#include "GroupRepository.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

GroupRepository::GroupRepository(QObject* parent) : QObject(parent) {}

GroupRepository& GroupRepository::instance() {
    static GroupRepository repo;

    return (repo);
}

bool GroupRepository::ensureDatabaseOpen() {
    QString userId = CurrentUser::instance().getUserId();

    if (userId.isEmpty()) {
        qWarning() << "Current user ID is not set";

        return (false);
    }
    return (DatabaseManager::instance().initUserDatabase(userId));
}

Group GroupRepository::getGroup(const QString& groupID) {
    if (!ensureDatabaseOpen()) {
        return (Group());
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());
    QSqlQuery query(db);

    // 获取群组基本信息
    query.prepare("SELECT cg.gid, cg.name, ""(SELECT COUNT(*) FROM group_members WHERE gid = cg.gid) as member_count, ""cg.owner_id, cg.avatar_url, cg.remark, cg.announcement ""FROM chat_groups cg ""WHERE cg.gid = ?");
    query.addBindValue(groupID);

    if (query.exec() && query.next()) {
        Group group;

        group.groupId = query.value("gid").toString();
        group.groupName = query.value("name").toString();
        group.memberNum = query.value("member_count").toInt();
        group.ownerId = query.value("owner_id").toString();
        group.avatarUrl = query.value("avatar_url").toString();
        group.remark = query.value("remark").toString();
        group.announcement = query.value("announcement").toString();

        // 获取所有成员信息
        QSqlQuery memberQuery(db);

        memberQuery.prepare("SELECT uid, name, avatar_url, nickname, role ""FROM group_members ""WHERE gid = ?");
        memberQuery.addBindValue(groupID);

        if (memberQuery.exec()) {
            while (memberQuery.next()) {
                GroupMember member;

                member.uid = memberQuery.value("uid").toString();
                member.name = memberQuery.value("name").toString();
                member.avatarUrl = memberQuery.value("avatar_url").toString();
                member.nickname = memberQuery.value("nickname").toString();
                member.role = memberQuery.value("role").toString();
                group.members.append(member);
            }
        } else {
            qWarning() << "Failed to get group members:" << memberQuery.lastError().text();
        }
        return (group);
    } else {
        qWarning() << "Failed to get group:" << query.lastError().text();

        return (Group());
    }
}

QVector <Group> GroupRepository::getAllGroup() {
    QVector <Group> groups;

    if (!ensureDatabaseOpen()) {
        return (groups);
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());
    QSqlQuery query(db);

    // 修改SQL查询以匹配表结构
    if (query.exec("SELECT cg.gid, cg.name, ""(SELECT COUNT(*) FROM group_members WHERE gid = cg.gid) as member_count, ""cg.owner_id, cg.avatar_url, cg.remark, cg.announcement ""FROM chat_groups cg")) {
        while (query.next()) {
            Group group;

            group.groupId = query.value("gid").toString();
            group.groupName = query.value("name").toString();
            group.memberNum = query.value("member_count").toInt();
            group.ownerId = query.value("owner_id").toString();
            group.avatarUrl = query.value("avatar_url").toString();
            group.remark = query.value("remark").toString();
            groups.append(group);
        }
    } else {
        qWarning() << "Failed to get all groups:" << query.lastError().text();
    }
    return (groups);
}

void GroupRepository::insertGroup(const Group& group) {
    if (!ensureDatabaseOpen()) {
        return;
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());

    db.transaction();

    // 插入或更新群组基本信息
    QSqlQuery groupQuery(db);

    groupQuery.prepare("INSERT OR REPLACE INTO chat_groups ""(gid, name, owner_id, avatar_url, remark) ""VALUES (?, ?, ?, ?, ?)");
    groupQuery.addBindValue(group.groupId);
    groupQuery.addBindValue(group.groupName);
    groupQuery.addBindValue(group.ownerId);
    groupQuery.addBindValue(group.avatarUrl);
    groupQuery.addBindValue(group.remark);

    bool success = groupQuery.exec();

    // 插入成员
    if (success && !group.members.isEmpty()) {
        QSqlQuery memberQuery(db);

        memberQuery.prepare("INSERT OR REPLACE INTO group_members ""(gid, uid, name, avatar_url, nickname, role) ""VALUES (?, ?, ?, ?, ?, ?)");

        for (const GroupMember& member : group.members) {
            memberQuery.addBindValue(group.groupId);
            memberQuery.addBindValue(member.uid);
            memberQuery.addBindValue(member.name);
            memberQuery.addBindValue(member.avatarUrl);
            memberQuery.addBindValue(member.nickname);
            memberQuery.addBindValue(member.role);
            success = memberQuery.exec();

            if (!success)
                break;
        }
    }

    if (success) {
        db.commit();
    } else {
        db.rollback();
        qWarning() << "Failed to insert group:" << groupQuery.lastError().text();
    }
}

void GroupRepository::removeGroup(const QString& groupID) {
    // QMutexLocker locker(&mutex);
    if (!ensureDatabaseOpen()) {
        return;
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());

    db.transaction();

    // 删除群组相关的所有记录
    bool success = true;

    // 删除成员记录
    QSqlQuery deleteMembersQuery(db);

    deleteMembersQuery.prepare("DELETE FROM group_members WHERE gid = ?");
    deleteMembersQuery.addBindValue(groupID);
    success = deleteMembersQuery.exec();

    // 删除群组基本信息
    if (success) {
        QSqlQuery deleteGroupQuery(db);

        deleteGroupQuery.prepare("DELETE FROM chat_groups WHERE gid = ?");
        deleteGroupQuery.addBindValue(groupID);
        success = deleteGroupQuery.exec();
    }

    if (success) {
        db.commit();
    } else {
        db.rollback();
        qWarning() << "Failed to remove group:" << groupID;
    }
}

bool GroupRepository::isGroup(QString &id) {
    // QMutexLocker locker(&mutex);
    if (!ensureDatabaseOpen()) {
        return (false);
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) FROM chat_groups WHERE gid = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        return (query.value(0).toInt() > 0);
    }
    return (false);
}

bool GroupRepository::loadGroups(const QJsonArray& groups) {
    if (!ensureDatabaseOpen()) {
        return (false);
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());

    db.transaction();

    bool success = true;

    for (const QJsonValue& groupVal : groups) {
        QJsonObject groupObj = groupVal.toObject();

        // 准备群组基本信息
        QSqlQuery groupQuery(db);

        groupQuery.prepare("INSERT OR REPLACE INTO chat_groups ""(gid, name, owner_id, avatar_url, remark) ""VALUES (?, ?, ?, ?, ?)");

        QString gid = QString::number(groupObj["gid"].toInt());

        groupQuery.addBindValue(gid);
        groupQuery.addBindValue(groupObj["name"].toString());
        groupQuery.addBindValue(groupObj["owner_id"].toString());
        groupQuery.addBindValue(groupObj["avatar_url"].toString());
        groupQuery.addBindValue(""); // remark暂时为空

        if (!groupQuery.exec()) {
            qWarning() << "Failed to insert group:" << groupQuery.lastError().text();
            success = false;
            break;
        }

        // 处理群成员信息
        QJsonArray members = groupObj["members"].toArray();

        for (const QJsonValue& memberVal : members) {
            QJsonObject memberObj = memberVal.toObject();
            QSqlQuery memberQuery(db);

            memberQuery.prepare("INSERT OR REPLACE INTO group_members ""(gid, uid, name, avatar_url, nickname, role) ""VALUES (?, ?, ?, ?, ?, ?)");
            memberQuery.addBindValue(gid);
            memberQuery.addBindValue(memberObj["uid"].toString());
            memberQuery.addBindValue(memberObj["name"].toString());
            memberQuery.addBindValue(memberObj["avatar_url"].toString());
            memberQuery.addBindValue(memberObj["nickname"].toString());
            memberQuery.addBindValue(memberObj["role"].toString());

            if (!memberQuery.exec()) {
                qWarning() << "Failed to insert group member:" << memberQuery.lastError().text();
                success = false;
                break;
            }
        }

        if (!success)
            break;
    }

    if (success) {
        db.commit();

        return (true);
    } else {
        db.rollback();

        return (false);
    }
}

GroupRole GroupRepository::getMemberRole(int groupId, const QString& uid) {
    if (!ensureDatabaseOpen()) {
        return (GroupRole::Member); // 数据库错误时返回默认身份
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);

    query.prepare("SELECT role FROM group_members WHERE gid = ? AND uid = ?");
    query.addBindValue(groupId);
    query.addBindValue(uid);

    if (query.exec() && query.next()) {
        QString role = query.value(0).toString();

        if (role == "owner") {
            return (GroupRole::Owner);
        } else if (role == "admin") {
            return (GroupRole::Admin);
        }
    } else {
        qWarning() << "Failed to search group member role:" << query.lastError().text();
    }
    return (GroupRole::Member); // 如果没有找到记录或是普通成员，返回Member
}

bool GroupRepository::loadGroupsAndMembers(const QJsonArray& groups) {
    if (!ensureDatabaseOpen()) {
        return (false);
    }

    auto& cu = CurrentUser::instance();
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(cu.getUserId());

    db.transaction();

    bool success = true;

    for (const QJsonValue& groupVal : groups) {
        QJsonObject groupObj = groupVal.toObject();

        // 创建群组对象
        Group group;

        group.groupId = QString::number(groupObj["gid"].toInt());
        group.groupName = groupObj["name"].toString();
        group.ownerId = groupObj["owner_id"].toString();
        group.avatarUrl = groupObj["avatar_url"].toString();
        group.remark = groupObj["nickname"].toString();  // 使用nickname作为remark

        // 处理群成员
        QJsonArray membersArray = groupObj["members"].toArray();

        for (const QJsonValue& memberVal : membersArray) {
            QJsonObject memberObj = memberVal.toObject();
            GroupMember member;

            member.uid = memberObj["uid"].toString();
            member.name = memberObj["name"].toString();
            member.avatarUrl = memberObj["avatar_url"].toString();
            member.nickname = memberObj["nickname"].toString();
            member.role = memberObj["role"].toString();
            group.members.append(member);
        }

        // 设置成员数量
        group.memberNum = group.members.size();

        // 插入群组和成员数据
        insertGroup(group);
    }

    if (success) {
        db.commit();

        return (true);
    } else {
        db.rollback();

        return (false);
    }
}