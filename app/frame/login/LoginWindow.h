#pragma once

#include "platform/SystemWindow.h"
#include "app/state/CurrentUserProfile.h"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVector>

class QLabel;
class LoginInputField;
class RegisterWindow;
class QAbstractButton;
class QCloseEvent;
class StatefulPushButton;
class QTimer;
struct LoginAccount;
template <typename T>
class QFutureWatcher;

class LoginWindow : public SystemWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);
    ~LoginWindow() override;

signals:
    void loginAccepted();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct BackgroundLight {
        QColor color;
        QPointF startPosition;
        QPointF currentPosition;
        QPointF targetPosition;
        qreal startRadius = 0.0;
        qreal currentRadius = 0.0;
        qreal targetRadius = 0.0;
        qreal startOpacity = 0.0;
        qreal currentOpacity = 0.0;
        qreal targetOpacity = 0.0;
        qreal progress = 0.0;
        qreal durationMs = 1.0;
        qreal rotation = 0.0;
        qreal rotationSpeed = 0.0;
    };

    void setupUi();
    void updateBackdropTheme();
    void updateAutoLoginRules();
    void updateAvatarForAccount(const QString& accountId);
    void attemptLogin();
    void cacheAuthenticatedAvatar(LoginAccount account, CurrentUserProfile authenticatedProfile);
    void finishLogin(const LoginAccount& account, const CurrentUserProfile& authenticatedProfile = {});
    void resetLoginPending(const QString& buttonText = QString());
    void showRegisterWindow();
    void closeRegisterWindow();
    void showAccountPopup();
    void centerOnPrimaryScreen();
    void setupBackgroundLights();
    void updateBackgroundLightColors();
    void advanceBackgroundLights();
    QPointF randomBackgroundPoint(qreal radius) const;
    void requestBackgroundLayerUpdate(bool force = false);

    QWidget* m_titleBar = nullptr;
    QWidget* m_avatarView = nullptr;
    LoginInputField* m_accountField = nullptr;
    LoginInputField* m_passwordField = nullptr;
    QWidget* m_accountPopup = nullptr;
    QPointer<RegisterWindow> m_registerWindow;
    StatefulPushButton* m_loginButton = nullptr;
    QLabel* m_errorLabel = nullptr;
    QAbstractButton* m_rememberButton = nullptr;
    QAbstractButton* m_autoLoginButton = nullptr;
    QTimer* m_backgroundTimer = nullptr;
    QElapsedTimer m_backgroundClock;
    qint64 m_lastBackgroundTick = 0;
    QVector<BackgroundLight> m_backgroundLights;
    QImage m_blurredBackgroundLayer;
    QString m_loginRequestId;
    QString m_pendingLoginAccountId;
    QString m_pendingLoginPassword;
    QFutureWatcher<QImage>* m_backgroundLayerWatcher = nullptr;
    qint64 m_lastBackgroundLayerRequest = -1000;
    int m_backgroundLayerGeneration = 0;
    bool m_backgroundLayerInFlight = false;
    bool m_backgroundLayerUpdatePending = false;
    bool m_loginPending = false;
};
