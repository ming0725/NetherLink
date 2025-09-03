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
#include <QLineEdit>

#include <QPushButton>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class Register;
} // namespace Ui

namespace Window {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Register : public QWidget {
        Q_OBJECT

        public:
            /**
             * @brief 构造函数
             *
             * @param parent 父界面
             */
            explicit Register(QWidget* parent = nullptr);

            /**
             * @brief 析构函数
             */
            ~Register();

        protected:
            void paintEvent(QPaintEvent* event) override;

            void mousePressEvent(QMouseEvent* 形参_鼠标事件)override;

            void mouseMoveEvent(QMouseEvent* 形参_鼠标事件)override;

        private:
            // QPixmap createRoundedPixmap(const QPixmap& source);

            void 函数_上传头像(const QString& filePath);

            void 函数_请求验证码(const QString& 形参_邮箱);

            void 函数_注册(const QString& 形参_昵称, const QString& 形参_邮箱, const QString& 形参_验证码, const QString& 形参_密码);

        private:
            Ui::Register* ui;
            QPushButton* 界面_关闭按钮;
            QPushButton* 界面_头像按钮;
            QLineEdit* 界面_昵称输入框;
            QLineEdit* 界面_邮箱输入框;
            QLineEdit* 界面_验证码输入框;
            QPushButton* 界面_验证码按钮;
            QLineEdit* 界面_密码输入框;
            QLineEdit* 界面_确认密码输入框;
            QPushButton* 界面_注册按钮;
            QPushButton* 界面_登录按钮;

        private:
            QPoint 成员变量_鼠标偏移量;
            QString avatarUrl; // 保存上传后的头像URL
            QString 成员变量_昵称;
            QString 成员变量_邮箱;
            QString 成员变量_验证码;
            QString 成员变量_密码;
            QString 成员变量_确认密码;
            bool 成员变量_正在注册 = false;

        signals:
            /**
             * @brief 注册成功信号
             *
             * @param 形参_邮箱 用户邮箱
             * @param 形参_密码 用户密码
             */
            void sig_registerSuccess(const QString& 形参_邮箱, const QString& 形参_密码);
    };
}

#endif /* INCLUDE_WINDOW_REGISTER */
