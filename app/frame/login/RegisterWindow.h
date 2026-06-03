#pragma once

#include "platform/SystemWindow.h"

#include <QString>

class QLabel;
class QShowEvent;
class RegisterInputField;
class RegisterRequirementRow;
class StatefulPushButton;

class RegisterWindow : public SystemWindow
{
    Q_OBJECT

public:
    explicit RegisterWindow(QWidget* parent = nullptr);
    ~RegisterWindow() override;

signals:
    void accountRegistered(const QString& accountId, const QString& password);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void updateFieldGeometry();
    void updateBackdropTheme();
    void updateValidationState();
    void attemptRegister();
    void resetRegisterPending();
    void requestVerificationCode();
    void scheduleClose();
    void centerOnOwnerOrScreen();
    void setErrorText(const QString& text);

    bool emailValid() const;
    bool verificationCodeValid() const;
    bool nicknameValid() const;
    bool passwordLengthValid() const;
    bool passwordUpperValid() const;
    bool passwordDigitValid() const;
    bool passwordSymbolValid() const;
    bool repeatPasswordValid() const;

    QWidget* m_titleBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    RegisterInputField* m_emailField = nullptr;
    RegisterInputField* m_codeField = nullptr;
    RegisterInputField* m_nicknameField = nullptr;
    RegisterInputField* m_passwordField = nullptr;
    RegisterInputField* m_repeatPasswordField = nullptr;
    RegisterRequirementRow* m_emailRule = nullptr;
    RegisterRequirementRow* m_nicknameRule = nullptr;
    RegisterRequirementRow* m_passwordLengthRule = nullptr;
    RegisterRequirementRow* m_passwordUpperRule = nullptr;
    RegisterRequirementRow* m_passwordDigitRule = nullptr;
    RegisterRequirementRow* m_passwordSymbolRule = nullptr;
    RegisterRequirementRow* m_repeatRule = nullptr;
    QLabel* m_errorLabel = nullptr;
    StatefulPushButton* m_codeButton = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_registerButton = nullptr;
    QString m_registerRequestId;
    QString m_pendingAccountId;
    QString m_pendingPassword;
    QString m_pendingNickname;
    bool m_registerPending = false;
};
