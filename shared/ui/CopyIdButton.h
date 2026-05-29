#pragma once

#include <QToolButton>

class CopyIdButton final : public QToolButton
{
public:
    explicit CopyIdButton(QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_hovered = false;
};
