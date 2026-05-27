#pragma once

#include "shared/theme/ThemeManager.h"

class InlineEditableText;
class PaintedLabel;
class StatefulPushButton;
class QString;
class QWidget;

namespace CurrentUserPopupStyle {

constexpr int kProfileEditAvatarSize = 74;
constexpr int kProfileEditInputHeight = 36;
constexpr int kProfileEditLabelWidth = 62;
constexpr int kStatusChoiceRadius = 8;
constexpr int kStatusIconChoiceCount = 4;
constexpr int kStatusChoiceIconSize = 72;

PaintedLabel* makeLabel(const QString& text, ThemeColor role, int pixelSize, QWidget* parent);
void applyEditStyle(InlineEditableText* edit);
void applyDefaultButtonStyle(StatefulPushButton* button);

} // namespace CurrentUserPopupStyle
