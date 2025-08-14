/**
 * @file Register.h
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-14 周四 08:52:14
 * @brief 【描述】 注册界面
 */

/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_REGISTER

#define INCLUDE_WINDOW_REGISTER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */
#include <QMainWindow>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class Register;
} // namespace Ui

namespace Window {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Register : public QMainWindow {
        Q_OBJECT

        public:
            explicit Register(QWidget* parent = nullptr);

            ~Register();

        signals:
            void registerSuccess(const QString& 邮箱, const QString& 密码);

        private:
            // QPixmap createRoundedPixmap(const QPixmap& source);

            void 函数_上传头像(const QString& filePath);

            void requestVerificationCode(const QString& 邮箱);

            bool 函数_检查输入框信息合法性(QString 昵称, QString 邮箱, QString 验证码, QString 密码, QString 确认密码);

            void 函数_注册(const QString& 昵称, const QString& 邮箱, const QString& 验证码, const QString& 密码);

        private:
            Ui::Register* ui;
            QString avatarUrl; // 保存上传后的头像URL
            QString 昵称;
            QString 邮箱;
            QString 验证码;
            QString 密码;
            QString 确认密码;
            bool 正在注册 = false;
    };
}

#endif /* INCLUDE_WINDOW_REGISTER */
