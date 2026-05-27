#include "RegisterWindow.h"

#include "LoginAccountRepository.h"
#ifdef Q_OS_WIN
#include "platform/windows/WindowsWindowControlButton.h"
#endif
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/StatefulPushButton.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>

constexpr int kWindowWidth = 460;
constexpr int kWindowHeight = 660;
constexpr int kWindowFrameInset = 1;
constexpr int kTitleBarHeight = 38;
constexpr int kContentTopSpacing = 14;
constexpr int kFormWidth = 328;
constexpr int kTitleLabelHeight = 32;
constexpr int kTitleToFieldSpacing = 22;
constexpr int kCodeButtonWidth = 104;
constexpr int kCancelButtonWidth = 104;
constexpr int kActionButtonHeight = 44;
constexpr int kInlineButtonSpacing = 10;
constexpr int kFieldHeight = 42;
constexpr int kFieldRadius = 10;
constexpr int kFieldVerticalSpacing = 12;
constexpr int kRequirementHeight = 21;
constexpr int kPasswordFieldToRulesSpacing = 4;
constexpr int kPasswordRulesToRepeatSpacing = 2;
constexpr int kRepeatRuleBottomSpacing = 4;
constexpr int kErrorLabelHeight = 19;

QColor registerBackgroundColor()
{
    return ThemeManager::instance().isDark() ? QColor(Qt::black) : QColor(Qt::white);
}

QColor successTextColor()
{
    return ThemeManager::instance().isDark() ? QColor(0x8f, 0xe0, 0xa5) : QColor(0x18, 0x8d, 0x48);
}

QColor disabledRuleColor()
{
    return ThemeManager::instance().color(ThemeColor::TertiaryText);
}

void applyDefaultRegisterButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }

    button->setDefaultStyle();
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

bool containsMatch(const QString& text, const QString& pattern)
{
    return QRegularExpression(pattern).match(text).hasMatch();
}

class RegisterInputField final : public QWidget
{
public:
    explicit RegisterInputField(const QString& placeholder, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_lineEdit(new QLineEdit(this))
    {
        setFixedSize(kFormWidth, kFieldHeight);
        setMouseTracking(true);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);

        m_lineEdit->setFrame(false);
        m_lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
        m_lineEdit->setAutoFillBackground(false);
        m_lineEdit->setFont(AppFonts::applicationPixelSizedFont(15));
        m_lineEdit->setPlaceholderText(placeholder);
        m_lineEdit->setTextMargins(16, 0, 16, 0);
        m_lineEdit->installEventFilter(this);
        updatePalette();
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            updatePalette();
            update();
        });
    }

    void setPasswordMode(bool enabled)
    {
        m_lineEdit->setEchoMode(enabled ? QLineEdit::Password : QLineEdit::Normal);
    }

    void setMaxLength(int length)
    {
        m_lineEdit->setMaxLength(length);
    }

    QString text() const
    {
        return m_lineEdit->text();
    }

    void setText(const QString& text)
    {
        m_lineEdit->setText(text);
    }

    QLineEdit* lineEdit() const
    {
        return m_lineEdit;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool focused = m_lineEdit->hasFocus();
        const QColor fill = focused
                ? ThemeManager::instance().color(ThemeColor::InputFocusBackground)
                : ThemeManager::instance().color(ThemeColor::InputBackground);
        const QColor border = focused
                ? ThemeManager::instance().color(ThemeColor::Accent)
                : (m_hovered ? ThemeManager::instance().color(ThemeColor::TertiaryText)
                             : ThemeManager::instance().color(ThemeColor::Divider));

        painter.setPen(QPen(border, focused ? 1.4 : 1.0));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                kFieldRadius,
                                kFieldRadius);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        m_lineEdit->setGeometry(0, 1, width(), qMax(0, height() - 2));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_lineEdit->setFocus(Qt::MouseFocusReason);
        }
        QWidget::mousePressEvent(event);
    }

    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_lineEdit &&
            (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
            update();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void updatePalette()
    {
        QPalette palette = m_lineEdit->palette();
        palette.setColor(QPalette::Base, Qt::transparent);
        palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
        palette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
        palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::AccentTextSelection));
        palette.setColor(QPalette::HighlightedText,
                         ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent));
        m_lineEdit->setPalette(palette);
    }

    QLineEdit* m_lineEdit = nullptr;
    bool m_hovered = false;
};

