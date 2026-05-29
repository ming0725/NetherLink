#include "CopyIdButton.h"

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

CopyIdButton::CopyIdButton(QWidget* parent)
    : QToolButton(parent)
{
    setFixedSize(26, 24);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setIconSize(QSize(15, 15));
    setToolTip(QStringLiteral("复制ID"));
    setAccessibleName(QStringLiteral("复制ID"));
}

bool CopyIdButton::event(QEvent* event)
{
    if (event->type() == QEvent::Enter) {
        m_hovered = true;
        update();
    } else if (event->type() == QEvent::Leave) {
        m_hovered = false;
        update();
    }
    return QToolButton::event(event);
}

void CopyIdButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (isDown() || m_hovered) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(isDown()
                ? ThemeColor::ControlPressed
                : ThemeColor::ControlHover));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
    }

    QPixmap icon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/copy.svg"),
                                                   iconSize(),
                                                   Qt::KeepAspectRatio,
                                                   devicePixelRatioF());
    if (!icon.isNull()) {
        if (ThemeManager::instance().isDark()) {
            QImage image = icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
            image.invertPixels(QImage::InvertRgb);
            icon = QPixmap::fromImage(image);
            icon.setDevicePixelRatio(devicePixelRatioF());
        }
        QRect target(QPoint(0, 0), iconSize());
        target.moveCenter(rect().center());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target, icon);
        return;
    }

    const QColor iconColor = isEnabled()
            ? ThemeManager::instance().color(m_hovered ? ThemeColor::PrimaryText : ThemeColor::TertiaryText)
            : ThemeManager::instance().color(ThemeColor::PlaceholderText);
    painter.setPen(QPen(iconColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QRect iconRect(QPoint(0, 0), QSize(16, 16));
    QRect centeredIconRect = iconRect;
    centeredIconRect.moveCenter(rect().center());
    const QRectF backSheet(centeredIconRect.left() + 6,
                           centeredIconRect.top() + 2,
                           8,
                           10);
    const QRectF frontSheet(centeredIconRect.left() + 2,
                            centeredIconRect.top() + 6,
                            10,
                            8);
    painter.drawRoundedRect(backSheet, 2, 2);
    painter.fillRect(frontSheet.adjusted(0, 0, 1, 1),
                     ThemeManager::instance().color(ThemeColor::PageBackground));
    painter.drawRoundedRect(frontSheet, 2, 2);
}
