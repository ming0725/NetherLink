#include "shared/ui/popup/dialogs/InWindowPopupDialogStyle.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

#include <QApplication>
#include <QFont>

namespace InWindowPopup::DialogStyle {

PaintedLabel* createTitleLabel(const QString& title, QWidget* parent)
{
    auto* label = new PaintedLabel(title, parent);
    label->setObjectName(QStringLiteral("inWindowPopupTitle"));
    label->setWordWrap(true);
    QFont font = QApplication::font();
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    return label;
}

PaintedLabel* createBodyLabel(const QString& text, QWidget* parent)
{
    auto* label = new PaintedLabel(text, parent);
    label->setObjectName(QStringLiteral("inWindowPopupBody"));
    label->setWordWrap(true);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    QFont font = QApplication::font();
    font.setPixelSize(14);
    label->setFont(font);
    return label;
}

StatefulPushButton* createButton(const QString& text, QWidget* parent)
{
    auto* button = new StatefulPushButton(text, parent);
    button->setFixedSize(kButtonWidth, kButtonHeight);
    return button;
}

void applyDefaultButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }

    button->setDefaultStyle();
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::InWindowPopupStroke));
    button->setBorderWidth(1);
}

void applyTextInputStyle(InlineEditableText* edit)
{
    if (!edit) {
        return;
    }

    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor selectionBackground = ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedTextColor = ThemeManager::textColorOn(
            ThemeManager::colorCompositedOver(selectionBackground, inputBackground));
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
}

} // namespace InWindowPopup::DialogStyle
