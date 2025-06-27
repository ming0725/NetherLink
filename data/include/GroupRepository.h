// GroupRepository.h
#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QSqlDatabase>
#include <QJsonArray>
#include "Group.h"
#include "ChatMessage.h"  // 为了使用 GroupRole 枚举

class GroupRepository : public QObject {
    Q_OBJECT
public:
    static GroupRepository& instance();

    // 获取单个群信息
    Group getGroup(const QString& groupID);
    // 获取所有群列表
    QVector<Group> getAllGroup();

    // 插入 / 删除 / 判断是否存在群
    void insertGroup(const Group& group);
    void removeGroup(const QString& groupID);
    bool isGroup(QString& id);

    // 批量加载群列表（JSON 数组形式），同步写入数据库并缓存
    bool loadGroups(const QJsonArray& groups);

    // 获取用户在群组中的身份
    GroupRole getMemberRole(int groupId, const QString& uid);

    bool loadGroupsAndMembers(const QJsonArray& groups);

private:
    explicit GroupRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRepository)

    // 确保已打开当前用户对应的数据库连接
    bool ensureDatabaseOpen();
};
