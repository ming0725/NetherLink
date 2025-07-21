/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "DatabaseManager.h"
#include <QDir>

#include "CurrentUser.h"
#include <QSqlError>
#include <QStandardPaths>
#include <QThread>

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
    // 设置模板数据库路径
    m_templateDbPath = "client.db";
}

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager manager;

    return (manager);
}

QString DatabaseManager::getConnectionName(const QString& userId) const {
    return (QString("user_%1_thread_%2").arg(userId).arg(quintptr(QThread::currentThreadId())));
}

bool DatabaseManager::initUserDatabase(const QString& userId) {
    // 如果没有指定userId，则使用CurrentUser中的userId
    QString actualUserId = userId.isEmpty() ? CurrentUser::instance().getUserId() : userId;

    if (actualUserId.isEmpty()) {
        qWarning() << "无法初始化用户数据库：用户ID为空";

        return (false);
    }

    QString connectionName = QString("user_%1").arg(actualUserId);

    // 如果连接已存在且有效，直接返回
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);

        if (db.isValid() && db.isOpen()) {
            return (true);
        }

        // 如果连接存在但无效，移除它
        QSqlDatabase::removeDatabase(connectionName);
    }

    // 确保用户数据目录存在
    QString userDataDir = getUserDataDir(actualUserId);

    QDir().mkpath(userDataDir);

    // 用户数据库文件路径
    QString dbFilePath = userDataDir + "/user.db";

    // 如果数据库文件不存在，从资源中复制
    if (!QFile::exists(dbFilePath)) {
        // 从资源中读取模板数据库
        QFile resourceDb(":/client.db");

        if (!resourceDb.open(QIODevice::ReadOnly)) {
            qWarning() << "无法打开资源中的模板数据库";

            return (false);
        }

        // 创建用户数据库文件
        QFile userDb(dbFilePath);

        if (!userDb.open(QIODevice::WriteOnly)) {
            qWarning() << "无法创建用户数据库文件:" << dbFilePath;

            return (false);
        }

        // 复制数据
        userDb.write(resourceDb.readAll());
        resourceDb.close();
        userDb.close();

        // 确保新文件可写
        QFile dbFile(dbFilePath);

        if (!dbFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
            qWarning() << "无法设置数据库文件权限:" << dbFilePath;
        }
    }

    // 创建数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);

    db.setDatabaseName(dbFilePath);

    if (!db.open()) {
        qWarning() << "无法打开用户数据库:" << db.lastError().text();

        return (false);
    }
    return (true);
}

QSqlDatabase DatabaseManager::getUserDatabase(const QString& userId) {
    // 如果没有指定userId，则使用CurrentUser中的userId
    QString actualUserId = userId.isEmpty() ? CurrentUser::instance().getUserId() : userId;

    if (actualUserId.isEmpty()) {
        qWarning() << "无法获取用户数据库：用户ID为空";

        return (QSqlDatabase());
    }

    QString connectionName = QString("user_%1").arg(actualUserId);

    if (QSqlDatabase::contains(connectionName)) {
        return (QSqlDatabase::database(connectionName));
    }
    return (QSqlDatabase());
}

void DatabaseManager::closeUserDatabase(const QString& userId) {
    QString connectionName = getConnectionName(userId);

    if (QSqlDatabase::contains(connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(connectionName);

            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
}

void DatabaseManager::closeAllDatabases() {
    QStringList connectionNames = QSqlDatabase::connectionNames();

    for (const QString& connectionName : connectionNames) {
        if (connectionName.startsWith("user_")) {
            QSqlDatabase db = QSqlDatabase::database(connectionName);

            db.close();
            QSqlDatabase::removeDatabase(connectionName);
        }
    }
}

QString DatabaseManager::getUserDatabasePath(const QString& userId) const {
    return (m_userDataDir + "/" + userId + "/user.db");
}

bool DatabaseManager::createUserDataDir(const QString& userId) const {
    QString userDir = m_userDataDir + "/" + userId;

    return (QDir().mkpath(userDir));
}

bool DatabaseManager::copyTemplateDatabase(const QString& userId) const {
    // 检查模板数据库是否存在
    QFile templateFile(m_templateDbPath);

    if (!templateFile.exists()) {
        qWarning() << "Template database does not exist:" << m_templateDbPath;

        return (false);
    }

    // 复制模板数据库到用户目录
    QString targetPath = getUserDatabasePath(userId);

    return (QFile::copy(m_templateDbPath, targetPath));
}

QString DatabaseManager::getUserDataDir(const QString& userId) {
    return (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/users/" + userId);
}

QString DatabaseManager::getTemplateDbPath() {
    return (":/client.db");
}