class RegisterRequirementRow final : public QWidget
{
public:
    explicit RegisterRequirementRow(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_text(text)
    {
        setFixedSize(kFormWidth, kRequirementHeight);
        setAttribute(Qt::WA_StyledBackground, false);
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            update();
        });
    }

    void setPassed(bool passed)
    {
        if (m_passed == passed) {
            return;
        }
        m_passed = passed;
        update();
    }

    void setErrorOnly(bool errorOnly)
    {
        if (m_errorOnly == errorOnly) {
            return;
        }
        m_errorOnly = errorOnly;
        update();
    }

    void setContentVisible(bool visible)
    {
        if (m_contentVisible == visible) {
            return;
        }
        m_contentVisible = visible;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        if (!m_contentVisible) {
            return;
        }

        const QColor color = m_errorOnly
                ? ThemeManager::instance().color(ThemeColor::DangerText)
                : (m_passed ? successTextColor() : disabledRuleColor());
        painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        const QRect iconRect(1, 4, 13, 13);
        if (m_errorOnly) {
            painter.drawLine(QPointF(iconRect.left() + 3.5, iconRect.top() + 3.5),
                             QPointF(iconRect.right() - 3.5, iconRect.bottom() - 3.5));
            painter.drawLine(QPointF(iconRect.right() - 3.5, iconRect.top() + 3.5),
                             QPointF(iconRect.left() + 3.5, iconRect.bottom() - 3.5));
        } else if (m_passed) {
            painter.drawLine(QPointF(iconRect.left() + 2.5, iconRect.center().y() + 0.5),
                             QPointF(iconRect.left() + 5.6, iconRect.bottom() - 2.5));
            painter.drawLine(QPointF(iconRect.left() + 5.6, iconRect.bottom() - 2.5),
                             QPointF(iconRect.right() - 1.5, iconRect.top() + 2.5));
        } else {
            painter.drawEllipse(QRectF(iconRect).adjusted(2.0, 2.0, -2.0, -2.0));
        }

        painter.setFont(AppFonts::applicationPixelSizedFont(12));
        painter.setPen(color);
        painter.drawText(rect().adjusted(20, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_text);
    }

private:
    QString m_text;
    bool m_passed = false;
    bool m_errorOnly = false;
    bool m_contentVisible = true;
};

class RegisterWindowControlButton final : public QAbstractButton
{
public:
    enum class Kind {
        Minimize,
        Close
    };

    explicit RegisterWindowControlButton(Kind kind, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_kind(kind)
    {
        setFixedSize(38, 32);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, false);
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        m_pressed = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
        QAbstractButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        m_pressed = false;
        update();
        QAbstractButton::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (m_hovered) {
            QColor fill = m_kind == Kind::Close
                    ? ThemeManager::instance().color(ThemeColor::WindowCloseHover)
                    : ThemeManager::instance().color(m_pressed ? ThemeColor::ControlPressed
                                                               : ThemeColor::ControlHover);
            if (m_pressed && m_kind == Kind::Close) {
                fill = fill.darker(112);
            }
            painter.fillRect(rect(), fill);
        }

        QPen pen(m_kind == Kind::Close && m_hovered
                 ? QColor(255, 255, 255)
                 : ThemeManager::instance().color(ThemeColor::SecondaryText),
                 1.5,
                 Qt::SolidLine,
                 Qt::RoundCap,
                 Qt::RoundJoin);
        painter.setPen(pen);
        const QPointF center = rect().center();
        if (m_kind == Kind::Minimize) {
            painter.drawLine(QPointF(center.x() - 5.0, center.y() + 3.0),
                             QPointF(center.x() + 5.0, center.y() + 3.0));
        } else {
            painter.drawLine(QPointF(center.x() - 4.5, center.y() - 4.5),
                             QPointF(center.x() + 4.5, center.y() + 4.5));
            painter.drawLine(QPointF(center.x() + 4.5, center.y() - 4.5),
                             QPointF(center.x() - 4.5, center.y() + 4.5));
        }
    }

private:
    Kind m_kind;
    bool m_hovered = false;
    bool m_pressed = false;
};

