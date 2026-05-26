#include "LoginInputField.h"

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace {
constexpr int kFieldHeight = 44;
constexpr int kHorizontalInset = 38;
constexpr int kButtonSize = 28;
constexpr int kButtonGap = 2;
constexpr int kButtonRightInset = 8;
constexpr int kRadius = 10;
} // namespace

LoginInputField::LoginInputField(QWidget* parent)
    : QWidget(parent)
    , m_lineEdit(new QLineEdit(this))
{
    setFixedHeight(kFieldHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    m_lineEdit->setFrame(false);
    m_lineEdit->setAlignment(Qt::AlignCenter);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_lineEdit->setAutoFillBackground(false);
    m_lineEdit->setFont(AppFonts::applicationPixelSizedFont(15));
    updateTextMargins();

    QPalette editPalette = m_lineEdit->palette();
    editPalette.setColor(QPalette::Base, Qt::transparent);
    editPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    editPalette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
    editPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::AccentTextSelection));
    editPalette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent));
    m_lineEdit->setPalette(editPalette);

    m_lineEdit->installEventFilter(this);
    connect(m_lineEdit, &QLineEdit::textChanged, this, &LoginInputField::textChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        QPalette palette = m_lineEdit->palette();
        palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
        palette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
        palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::AccentTextSelection));
        palette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent));
        m_lineEdit->setPalette(palette);
        update();
    });

    updateLineEditGeometry();
}

void LoginInputField::setDropdownEnabled(bool enabled)
{
    if (m_dropdownEnabled == enabled) {
        return;
    }
    m_dropdownEnabled = enabled;
    updateTextMargins();
    update();
}

bool LoginInputField::dropdownEnabled() const
{
    return m_dropdownEnabled;
}

void LoginInputField::setPasswordMode(bool enabled)
{
    m_lineEdit->setEchoMode(enabled ? QLineEdit::Password : QLineEdit::Normal);
}

void LoginInputField::setDigitsOnly(bool enabled)
{
    if (enabled) {
        auto* validator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d*")), m_lineEdit);
        m_lineEdit->setValidator(validator);
    } else {
        m_lineEdit->setValidator(nullptr);
    }
}

void LoginInputField::setPlaceholderText(const QString& text)
{
    m_lineEdit->setPlaceholderText(text);
}

void LoginInputField::setText(const QString& text)
{
    m_lineEdit->setText(text);
}

QString LoginInputField::text() const
{
    return m_lineEdit->text();
}

QLineEdit* LoginInputField::lineEdit() const
{
    return m_lineEdit;
}

void LoginInputField::paintEvent(QPaintEvent* event)
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

    const QRectF fieldRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(fill);
    painter.setPen(QPen(border, focused ? 1.4 : 1.0));
    painter.drawRoundedRect(fieldRect, kRadius, kRadius);

    if (focused) {
        const QRectF clearRect(clearButtonRect());
        QPen clearPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                      1.7,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter.setPen(clearPen);
        const QPointF center = clearRect.center();
        painter.drawLine(QPointF(center.x() - 4.0, center.y() - 4.0),
                         QPointF(center.x() + 4.0, center.y() + 4.0));
        painter.drawLine(QPointF(center.x() + 4.0, center.y() - 4.0),
                         QPointF(center.x() - 4.0, center.y() + 4.0));
    }

    if (m_dropdownEnabled) {
        const QRectF arrowRect(dropdownButtonRect());
        const QPointF center = arrowRect.center();
        QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                      1.6,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(center.x() - 4.5, center.y() - 2.0),
                         QPointF(center.x(), center.y() + 2.5));
        painter.drawLine(QPointF(center.x(), center.y() + 2.5),
                         QPointF(center.x() + 4.5, center.y() - 2.0));
    }
}

void LoginInputField::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLineEditGeometry();
}

void LoginInputField::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_lineEdit->hasFocus() && clearButtonRect().contains(event->pos())) {
        m_lineEdit->clear();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_dropdownEnabled && dropdownButtonRect().contains(event->pos())) {
        emit dropdownRequested();
        event->accept();
        return;
    }

    m_lineEdit->setFocus(Qt::MouseFocusReason);
    QWidget::mousePressEvent(event);
}

void LoginInputField::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void LoginInputField::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

bool LoginInputField::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_lineEdit) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
            updateClearButtonState();
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QPoint parentPos = m_lineEdit->mapTo(this, mouseEvent->pos());
                if (m_lineEdit->hasFocus() && clearButtonRect().contains(parentPos)) {
                    m_lineEdit->clear();
                    return true;
                }
                if (m_dropdownEnabled && dropdownButtonRect().contains(parentPos)) {
                    emit dropdownRequested();
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect LoginInputField::clearButtonRect() const
{
    if (m_dropdownEnabled) {
        return QRect(width() - (kButtonSize * 2) - kButtonGap - kButtonRightInset,
                     (height() - kButtonSize) / 2,
                     kButtonSize,
                     kButtonSize);
    }
    return dropdownButtonRect();
}

QRect LoginInputField::dropdownButtonRect() const
{
    return QRect(width() - kButtonSize - kButtonRightInset,
                 (height() - kButtonSize) / 2,
                 kButtonSize,
                 kButtonSize);
}

void LoginInputField::updateLineEditGeometry()
{
    m_lineEdit->setGeometry(0, 1, width(), qMax(0, height() - 2));
}

void LoginInputField::updateTextMargins()
{
    const int trailingControlsWidth = m_dropdownEnabled
            ? kButtonRightInset + kButtonSize + kButtonGap + kButtonSize + kButtonRightInset
            : kHorizontalInset;
    const int inset = qMax(kHorizontalInset, trailingControlsWidth);
    m_lineEdit->setTextMargins(inset, 0, inset, 0);
}

void LoginInputField::updateClearButtonState()
{
    const bool focused = m_lineEdit->hasFocus();
    if (m_hasEditFocus == focused) {
        return;
    }
    m_hasEditFocus = focused;
    update();
}
