/**
 * @file Login.h
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-11 周一 09:08:25
 * @brief 【描述】 登录界面
 */

/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_LOGIN

#define INCLUDE_WINDOW_LOGIN

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QFutureWatcher>

#include "Window/Register.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class Login;
} // namespace Ui

namespace Window {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Login : public QMainWindow {
        Q_OBJECT

        public:
            explicit Login(QWidget* parent = nullptr);

            ~Login();

            void 函数_设置登录信息(const QString& 邮箱, const QString& 密码);

        private:
            // void 函数_获取用户头像(const QPixmap& userhead);

            bool 函数_检查输入框信息合法性(QString 邮箱, QString 密码);

            void 函数_登录(QString 邮箱, QString 密码);

            void 函数_登录成功(const QString& uid, const QString& token);

            void loadContacts(const QString& uid, const QString& token);

            void onContactsLoaded();

        private:
            Ui::Login* ui;
            Register 界面_注册;
            QString 邮箱;
            QString 密码;
            QWidget* mainWindow = nullptr;
            QFutureWatcher <void>* contactsWatcher = nullptr;
            QString currentUid;
            QString currentToken;
            bool 正在登录 = false;
    };
} // namespace Window

#endif /* INCLUDE_WINDOW_LOGIN */