QLabel* makeRegisterLabel(const QString& text, int pixelSize, ThemeColor color, QWidget* parent, int weight)
{
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setFont(AppFonts::applicationPixelWeightedFont(pixelSize, weight));
    label->setAutoFillBackground(false);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, ThemeManager::instance().color(color));
    label->setPalette(palette);
    return label;
}

void updateRegisterLabelColor(QLabel* label, ThemeColor color)
{
    if (!label) {
        return;
    }

    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, ThemeManager::instance().color(color));
    label->setPalette(palette);
}

RegisterWindow::RegisterWindow(QWidget* parent)
    : SystemWindow(parent)
{
    setWindowFlag(Qt::Window, true);
    setFixedSize(kWindowWidth, kWindowHeight);
    setWindowTitle(QStringLiteral("注册NetherLink账号"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCompactTrafficLightsEnabled(true);
    updateBackdropTheme();
    setupUi();
    qApp->installEventFilter(this);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateBackdropTheme();
        updateRegisterLabelColor(m_titleLabel, ThemeColor::PrimaryText);
        updateRegisterLabelColor(m_errorLabel, ThemeColor::DangerText);
        applyDefaultRegisterButtonStyle(m_codeButton);
        applyDefaultRegisterButtonStyle(m_cancelButton);
        if (m_registerButton) {
            m_registerButton->setPrimaryStyle();
        }
        update();
    });
}

RegisterWindow::~RegisterWindow()
{
    qApp->removeEventFilter(this);
}

void RegisterWindow::setupUi()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setGeometry(kWindowFrameInset,
                            kWindowFrameInset,
                            width() - kWindowFrameInset * 2,
                            kTitleBarHeight);
    m_titleBar->setAttribute(Qt::WA_StyledBackground, false);
    setDragTitleBar(m_titleBar);

#ifdef Q_OS_WIN
    auto* minimizeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Minimize, m_titleBar);
    auto* closeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Close, m_titleBar);
    closeButton->setGeometry(m_titleBar->width() - closeButton->width(), 0, closeButton->width(), closeButton->height());
    minimizeButton->setGeometry(closeButton->x() - minimizeButton->width(), 0,
                                minimizeButton->width(), minimizeButton->height());
    minimizeButton->raise();
    closeButton->raise();
    connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QAbstractButton::clicked, this, &RegisterWindow::scheduleClose);
#elif !defined(Q_OS_MACOS)
    auto* minimizeButton = new RegisterWindowControlButton(RegisterWindowControlButton::Kind::Minimize, m_titleBar);
    auto* closeButton = new RegisterWindowControlButton(RegisterWindowControlButton::Kind::Close, m_titleBar);
    closeButton->setGeometry(m_titleBar->width() - closeButton->width(), 0, closeButton->width(), closeButton->height());
    minimizeButton->setGeometry(closeButton->x() - minimizeButton->width(), 0,
                                minimizeButton->width(), minimizeButton->height());
    connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QAbstractButton::clicked, this, &RegisterWindow::scheduleClose);
