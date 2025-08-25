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

#include <QLabel>

#include "Window/Register.hpp"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class Login;
} // namespace Ui

namespace Window {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Login : public QWidget {
        Q_OBJECT

        public:
            explicit Login(QWidget* parent = nullptr);

            ~Login();

            void 函数_设置登录信息(const QString& 形参_邮箱, const QString& 形参_密码);

        protected:
            void paintEvent(QPaintEvent* event) override;

            void mousePressEvent(QMouseEvent* 形参_鼠标事件)override;

            void mouseMoveEvent(QMouseEvent* 形参_鼠标事件)override;

        private:
            void 函数_获取用户头像(const QPixmap& userhead);

            void 函数_登录(QString 形参_邮箱, QString 形参_密码);

            void 函数_登录成功(const QString& uid, const QString& token);

            void loadContacts(const QString& uid, const QString& token);

            void onContactsLoaded();

        private:
            Ui::Login* ui;
            QPushButton* 界面_关闭按钮;
            QLabel* 界面_头像标签;
            QLineEdit* 界面_邮箱输入框;
            QLineEdit* 界面_密码输入框;
            QPushButton* 界面_登录按钮;
            QPushButton* 界面_注册按钮;
            Register 界面_注册界面;
            QWidget* mainWindow = nullptr;

        private:
            QPoint 成员变量_鼠标偏移量;
            QString 成员变量_邮箱;
            QString 成员变量_密码;
            QFutureWatcher <void>* contactsWatcher = nullptr;
            QString currentUid;
            QString currentToken;
            bool 成员变量_正在登录 = false;
    };
} // namespace Window

#endif /* INCLUDE_WINDOW_LOGIN */
