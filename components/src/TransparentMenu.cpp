#include "TransparentMenu.h"
#include <QPainter>
#include <QStyleOption>
#include <QProxyStyle>

class MenuItemStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type,
                           const QStyleOption *opt,
                           const QSize &size,
                           const QWidget *widget) const override
    {
        QSize s = QProxyStyle::sizeFromContents(type, opt, size, widget);
        if (type == QStyle::CT_MenuItem) {

            if (auto mo = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)) {
                if (mo->menuItemType == QStyleOptionMenuItem::Separator) {
                    return s;
                }
            }

            int minH = 40;
            if (auto menu = qobject_cast<const TransparentMenu*>(widget)) {
                minH = menu->itemMinHeight();
            }
            if (s.height() < minH) {
                s.setHeight(minH);
            }
        }
        return s;
    }
};

TransparentMenu::TransparentMenu(QWidget *parent) : QMenu(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setContentsMargins(15, 12, 15, 12);
    setStyle(new MenuItemStyle(style()));
}

void TransparentMenu::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制半透明白色背景并加灰色描边
    QRectF contentRect = rect().adjusted(15, 12, -15, -12);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(contentRect, 8, 8);

    // 灰色描边 (边框宽度 1px)
    painter.setPen(QPen(QColor(224, 224, 224, 128), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(contentRect, 8, 8);

    // 遍历绘制每项
    for (QAction* action : actions()) {
        QRect itemRect = actionGeometry(action);

        if (action->isSeparator()) {
            // 绘制分隔线，使用 QSS 中的 rgba(0,0,0,0.1)
            painter.setPen(QPen(QColor(0, 0, 0, 25), 1));
            int y = itemRect.center().y();
            painter.drawLine(itemRect.left() + 8, y, itemRect.right() - 8, y);
        } else {
            bool isSelected = itemRect.contains(mapFromGlobal(QCursor::pos()));
            if (isSelected) {
                // 选中背景，rgba(0,0,0,0.1)
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 25));
                painter.drawRoundedRect(itemRect.adjusted(4, 2, -4, -2), 4, 4);
            }

            // 绘制图标
            bool hasIcon = !action->icon().isNull();
            if (hasIcon) {
                QSize iconSize(16, 16);
                QRect iconRect(itemRect.left() + 12,
                               itemRect.top() + (itemRect.height() - iconSize.height()) / 2,
                               iconSize.width(), iconSize.height());
                action->icon().paint(&painter, iconRect);
            }

            // 绘制文本
            painter.setPen(QColor(51, 51, 51));
            QFont font = painter.font();
            font.setPixelSize(14);
            painter.setFont(font);

            int textLeft = itemRect.left() + (hasIcon ? 34 : 12);
            QRect textRect = itemRect.adjusted(textLeft - itemRect.left(), 0, -8, 0);
            painter.drawText(textRect,
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             action->text());
        }
    }
}

void TransparentMenu::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    move(pos() - QPoint(0, contentsMargins().top()));
}

void TransparentMenu::setItemMinHeight(int h) {
    if (h <= 0) return;
    m_itemMinHeight = h;
    style()->unpolish(this);
    style()->polish(this);
    updateGeometry();
    update();
}

int TransparentMenu::itemMinHeight() const {
    return m_itemMinHeight;
}
