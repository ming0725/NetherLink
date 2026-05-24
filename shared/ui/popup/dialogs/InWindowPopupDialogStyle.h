#pragma once

#include <QColor>
#include <QString>

class InlineEditableText;
class PaintedLabel;
class StatefulPushButton;
class QWidget;

namespace InWindowPopup::DialogStyle {

constexpr int kPopupPadding = 24;
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 34;
constexpr int kInputHeight = 36;

PaintedLabel* createTitleLabel(const QString& title, QWidget* parent = nullptr);
PaintedLabel* createBodyLabel(const QString& text, QWidget* parent = nullptr);
StatefulPushButton* createButton(const QString& text, QWidget* parent = nullptr);
void applyDefaultButtonStyle(StatefulPushButton* button);
void applyTextInputStyle(InlineEditableText* edit);

} // namespace InWindowPopup::DialogStyle
