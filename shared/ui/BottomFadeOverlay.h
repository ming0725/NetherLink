#pragma once

#include <QWidget>

#include "shared/theme/ThemeManager.h"

class BottomFadeOverlay final : public QWidget
{
public:
    explicit BottomFadeOverlay(QWidget* parent = nullptr);

    void setBackgroundRole(ThemeColor role);
    void setFadeHeight(int height);
    void setSolidAlpha(int alpha);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ThemeColor m_backgroundRole = ThemeColor::PageBackground;
    int m_fadeHeight = 32;
    int m_solidAlpha = 192;
};
