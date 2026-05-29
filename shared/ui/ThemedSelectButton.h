#pragma once

#include <QToolButton>

class ThemedSelectButton final : public QToolButton
{
public:
    explicit ThemedSelectButton(QWidget* parent = nullptr);

    void setMenuHoverSuppressed(bool suppressed);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_hovered = false;
    bool m_menuHoverSuppressed = false;
};
