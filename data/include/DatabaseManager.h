
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMutex>

#include <QSqlDatabase>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class DatabaseManager : public QObject {
    Q_OBJECT

    public:
        static DatabaseManager& instance();

        // 初始化用户数据库，如果不存在则从模板创建
        bool initUserDatabase(const QString& userId);

        // 获取用户数据库连接
        QSqlDatabase getUserDatabase(const QString& userId);

        // 关闭用户数据库连接
        void closeUserDatabase(const QString& userId);

        // 关闭所有数据库连接
        void closeAllDatabases();

        // 获取用户数据库文件路径
        QString getUserDatabasePath(const QString& userId) const;

    private:
        explicit DatabaseManager(QObject* parent = nullptr);

        Q_DISABLE_COPY(DatabaseManager)

        // 创建用户数据库目录
        bool createUserDataDir(const QString& userId) const;

        // 从模板复制数据库
        bool copyTemplateDatabase(const QString& userId) const;

        // 获取当前线程的连接名
        QString getConnectionName(const QString& userId) const;

        // 获取用户数据目录
        QString getUserDataDir(const QString& userId);

        // 获取模板数据库路径
        QString getTemplateDbPath();

        mutable QMutex m_mutex;
        QString m_templateDbPath;
        QString m_userDataDir;
};
