/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMouseEvent>
#include <QPainter>

#include "View/Friend/NotificationItem.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

NotificationItem::NotificationItem(const QString& text, QWidget* parent) : QWidget(parent), textLabel(new QLabel(text, this)), arrowLabel(new QLabel(this)), badge(new NotificationBadge(this)) {
    setFixedHeight(40);
    setMouseTracking(true);

    // 设置文本标签样式
    QFont font = textLabel->font();

    font.setPixelSize(14);
    textLabel->setFont(font);

    // 设置箭头图标
    QPixmap arrowPixmap(":/icon/right_arrow.png");

    arrowLabel->setPixmap(arrowPixmap.scaled(10, 10, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 初始化徽章
    badge->setCount(0);
    badge->hide();
}

QSize NotificationItem::sizeHint() const {
    return (QSize(200, 40));
}

void NotificationItem::setUnreadCount(int count) {
    if (count > 0) {
        badge->setCount(count);
        badge->show();
    } else {
        badge->setCount(0);
        badge->hide();
    }
    update();
    resizeEvent(nullptr);
}

int NotificationItem::getUnreadCount() const {
    return (badge->getCount());
}

void NotificationItem::clearUnreadCount() {
    setUnreadCount(0);
}

void NotificationItem::setSelected(bool select) {
    selected = select;
    update();
}

void NotificationItem::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    // 设置背景颜色
    if (pressed) {
        painter.fillRect(rect(), QColor(0xD3D3D3));  // 按下时为灰色
    } else if (selected) {
        painter.fillRect(rect(), QColor(0xE6E6E6));  // 选中时为中灰色
    } else if (hovered) {
        painter.fillRect(rect(), QColor(0xF0F0F0));  // 悬停时为浅灰色
    } else {
        painter.fillRect(rect(), Qt::white);         // 正常时为白色
    }
}

void NotificationItem::enterEvent(QEnterEvent*) {
    hovered = true;
    update();
}

void NotificationItem::leaveEvent(QEvent*) {
    hovered = false;
    pressed = false;
    update();
}

void NotificationItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed = true;
        update();
    }
}

void NotificationItem::mouseReleaseEvent(QMouseEvent* event) {
    if ((event->button() == Qt::LeftButton) && pressed) {
        pressed = false;

        if (rect().contains(event->pos())) {
            clearUnreadCount();  // 点击时清除未读消息

            emit clicked();
        }
        update();
    }
}

void NotificationItem::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    const int padding = 12;
    const int arrowWidth = 16;
    const int badgeWidth = badge->sizeHint().width();

    // 文本标签位置（为徽章预留空间）
    textLabel->setGeometry(padding, 0, width() - padding * 2 - arrowWidth - badgeWidth - 8, height());

    // 徽章位置（在箭头左侧）
    QSize badgeSize = badge->sizeHint();
    int badgeY = (height() - badgeSize.height()) / 2;

    badge->setGeometry(width() - padding - arrowWidth - badgeWidth - 8, badgeY + 2, badgeSize.width(), badgeSize.height());

    // 箭头位置
    arrowLabel->setGeometry(width() - padding - arrowWidth, (height() - 16) / 2, arrowWidth, 16);
}