#endif

    auto* form = new QWidget(this);
    const int formLeft = (width() - kFormWidth) / 2;
    const int formTop = kWindowFrameInset + kTitleBarHeight + kContentTopSpacing;
    form->setGeometry(formLeft, formTop, kFormWidth, height() - formTop - 18);
    form->setAttribute(Qt::WA_StyledBackground, false);

    m_titleLabel = makeRegisterLabel(QStringLiteral("注册NetherLink账号"),
                                     24,
                                     ThemeColor::PrimaryText,
                                     form,
                                     QFont::DemiBold);
    m_titleLabel->setGeometry(0, 0, kFormWidth, kTitleLabelHeight);

    m_emailField = new RegisterInputField(QStringLiteral("邮箱"), form);
    m_emailField->setMaxLength(80);
    m_emailRule = new RegisterRequirementRow(QStringLiteral("邮箱格式错误"), form);
    m_emailRule->setErrorOnly(true);
    m_emailRule->setContentVisible(false);
    m_emailRule->hide();

    m_codeField = new RegisterInputField(QStringLiteral("验证码"), form);
    m_codeField->setMaxLength(6);
    m_codeField->setFixedSize(kFormWidth - kCodeButtonWidth - kInlineButtonSpacing, kFieldHeight);
    m_codeButton = new StatefulPushButton(QStringLiteral("获取验证码"), form);
    m_codeButton->setFixedSize(kCodeButtonWidth, kFieldHeight);
    m_codeButton->setRadius(kFieldRadius);
    m_codeButton->setFont(AppFonts::applicationPixelWeightedFont(13, QFont::DemiBold));
    applyDefaultRegisterButtonStyle(m_codeButton);

    m_nicknameField = new RegisterInputField(QStringLiteral("昵称"), form);
    m_nicknameField->setMaxLength(24);
    m_nicknameRule = new RegisterRequirementRow(QStringLiteral("昵称 2-24 个字符"), form);
    m_nicknameRule->setErrorOnly(true);
    m_nicknameRule->setContentVisible(false);
    m_nicknameRule->hide();

    m_passwordField = new RegisterInputField(QStringLiteral("密码"), form);
    m_passwordField->setMaxLength(32);
    m_passwordField->setPasswordMode(true);
    m_passwordLengthRule = new RegisterRequirementRow(QStringLiteral("密码 8-32 个字符"), form);
    m_passwordUpperRule = new RegisterRequirementRow(QStringLiteral("至少包含 1 个大写字母"), form);
    m_passwordDigitRule = new RegisterRequirementRow(QStringLiteral("至少包含 1 个数字"), form);
    m_passwordSymbolRule = new RegisterRequirementRow(QStringLiteral("至少包含 1 个符号"), form);

    m_repeatPasswordField = new RegisterInputField(QStringLiteral("重复密码"), form);
    m_repeatPasswordField->setMaxLength(32);
    m_repeatPasswordField->setPasswordMode(true);
    m_repeatRule = new RegisterRequirementRow(QStringLiteral("两次输入的密码一致"), form);

    m_errorLabel = makeRegisterLabel(QString(), 12, ThemeColor::DangerText, form, QFont::Normal);
    m_errorLabel->setFixedHeight(kErrorLabelHeight);

    m_cancelButton = new StatefulPushButton(QStringLiteral("取消"), form);
    m_cancelButton->setFixedSize(kCancelButtonWidth, kActionButtonHeight);
    m_cancelButton->setRadius(10);
    m_cancelButton->setFont(AppFonts::applicationPixelWeightedFont(15, QFont::DemiBold));
    applyDefaultRegisterButtonStyle(m_cancelButton);

    m_registerButton = new StatefulPushButton(QStringLiteral("完成注册"), form);
    m_registerButton->setFixedSize(kFormWidth - kCancelButtonWidth - kInlineButtonSpacing, kActionButtonHeight);
    m_registerButton->setRadius(10);
    m_registerButton->setPrimaryStyle();
    m_registerButton->setFont(AppFonts::applicationPixelWeightedFont(15, QFont::DemiBold));

    updateFieldGeometry();

    const QVector<RegisterInputField*> fields = {
        m_emailField,
        m_codeField,
        m_nicknameField,
        m_passwordField,
        m_repeatPasswordField
    };
    for (RegisterInputField* field : fields) {
        connect(field->lineEdit(), &QLineEdit::textChanged, this, &RegisterWindow::updateValidationState);
        field->lineEdit()->installEventFilter(this);
    }

    setTabOrder(m_emailField->lineEdit(), m_codeField->lineEdit());
    setTabOrder(m_codeField->lineEdit(), m_nicknameField->lineEdit());
    setTabOrder(m_nicknameField->lineEdit(), m_passwordField->lineEdit());
    setTabOrder(m_passwordField->lineEdit(), m_repeatPasswordField->lineEdit());
    connect(m_repeatPasswordField->lineEdit(), &QLineEdit::returnPressed, this, &RegisterWindow::attemptRegister);
    connect(m_codeButton, &QPushButton::clicked, this, &RegisterWindow::requestVerificationCode);
    connect(m_cancelButton, &QPushButton::clicked, this, &RegisterWindow::scheduleClose);
    connect(m_registerButton, &QPushButton::clicked, this, &RegisterWindow::attemptRegister);

    updateValidationState();
}

