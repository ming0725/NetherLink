
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QFutureWatcher>
#include <QPushButton>

#include "FramelessWindow.h"
#include "LineEditComponent.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MainWindow;

class Login : public FramelessWindow {
    Q_OBJECT

    public:
        explicit Login(QWidget* parent = nullptr);

        ~Login() override;

        void doLogin(QString account, QString password);

        void loadContacts(const QString& uid, const QString& token);

    protected:
        void paintEvent(QPaintEvent* event) override;

        void getUserHeadBytes(const QPixmap& userhead);

        bool eventFilter(QObject* target, QEvent* event) override;

        void closeEvent(QCloseEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

    private slots:
        void onLoginSuccess(const QString& uid, const QString& token);

        void onContactsLoaded();

        void setLoginInfo(const QString& email, const QString& password);

    private:
        bool isValidEmail(const QString& email) const;

        static const QRegularExpression emailRegex; // 静态邮箱验证正则表达式
        QLabel* userHead = Q_NULLPTR;
        LineEditComponent* userAccountEdit = Q_NULLPTR;
        LineEditComponent* userPasswordEdit = Q_NULLPTR;
        QPushButton* loginButton = Q_NULLPTR;
        QPushButton* closeButton = Q_NULLPTR;
        QLabel* registerButton = Q_NULLPTR;
        bool isLogining = false;

        // 异步加载相关
        QFutureWatcher <void>* contactsWatcher = Q_NULLPTR;
        QFutureWatcher <void>* mainWindowWatcher = Q_NULLPTR;
        QWidget* mainWindow = Q_NULLPTR;
        QString currentUid;
        QString currentToken;

    signals:
        void startloginAccountSignal(const QString& userAccount, const QString& password);

        void VerifySucceed(const QPixmap& userhead_pixmap, const QByteArray& imagebytes, const QString& userName, const int& userAccount);

};
