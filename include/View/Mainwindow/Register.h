/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_REGISTER
#define INCLUDE_VIEW_MAINWINDOW_REGISTER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */
#include <QPushButton>

#include "Components/CustomPushButton.h"
#include "Components/LineEditComponent.h"

#include "Utils/FramelessWindow.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class Register : public FramelessWindow {
    Q_OBJECT

    public:
        explicit Register(QWidget* parent = nullptr);

        ~Register() override;

    signals:
        void registerSuccess(const QString& 邮箱, const QString& 密码);

    protected:
        void paintEvent(QPaintEvent* event) override;

        bool eventFilter(QObject* target, QEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

    private:
        void doRegister(const QString& 邮箱, const QString& code, const QString& 密码, const QString& nickname);

        void requestVerificationCode(const QString& 邮箱);

        void uploadAvatar(const QString& filePath);

        QPixmap createRoundedPixmap(const QPixmap& source);

        bool 函数_是否为有效邮箱(const QString& 邮箱) const;

    private:
        static const QRegularExpression 邮箱正则表达式; // 静态邮箱验证正则表达式
        QLabel* closeButton;
        QLabel* userHead;
        QString avatarUrl; // 保存上传后的头像URL
        LineEditComponent* usernameEdit; // 邮箱输入框
        LineEditComponent* verificationCodeEdit;
        CustomPushButton* getCodeButton;
        LineEditComponent* nicknameEdit;  // 用户名输入框
        LineEditComponent* passwordEdit;
        LineEditComponent* confirmPasswordEdit;
        CustomPushButton* registerButton;
        bool isRegistering = false;
};

#endif /* INCLUDE_VIEW_MAINWINDOW_REGISTER */
