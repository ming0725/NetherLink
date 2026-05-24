#pragma once

#include <QFrame>

class QPaintEvent;

class InWindowPopupFrame final : public QFrame
{
public:
    explicit InWindowPopupFrame(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};
