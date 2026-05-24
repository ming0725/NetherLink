#pragma once

#include "shared/ui/popup/InWindowPopupDialogs.h"

#include <QWidget>

#include <functional>

namespace InWindowPopup {

class MessageContent final : public QWidget
{
public:
    MessageContent(const QString& title,
                   const QString& text,
                   Button defaultButton,
                   QWidget* parent = nullptr);

    std::function<void(Button)> buttonActivated;
};

} // namespace InWindowPopup
