
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QObject>
#include <QString>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

// 当前用户信息的全局单例类
class CurrentUser : public QObject {
    Q_OBJECT

    public:
        static CurrentUser& instance();

        // 设置当前用户信息
        void setUserInfo(const QString& userId, const QString& token, const QString& userName = "", const QString& avatarPath = "");

        // 获取当前用户ID
        QString getUserId() const {
            return (m_userId);
        }

        // 获取当前用户名称
        QString getUserName() const {
            return (m_userName);
        }

        // 获取当前用户头像路径
        QString getAvatarPath() const {
            return (m_avatarPath);
        }

        // 获取当前用户token
        QString getToken() const {
            return (m_token);
        }

        // 检查是否已设置用户
        bool isUserSet() const {
            return (!m_userId.isEmpty());
        }

        // 清除当前用户信息
        void clear();

    signals:
        void userChanged(const QString& userId);

    private:
        explicit CurrentUser(QObject* parent = nullptr);

        Q_DISABLE_COPY(CurrentUser) QString m_userId;

        QString m_userName;
        QString m_avatarPath;
        QString m_token;
};
