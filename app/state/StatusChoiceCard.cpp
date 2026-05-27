#include "app/state/StatusChoiceCard.h"

#include "app/state/CurrentUserPopupStyle.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/PaintedLabel.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QVBoxLayout>

using namespace CurrentUserPopupStyle;

StatusChoiceCard::StatusChoiceCard(const QString& title,
                                   const QString& iconPath,
                                   int index,
                                   QWidget* parent)
    : QWidget(parent)
    , m_choiceIndex(index)
    , m_icon(new QLabel(this))
    , m_title(new PaintedLabel(title, this))
    , m_iconPath(iconPath)
{
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setMinimumSize(132, 178);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_icon->setFixedSize(kStatusChoiceIconSize, kStatusChoiceIconSize);
    m_icon->setAlignment(Qt::AlignCenter);
    m_title->setFont(AppFonts::applicationPixelSizedFont(16, true));
    m_title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 22, 18, 18);
    layout->setSpacing(18);
    layout->addStretch(1);
    layout->addWidget(m_icon, 0, Qt::AlignHCenter);
    layout->addWidget(m_title);
    layout->addStretch(1);
    refresh();
}

int StatusChoiceCard::choiceIndex() const
{
    return m_choiceIndex;
}

void StatusChoiceCard::setSelected(bool selected)
{
    m_selected = selected;
    refresh();
    update();
}

void StatusChoiceCard::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void StatusChoiceCard::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void StatusChoiceCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        if (clicked) {
            clicked(m_choiceIndex);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void StatusChoiceCard::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
    QColor stroke = m_selected ? accent : ThemeManager::instance().color(ThemeColor::Divider);
    if (m_hovered && !m_selected) {
        stroke = QColor(0xac, 0xac, 0xac, 128);
    }
    const int strokeWidth = m_selected ? 2 : 1;
    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(stroke, strokeWidth));
    painter.drawRoundedRect(cardRect.adjusted(strokeWidth / 2.0,
                                              strokeWidth / 2.0,
                                              -strokeWidth / 2.0,
                                              -strokeWidth / 2.0),
                            kStatusChoiceRadius,
                            kStatusChoiceRadius);
}

void StatusChoiceCard::refresh()
{
    m_title->setTextColor(m_selected
                          ? ThemeManager::instance().color(ThemeColor::Accent)
                          : ThemeManager::instance().color(ThemeColor::PrimaryText));
    const QPixmap pixmap = ImageService::instance().scaled(m_iconPath,
                                                          m_icon->size(),
                                                          Qt::KeepAspectRatio,
                                                          devicePixelRatioF());
    m_icon->setPixmap(pixmap);
}
