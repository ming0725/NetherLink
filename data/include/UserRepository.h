#pragma once
#include "User.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>

class UserRepository : public QObject {
    Q_OBJECT

    public:
        static UserRepository& instance();

        User getUser(const QString& userID);

        QVector <User> getAllUser();

        void insertUser(const User& user);

        void removeUser(const QString& userID);

        QString getName(QString userID);

        QString getUserName(const QString& userID);

        // 加载当前用户和联系人信息
        bool loadUserAndContacts(const QJsonObject& userInfo, const QJsonArray& friends);

    private:
        explicit UserRepository(QObject* parent = nullptr);

        Q_DISABLE_COPY(UserRepository)

        // 确保数据库连接已打开
        bool ensureDatabaseOpen();

        // 更新当前用户信息
        bool updateCurrentUser(const QJsonObject& userInfo);

};
