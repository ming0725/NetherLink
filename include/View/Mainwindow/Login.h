/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_LOGIN
#define INCLUDE_VIEW_MAINWINDOW_LOGIN

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QFutureWatcher>
#include <QPushButton>

#include "Components/LineEditComponent.h"
#include "Components/CustomPushButton.h"
#include "Utils/FramelessWindow.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MainWindow;

class Login : public FramelessWindow {
    Q_OBJECT

    public:
        explicit Login(QWidget* parent = nullptr);

        ~Login() override;

        void setLoginInfo(const QString& email, const QString& password);

    protected:
        void paintEvent(QPaintEvent* event) override;

        bool eventFilter(QObject* target, QEvent* event) override;

        void closeEvent(QCloseEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

    private:
        void getUserHeadBytes(const QPixmap& userhead);

        void doLogin(QString account, QString password);

        void onLoginSuccess(const QString& uid, const QString& token);

        void loadContacts(const QString& uid, const QString& token);

        void onContactsLoaded();

        bool isValidEmail(const QString& email) const;

    private:
        static const QRegularExpression emailRegex;  // 静态邮箱验证正则表达式
        QLabel* closeButton;
        QLabel* userHead;
        LineEditComponent* userAccountEdit;
        LineEditComponent* userPasswordEdit;
        CustomPushButton* loginButton;
        QLabel* registerButton;
        QWidget* mainWindow = nullptr;
        QFutureWatcher<void>* contactsWatcher = nullptr;
        QString currentUid;
        QString currentToken;
        bool isLogining = false;
};

#endif /* INCLUDE_VIEW_MAINWINDOW_LOGIN */
