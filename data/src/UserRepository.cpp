
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "UserRepository.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include "CurrentUser.h"
#include "DatabaseManager.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

UserRepository::UserRepository(QObject* parent) : QObject(parent) {}

UserRepository& UserRepository::instance() {
    static UserRepository repo;

    return (repo);
}

bool UserRepository::ensureDatabaseOpen() {
    QString userId = CurrentUser::instance().getUserId();

    if (userId.isEmpty()) {
        qWarning() << "Current user ID is not set";

        return (false);
    }
    return (DatabaseManager::instance().initUserDatabase(userId));
}

User UserRepository::getUser(const QString& userID) {
    // QMutexLocker locker(&mutex);
    if (!ensureDatabaseOpen()) {
        return (User());
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);

    query.prepare("SELECT u.id, u.name, f.remark, u.avatar_url, f.status, u.signature ""FROM users u LEFT JOIN friends f ON u.uid = f.friend_id ""WHERE u.id = ?");
    query.addBindValue(userID);

    if (query.exec() && query.next()) {
        User user;

        user.id = query.value(0).toString();
        user.nick = query.value(1).toString();
        user.remark = query.value(2).toString();
        user.avatarPath = query.value(3).toString();
        user.status = static_cast <UserStatus>(query.value(4).toInt());
        user.signature = query.value(5).toString();

        return (user);
    } else {
        return (User());
    }
}

QVector <User> UserRepository::getAllUser() {
    // QMutexLocker locker(&mutex);
    QVector <User> users;

    if (!ensureDatabaseOpen()) {
        return (users);
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);

    if (query.exec("SELECT u.id, u.name, f.remark, u.avatar_url, f.status, u.signature ""FROM users u LEFT JOIN friends f ON u.uid = f.friend_id")) {
        while (query.next()) {
            User user;

            user.id = query.value(0).toString();
            user.nick = query.value(1).toString();
            user.remark = query.value(2).toString();
            user.avatarPath = query.value(3).toString();
            user.status = static_cast <UserStatus>(query.value(4).toInt());
            user.signature = query.value(5).toString();
            users.append(user);
        }
    } else {
        qWarning() << "Failed to get all users:" << query.lastError().text();
    }
    return (users);
}

void UserRepository::insertUser(const User& user) {
    if (!ensureDatabaseOpen()) {
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());

    db.transaction();

    // 首先插入或更新users表
    QSqlQuery userQuery(db);

    userQuery.prepare("INSERT OR REPLACE INTO users (uid, id, name, avatar_url, signature) ""VALUES (?, ?, ?, ?, ?)");
    userQuery.addBindValue(user.id); // 假设id同时也是uid
    userQuery.addBindValue(user.id);
    userQuery.addBindValue(user.nick);
    userQuery.addBindValue(user.avatarPath);
    userQuery.addBindValue(user.signature);

    bool success = userQuery.exec();

    // 然后插入或更新friends表
    if (success) {
        QSqlQuery friendQuery(db);

        friendQuery.prepare("INSERT OR REPLACE INTO friends (friend_id, status, remark, group_name) ""VALUES (?, ?, ?, ?)");
        friendQuery.addBindValue(user.id);
        friendQuery.addBindValue(static_cast <int>(user.status));
        friendQuery.addBindValue(user.remark);
        friendQuery.addBindValue(""); // group_name为空
        success = friendQuery.exec();
    }

    if (success) {
        db.commit();
    } else {
        db.rollback();
        qWarning() << "Failed to insert user:" << userQuery.lastError().text();
    }
}

void UserRepository::removeUser(const QString& userID) {
    // QMutexLocker locker(&mutex);
    if (!ensureDatabaseOpen()) {
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());
    QSqlQuery query(db);

    query.prepare("DELETE FROM friends WHERE friend_id = ?");
    query.addBindValue(userID);

    if (!query.exec()) {
        qWarning() << "Failed to remove user from friends:" << query.lastError().text();
    }
}

QString UserRepository::getName(QString userID) {
    // QMutexLocker locker(&mutex);
    User user = getUser(userID);

    return (user.nick);
}

QString UserRepository::getUserName(const QString& userID) {
    // 直接调用已有的getName方法
    return (getName(userID));
}

QString statusText(UserStatus userStatus) {
    if (userStatus == Online)
        return ("在线");


    if (userStatus == Offline)
        return ("离线");


    if (userStatus == Flying)
        return ("飞行模式");
    else
        return ("挖矿中");
}

QString statusIconPath(UserStatus userStatus) {
    QString prefix = ":/icon/";

    if (userStatus == Online)
        return (prefix + "online.png");


    if (userStatus == Offline)
        return (prefix + "offline.png");


    if (userStatus == Flying)
        return (prefix + "flying.png");
    else
        return (prefix + "mining.png");
}

bool UserRepository::loadUserAndContacts(const QJsonObject& userInfo, const QJsonArray& friends) {
    // QMutexLocker locker(&mutex);
    if (!ensureDatabaseOpen()) {
        return (false);
    }

    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());

    db.transaction();

    bool success = updateCurrentUser(userInfo);

    if (success) {
        // 更新好友信息
        for (const QJsonValue& friendValue : friends) {
            QJsonObject friendObj = friendValue.toObject();

            // 插入或更新users表
            QSqlQuery userQuery(db);

            userQuery.prepare("INSERT OR REPLACE INTO users (uid, id, name, avatar_url, signature) ""VALUES (?, ?, ?, ?, ?)");

            QString friendUid = friendObj["uid"].toString();

            userQuery.addBindValue(friendUid);
            userQuery.addBindValue(friendUid); // 使用uid作为id
            userQuery.addBindValue(friendObj["name"].toString());
            userQuery.addBindValue(friendObj["avatar_url"].toString());
            userQuery.addBindValue(friendObj["signature"].toString());
            success = userQuery.exec();

            if (!success)
                break;

            // 插入或更新friends表
            QSqlQuery friendQuery(db);

            friendQuery.prepare("INSERT OR REPLACE INTO friends (friend_id, status, remark, group_name) ""VALUES (?, ?, ?, ?)");
            friendQuery.addBindValue(friendUid);
            friendQuery.addBindValue(friendObj["status"].toInt());
            friendQuery.addBindValue(friendObj["remark"].toString());
            friendQuery.addBindValue(friendObj["group_name"].toString());
            success = friendQuery.exec();

            if (!success)
                break;
        }
    }

    if (success) {
        db.commit();

        return (true);
    } else {
        db.rollback();
        qWarning() << "Failed to load user and contacts:" << db.lastError().text();

        return (false);
    }
}

bool UserRepository::updateCurrentUser(const QJsonObject& userInfo) {
    QSqlDatabase db = DatabaseManager::instance().getUserDatabase(CurrentUser::instance().getUserId());

    // 更新当前用户信息
    QSqlQuery userQuery(db);

    userQuery.prepare("INSERT OR REPLACE INTO users (uid, id, name, avatar_url) ""VALUES (?, ?, ?, ?)");
    userQuery.addBindValue(CurrentUser::instance().getUserId());
    userQuery.addBindValue(CurrentUser::instance().getUserId()); // 使用uid作为id
    userQuery.addBindValue(userInfo["name"].toString());
    userQuery.addBindValue(userInfo["avatar_url"].toString());

    if (!userQuery.exec()) {
        qWarning() << "Failed to update current user:" << userQuery.lastError().text();

        return (false);
    }
    return (true);
}
