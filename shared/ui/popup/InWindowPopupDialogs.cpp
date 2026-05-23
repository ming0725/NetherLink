#include "InWindowPopupDialogs.h"

#include "InWindowPopupOverlay.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

#include <QApplication>
#include <QEventLoop>
#include <QFont>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kPopupPadding = 24;
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 34;
constexpr int kInputHeight = 36;

QColor popupStrokeColor()
{
    return ThemeManager::instance().isDark()
            ? QColor(0x88, 0x88, 0x88, 190)
            : QColor(0x88, 0x88, 0x88, 170);
}

QColor colorCompositedOver(const QColor& foreground, const QColor& background)
{
    const QColor fg = foreground.toRgb();
    const QColor bg = background.toRgb();
    const qreal alpha = fg.alphaF();
    return QColor(qRound(fg.red() * alpha + bg.red() * (1.0 - alpha)),
                  qRound(fg.green() * alpha + bg.green() * (1.0 - alpha)),
                  qRound(fg.blue() * alpha + bg.blue() * (1.0 - alpha)));
}

PaintedLabel* createTitleLabel(const QString& title)
{
    auto* label = new PaintedLabel(title);
    label->setObjectName(QStringLiteral("inWindowPopupTitle"));
    label->setWordWrap(true);
    QFont font = QApplication::font();
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    return label;
}

PaintedLabel* createBodyLabel(const QString& text)
{
    auto* label = new PaintedLabel(text);
    label->setObjectName(QStringLiteral("inWindowPopupBody"));
    label->setWordWrap(true);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    QFont font = QApplication::font();
    font.setPixelSize(14);
    label->setFont(font);
    return label;
}

StatefulPushButton* createButton(const QString& text, QWidget* parent = nullptr)
{
    auto* button = new StatefulPushButton(text, parent);
    button->setFixedSize(kButtonWidth, kButtonHeight);
    return button;
}

void applyPopupDefaultButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }

    button->setDefaultStyle();
    button->setBorderColor(popupStrokeColor());
    button->setBorderWidth(1);
}

InWindowPopupOverlay* overlayFor(QWidget* widget)
{
    QWidget* cursor = widget;
    while (cursor) {
        if (auto* overlay = qobject_cast<InWindowPopupOverlay*>(cursor)) {
            return overlay;
        }
        cursor = cursor->parentWidget();
    }
    return nullptr;
}

QWidget* createMessageContent(const QString& title,
                              const QString& text,
                              InWindowPopup::Button defaultButton,
                              InWindowPopup::Button* result)
{
    auto* content = new QWidget;
    content->setMinimumWidth(360);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
    layout->setSpacing(14);

    layout->addWidget(createTitleLabel(title));
    layout->addWidget(createBodyLabel(text));
    layout->addSpacing(2);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto* noButton = createButton(QStringLiteral("取消"), content);
    applyPopupDefaultButtonStyle(noButton);
    auto* yesButton = createButton(QStringLiteral("确认"), content);
    yesButton->setPrimaryStyle();
    if (defaultButton == InWindowPopup::Button::No) {
        noButton->setFocus();
    } else {
        yesButton->setFocus();
    }

    buttonLayout->addWidget(noButton);
    buttonLayout->addWidget(yesButton);
    layout->addLayout(buttonLayout);

    QObject::connect(noButton, &QPushButton::clicked, content, [result, content]() {
        *result = InWindowPopup::Button::No;
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });
    QObject::connect(yesButton, &QPushButton::clicked, content, [result, content]() {
        *result = InWindowPopup::Button::Yes;
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    });

    return content;
}

} // namespace

InWindowPopup::Button InWindowPopup::question(QWidget* parent,
                                             const QString& title,
                                             const QString& text,
                                             Button defaultButton)
{
    Button result = Button::No;
    QWidget* content = createMessageContent(title, text, defaultButton, &result);

    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(460, 260);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        return result;
    }

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

QString InWindowPopup::getText(QWidget* parent,
                               const QString& title,
                               const QString& label,
                               QLineEdit::EchoMode echo,
                               const QString& text,
                               bool* accepted)
{
    if (accepted) {
        *accepted = false;
    }

    auto* content = new QWidget;
    content->setMinimumWidth(380);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
    layout->setSpacing(14);
    layout->addWidget(createTitleLabel(title));
    layout->addWidget(createBodyLabel(label));

    auto* edit = new InlineEditableText(content);
    edit->setTrimTextOnCommit(false);
    edit->setEchoMode(echo);
    edit->setText(text);
    edit->setMinimumHeight(kInputHeight);
    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor selectionBackground = ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedTextColor = ThemeManager::textColorOn(
            colorCompositedOver(selectionBackground, inputBackground));
    edit->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    edit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    edit->setNormalBackgroundColor(inputBackground);
    edit->setHoverBackgroundColor(inputBackground);
    edit->setFocusBackgroundColor(inputBackground);
    edit->setNormalBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setSelectionBackgroundColor(selectionBackground);
    edit->setSelectedTextColor(selectedTextColor);
    edit->setBorderWidth(1);
    edit->setRadius(8);
    edit->setHorizontalPadding(10);
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(edit);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto* cancelButton = createButton(QStringLiteral("取消"), content);
    applyPopupDefaultButtonStyle(cancelButton);
    auto* okButton = createButton(QStringLiteral("确认"), content);
    okButton->setPrimaryStyle();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    QString result = text;
    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content);
    if (!overlay) {
        return QString();
    }

    auto acceptInput = [accepted, edit, overlay, &result]() {
        result = edit->text();
        if (accepted) {
            *accepted = true;
        }
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    QObject::connect(okButton, &QPushButton::clicked, edit, acceptInput);
    QObject::connect(edit, &InlineEditableText::returnPressed, edit, acceptInput);
    QObject::connect(cancelButton, &QPushButton::clicked, edit, [overlay]() {
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });

    QTimer::singleShot(0, edit, [edit]() {
        edit->startEditing();
    });

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}