void RegisterWindow::updateFieldGeometry()
{
    if (!m_emailField || !m_codeField || !m_nicknameField || !m_passwordField ||
        !m_repeatPasswordField || !m_cancelButton || !m_registerButton) {
        return;
    }

    int y = kTitleLabelHeight + kTitleToFieldSpacing;

    m_emailField->setGeometry(0, y, kFormWidth, kFieldHeight);
    y += kFieldHeight;
    if (m_emailRule && m_emailRule->isVisible()) {
        m_emailRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
        y += kRequirementHeight;
    } else {
        if (m_emailRule) {
            m_emailRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
        }
        y += kFieldVerticalSpacing;
    }

    m_codeField->setGeometry(0, y, kFormWidth - kCodeButtonWidth - kInlineButtonSpacing, kFieldHeight);
    m_codeButton->setGeometry(kFormWidth - kCodeButtonWidth, y, kCodeButtonWidth, kFieldHeight);
    y += kFieldHeight + kFieldVerticalSpacing;

    m_nicknameField->setGeometry(0, y, kFormWidth, kFieldHeight);
    y += kFieldHeight;
    if (m_nicknameRule && m_nicknameRule->isVisible()) {
        m_nicknameRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
        y += kRequirementHeight;
    } else {
        if (m_nicknameRule) {
            m_nicknameRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
        }
        y += kFieldVerticalSpacing;
    }

    m_passwordField->setGeometry(0, y, kFormWidth, kFieldHeight);
    y += kFieldHeight + kPasswordFieldToRulesSpacing;
    m_passwordLengthRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
    y += kRequirementHeight;
    m_passwordUpperRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
    y += kRequirementHeight;
    m_passwordDigitRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
    y += kRequirementHeight;
    m_passwordSymbolRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
    y += kRequirementHeight + kPasswordRulesToRepeatSpacing;

    m_repeatPasswordField->setGeometry(0, y, kFormWidth, kFieldHeight);
    y += kFieldHeight;
    m_repeatRule->setGeometry(0, y, kFormWidth, kRequirementHeight);
    y += kRequirementHeight + kRepeatRuleBottomSpacing;

    m_errorLabel->setGeometry(0, y, kFormWidth, kErrorLabelHeight);
    y += kErrorLabelHeight + kFieldVerticalSpacing;

    m_cancelButton->setGeometry(0, y, kCancelButtonWidth, kActionButtonHeight);
    m_registerButton->setGeometry(kCancelButtonWidth + kInlineButtonSpacing,
                                  y,
                                  kFormWidth - kCancelButtonWidth - kInlineButtonSpacing,
                                  kActionButtonHeight);
}

bool RegisterWindow::eventFilter(QObject* watched, QEvent* event)
{
    auto isFieldLineEdit = [this, watched](const RegisterInputField* field) {
        return field && field->lineEdit() && watched == field->lineEdit();
    };

    if ((isFieldLineEdit(m_emailField)
         || isFieldLineEdit(m_codeField)
         || isFieldLineEdit(m_nicknameField)
         || isFieldLineEdit(m_passwordField)
         || isFieldLineEdit(m_repeatPasswordField))
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        updateValidationState();
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* targetWidget = qobject_cast<QWidget*>(watched);
        if (targetWidget && targetWidget->window() == this) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint globalPos = mouseEvent->globalPosition().toPoint();
            const QVector<RegisterInputField*> fields = {
                m_emailField,
                m_codeField,
                m_nicknameField,
                m_passwordField,
                m_repeatPasswordField
            };
            bool insideField = false;
            for (RegisterInputField* field : fields) {
                insideField = insideField || (field && field->rect().contains(field->mapFromGlobal(globalPos)));
            }
            if (!insideField) {
                if (QWidget* focused = focusWidget()) {
                    focused->clearFocus();
                }
            }
        }
    }

    return SystemWindow::eventFilter(watched, event);
}

void RegisterWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), registerBackgroundColor());
}

void RegisterWindow::showEvent(QShowEvent* event)
{
    SystemWindow::showEvent(event);
    centerOnOwnerOrScreen();
    if (m_emailField) {
        m_emailField->lineEdit()->setFocus(Qt::OtherFocusReason);
    }
}

void RegisterWindow::updateBackdropTheme()
{
    setBackdropColor(ThemeManager::instance().color(ThemeColor::WindowBackdropTint));
}

