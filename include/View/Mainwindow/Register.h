/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_REGISTER
#define INCLUDE_VIEW_MAINWINDOW_REGISTER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */
#include <QPushButton>

#include "Components/LineEditComponent.h"
#include "Components/CustomPushButton.h"
#include "Utils/FramelessWindow.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class Register : public FramelessWindow
{
    Q_OBJECT
public:
    explicit Register(QWidget* parent = nullptr);
    ~Register() override;
signals:
    void registerSuccess(const QString& email, const QString& password);
protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* target, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
private:
    void doRegister(const QString& email, const QString& code, const QString& password, const QString& nickname);
    void requestVerificationCode(const QString& email);
    void uploadAvatar(const QString& filePath);
    QPixmap createRoundedPixmap(const QPixmap& source);
    bool isValidEmail(const QString& email) const;
private:
    static const QRegularExpression emailRegex;  // 静态邮箱验证正则表达式
    QLabel* closeButton;
    QLabel* userHead;
    QString avatarUrl;  // 保存上传后的头像URL
    LineEditComponent* usernameEdit;      // 邮箱输入框
    LineEditComponent* verificationCodeEdit;
    CustomPushButton* getCodeButton;
    LineEditComponent* nicknameEdit;      // 用户名输入框
    LineEditComponent* passwordEdit;
    LineEditComponent* confirmPasswordEdit;
    CustomPushButton* registerButton;
    bool isRegistering = false;
};

#endif /* INCLUDE_VIEW_MAINWINDOW_REGISTER */
