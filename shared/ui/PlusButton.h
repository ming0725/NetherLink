#pragma once

#include "shared/ui/StatefulPushButton.h"

class PlusButton final : public StatefulPushButton
{
    Q_OBJECT

public:
    explicit PlusButton(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};
