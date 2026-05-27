#include "app/state/CurrentUserPopupStyle.h"

#include "shared/services/AppFonts.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

namespace CurrentUserPopupStyle {

PaintedLabel* makeLabel(const QString& text, ThemeColor role, int pixelSize, QWidget* parent)
{
    auto* label = new PaintedLabel(text, parent);
    label->setProperty("themeTextRole", static_cast<int>(role));
    label->setTextColor(ThemeManager::instance().color(role));
    label->setFont(AppFonts::applicationPixelSizedFont(pixelSize, false));
    return label;
}

void applyEditStyle(InlineEditableText* edit)
{
    if (!edit) {
        return;
    }

    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor selectionBackground =
            ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedText = ThemeManager::textColorOn(
            ThemeManager::colorCompositedOver(selectionBackground, inputBackground));

    edit->setFont(AppFonts::applicationPixelSizedFont(14, false));
    edit->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    edit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    edit->setNormalBackgroundColor(inputBackground);
    edit->setHoverBackgroundColor(inputBackground);
    edit->setFocusBackgroundColor(inputBackground);
    edit->setNormalBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Accent));
    edit->setSelectionBackgroundColor(selectionBackground);
    edit->setSelectedTextColor(selectedText);
    edit->setBorderWidth(1);
    edit->setRadius(8);
    edit->setHorizontalPadding(12);
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void applyDefaultButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }
    button->setDefaultStyle();
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

} // namespace CurrentUserPopupStyle
