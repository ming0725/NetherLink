/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "Data/AvatarLoader.h"
#include "View/Friend/FriendItemDelegate.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

FriendItemDelegate::FriendItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {
}

void FriendItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (!index.isValid())
        return;

    const User user = index.data(Qt::UserRole).value<User>();
    bool isSelected = index.data(Qt::UserRole + 1).toBool();
    bool isHovered = option.state & QStyle::State_MouseOver;

    // 设置抗锯齿
    painter->setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    if (isSelected) {
        painter->fillRect(option.rect, QColor(0x0099ff));
    } else if (isHovered) {
        painter->fillRect(option.rect, QColor(0xf0f0f0));
    } else {
        painter->fillRect(option.rect, QColor(0xffffff));
    }

    // 计算各个区域的位置
    QRect avatarRect = calculateAvatarRect(option.rect);
    QRect nameRect = calculateNameRect(option.rect);
    QRect statusRect = calculateStatusRect(option.rect);
    QRect statusIconRect = calculateStatusIconRect(option.rect);

    // 绘制头像
    drawAvatar(painter, avatarRect, user.id, user.avatarPath, user.status);

    // 绘制姓名
    drawName(painter, nameRect, user.nick, isSelected);

    // 绘制状态图标
    drawStatusIcon(painter, statusIconRect, user.status);

    // 绘制状态文本
    QString statusText = QString("[%1] %2").arg(this->statusText(user.status), user.signature);
    drawStatus(painter, statusRect, user.status, statusText, isSelected);
}

QSize FriendItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(144, 72);
}

bool FriendItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit itemClicked(index.row());
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void FriendItemDelegate::drawAvatar(QPainter* painter, const QRect& rect, const QString& userID, const QString& avatarPath, UserStatus status) const {
    // 加载头像
    QPixmap avatar = AvatarLoader::instance().getAvatar(userID);
    
    if (avatar.isNull()) {
        // 如果没有头像，绘制默认圆形
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0xcccccc));
        painter->drawEllipse(rect);
    } else {
        // 绘制圆形头像
        QPainterPath path;
        path.addEllipse(rect);
        painter->setClipPath(path);
        painter->drawPixmap(rect, avatar);
        painter->setClipping(false);
        
        // 如果是离线用户，添加灰色遮罩
        if (status == Offline) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 80)); // 半透明黑色遮罩
            painter->drawEllipse(rect);
        }
    }
}

void FriendItemDelegate::drawName(QPainter* painter, const QRect& rect, const QString& name, bool isSelected) const {
    QFont font;
    font.setBold(false);
    font.setPixelSize(14);
    painter->setFont(font);

    QColor textColor = isSelected ? Qt::white : Qt::black;
    painter->setPen(textColor);

    // 文本省略处理
    QFontMetrics fm(font);
    QString elidedText = fm.elidedText(name, Qt::ElideRight, rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);
}

void FriendItemDelegate::drawStatus(QPainter* painter, const QRect& rect, UserStatus status, const QString& signature, bool isSelected) const {
    QFont font;
    font.setBold(false);
    font.setPixelSize(12);
    painter->setFont(font);

    QColor textColor = isSelected ? Qt::white : Qt::gray;
    painter->setPen(textColor);

    // 文本省略处理
    QFontMetrics fm(font);
    QString elidedText = fm.elidedText(signature, Qt::ElideRight, rect.width());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);
}

void FriendItemDelegate::drawStatusIcon(QPainter* painter, const QRect& rect, UserStatus status) const {
    QPixmap statusPixmap = QPixmap(statusIconPath(status));
    if (!statusPixmap.isNull()) {
        statusPixmap = statusPixmap.scaled(STATUS_ICON_SIZE, STATUS_ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(rect, statusPixmap);
    }
}

QString FriendItemDelegate::statusText(UserStatus status) const {
    switch (status) {
        case Online:
            return "在线";
        case Offline:
            return "离线";
        case Flying:
            return "飞行模式";
        case Mining:
            return "挖矿中";
        default:
            return "未知";
    }
}

QString FriendItemDelegate::statusIconPath(UserStatus status) const {
    switch (status) {
        case Online:
            return ":/icon/online.png";
        case Offline:
            return ":/icon/offline.png";
        case Flying:
            return ":/icon/flying.png";
        case Mining:
            return ":/icon/mining.png";
        default:
            return ":/icon/offline.png";
    }
}

QRect FriendItemDelegate::calculateAvatarRect(const QRect& contentRect) const {
    const int h = contentRect.height();
    const int cy = contentRect.top() + h / 2;
    int x = contentRect.left() + LEFT_PAD;
    return QRect(x, cy - AVATAR_SIZE / 2, AVATAR_SIZE, AVATAR_SIZE);
}

QRect FriendItemDelegate::calculateNameRect(const QRect& contentRect) const {
    const int w = contentRect.width();
    const int h = contentRect.height();
    const int cy = contentRect.top() + h / 2; // 垂直中心线 y
    int contentX = contentRect.left() + LEFT_PAD + AVATAR_SIZE + SPACING;
    QFont font;
    font.setPixelSize(14);
    QFontMetrics fmName(font);
    const int nameH = fmName.height();
    const int nameY_bottom = cy - BETWEEN / 2;
    const int nameY_top = nameY_bottom - nameH;
    return QRect(contentX, nameY_top, w - (contentX - contentRect.left()) - LEFT_PAD - SPACING, nameH);
}

QRect FriendItemDelegate::calculateStatusRect(const QRect& contentRect) const {
    const int w = contentRect.width();
    const int h = contentRect.height();
    const int cy = contentRect.top() + h / 2; // 垂直中心线 y
    int contentX = contentRect.left() + LEFT_PAD + AVATAR_SIZE + SPACING;
    QFont font;
    font.setPixelSize(12);
    QFontMetrics fmText(font);
    const int textH = fmText.height();
    const int textY_top = cy + BETWEEN / 2;
    const int textW_available = w - (contentX - contentRect.left()) - LEFT_PAD - STATUS_ICON_SIZE - SPACING;
    return QRect(contentX + STATUS_ICON_SIZE + ICO_TEXT_DIST, textY_top + 1, textW_available, textH);
}

QRect FriendItemDelegate::calculateStatusIconRect(const QRect& contentRect) const {
    const int h = contentRect.height();
    const int cy = contentRect.top() + h / 2; // 垂直中心线 y
    int contentX = contentRect.left() + LEFT_PAD + AVATAR_SIZE + SPACING;
    QFont font;
    font.setPixelSize(12);
    const int textY_top = cy + BETWEEN / 2;
    return QRect(contentX, textY_top + 1, STATUS_ICON_SIZE + 2, STATUS_ICON_SIZE + 2);
} 
