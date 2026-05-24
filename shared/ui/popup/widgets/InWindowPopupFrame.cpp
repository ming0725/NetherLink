#include "shared/ui/popup/widgets/InWindowPopupFrame.h"

#include "shared/theme/ThemeManager.h"

#include <QPainter>

namespace {

constexpr int kPopupRadius = 12;

} // namespace

InWindowPopupFrame::InWindowPopupFrame(QWidget* parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
}

void InWindowPopupFrame::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF frameRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));
    painter.drawRoundedRect(frameRect, kPopupRadius, kPopupRadius);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::InWindowPopupStroke), 1));
    painter.drawRoundedRect(frameRect, kPopupRadius, kPopupRadius);
}