void RegisterWindow::updateValidationState()
{
    const bool showEmailError = !m_emailField->text().trimmed().isEmpty()
            && !m_emailField->lineEdit()->hasFocus()
            && !emailValid();
    m_emailRule->setVisible(showEmailError);
    m_emailRule->setContentVisible(showEmailError);

    const bool showNicknameError = !m_nicknameField->text().trimmed().isEmpty()
            && !m_nicknameField->lineEdit()->hasFocus()
            && !nicknameValid();
    m_nicknameRule->setVisible(showNicknameError);
    m_nicknameRule->setContentVisible(showNicknameError);
    updateFieldGeometry();

    m_passwordLengthRule->setPassed(passwordLengthValid());
    m_passwordUpperRule->setPassed(passwordUpperValid());
    m_passwordDigitRule->setPassed(passwordDigitValid());
    m_passwordSymbolRule->setPassed(passwordSymbolValid());
    m_repeatRule->setPassed(repeatPasswordValid());
    if (m_registerButton) {
        m_registerButton->setEnabled(emailValid()
                                     && verificationCodeValid()
                                     && nicknameValid()
                                     && passwordLengthValid()
                                     && passwordUpperValid()
                                     && passwordDigitValid()
                                     && passwordSymbolValid()
                                     && repeatPasswordValid());
    }
    setErrorText(QString());
}

void RegisterWindow::attemptRegister()
{
    updateValidationState();
    if (!m_registerButton->isEnabled()) {
        setErrorText(QStringLiteral("请先完成所有格式要求"));
        return;
    }

    const QString accountId = m_emailField->text().trimmed().toLower();
    const QString password = m_passwordField->text();
    const QString nickname = m_nicknameField->text().trimmed();
    if (!LoginAccountRepository::instance().registerAccount(accountId, password, nickname)) {
        setErrorText(QStringLiteral("该邮箱已经注册"));
        GlobalNotification::showFailure(this, QStringLiteral("注册失败"));
        return;
    }

    emit accountRegistered(accountId, password);
    GlobalNotification::showSuccess(parentWidget() ? parentWidget() : this, QStringLiteral("注册成功"));
    scheduleClose();
}

void RegisterWindow::requestVerificationCode()
{
    if (!emailValid()) {
        m_emailField->lineEdit()->clearFocus();
        updateValidationState();
        setErrorText(m_emailField->text().trimmed().isEmpty()
                     ? QStringLiteral("请输入邮箱")
                     : QStringLiteral("邮箱格式错误"));
        GlobalNotification::showFailure(this, QStringLiteral("无法获取验证码"));
        return;
    }

    setErrorText(QString());
    GlobalNotification::showSuccess(this, QStringLiteral("验证码已发送"));
    m_codeField->lineEdit()->setFocus(Qt::OtherFocusReason);
}

void RegisterWindow::scheduleClose()
{
    QTimer::singleShot(0, this, &QWidget::close);
}

void RegisterWindow::centerOnOwnerOrScreen()
{
    if (QWidget* owner = parentWidget()) {
        move(owner->frameGeometry().center() - rect().center());
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    move(available.center() - rect().center());
}

void RegisterWindow::setErrorText(const QString& text)
{
    if (m_errorLabel) {
        m_errorLabel->setText(text);
    }
}

bool RegisterWindow::emailValid() const
{
    static const QRegularExpression pattern(
            QStringLiteral(R"(^[A-Z0-9._%+\-]+@[A-Z0-9.\-]+\.[A-Z]{2,}$)"),
            QRegularExpression::CaseInsensitiveOption);
    return pattern.match(m_emailField->text().trimmed()).hasMatch();
}

bool RegisterWindow::verificationCodeValid() const
{
    return !m_codeField->text().trimmed().isEmpty();
}

bool RegisterWindow::nicknameValid() const
{
    const int length = m_nicknameField->text().trimmed().size();
    return length >= 2 && length <= 24;
}

bool RegisterWindow::passwordLengthValid() const
{
    const int length = m_passwordField->text().size();
    return length >= 8 && length <= 32;
}

bool RegisterWindow::passwordUpperValid() const
{
    return containsMatch(m_passwordField->text(), QStringLiteral(R"([A-Z])"));
}

bool RegisterWindow::passwordDigitValid() const
{
    return containsMatch(m_passwordField->text(), QStringLiteral(R"(\d)"));
}

bool RegisterWindow::passwordSymbolValid() const
{
    return containsMatch(m_passwordField->text(), QStringLiteral(R"([^A-Za-z0-9\s])"));
}

bool RegisterWindow::repeatPasswordValid() const
{
    const QString password = m_passwordField->text();
    return !password.isEmpty() && m_repeatPasswordField->text() == password;
}
