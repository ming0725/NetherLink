#include "../include/ApplyWindow.h"
#include "../include/SearchResultItem.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

SearchResultItem::SearchResultItem(const User& user, QString& uid, QWidget* parent) : QWidget(parent), avatarLabel(new AvatarLabel(this)), type(UserType), status(user.status), id(uid), name(user.nick), avatarUrl(user.avatarPath) {
    fullNameText = QString("%1 (%2)").arg(user.nick, user.id);
    fullStatusAndSignText = QString("[%1] %2").arg(::statusText(user.status), user.signature);
    setMouseTracking(true);
    setupUI();
    avatarLabel->loadAvatar(id, user.avatarPath, false);
}

SearchResultItem::SearchResultItem(const QString& groupId, const QString& groupName, int memberCount, QString& avatarUrl, QWidget* parent) : QWidget(parent), avatarLabel(new AvatarLabel(this)), type(GroupType), id(groupId), name(groupName), avatarUrl(avatarUrl) {
    fullNameText = QString("%1 (%2)").arg(groupName, groupId);
    fullStatusAndSignText = QString("%1人").arg(memberCount);
    setMouseTracking(true);
    setupUI();
    avatarLabel->loadAvatar(groupId, QUrl(avatarUrl), true);
}

void SearchResultItem::setupUI() {
    avatarLabel->setFixedSize(AVATAR_SIZE, AVATAR_SIZE);
    nameLabel = new QLabel(fullNameText, this);
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QFont font;

    font.setPixelSize(14);
    nameLabel->setFont(font);
    statusIconLabel = new QLabel(this);

    if (type == UserType) {
        statusIconLabel->setPixmap(QPixmap(::statusIconPath(status)).scaled(STATUS_ICON_SIZE, STATUS_ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        statusIconLabel->setPixmap(QPixmap(":/icon/friend_selected.png").scaled(STATUS_ICON_SIZE, STATUS_ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    statusIconLabel->setFixedSize(STATUS_ICON_SIZE + 2, STATUS_ICON_SIZE + 2);
    statusTextLabel = new QLabel(this);
    statusTextLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusTextLabel->setText(fullStatusAndSignText);
    font.setPixelSize(12);
    statusTextLabel->setFont(font);

    QPalette palette = statusTextLabel->palette();

    palette.setColor(QPalette::WindowText, Qt::gray);
    statusTextLabel->setPalette(palette);
    actionButton = new QPushButton((type == UserType) ? "添加" : "加入", this);
    actionButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    actionButton->setStyleSheet(R"(
        QPushButton {
            background-color: #0099FF;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #33ADFF;
        }
        QPushButton:pressed {
            background-color: #0077CC;
        }
    )");

    // 连接按钮点击事件
    connect(actionButton, &QPushButton::clicked, this, &SearchResultItem::onActionButtonClicked);
}

void SearchResultItem::onActionButtonClicked() {
    auto* window = new ApplyWindow((type == UserType) ? ApplyWindow::User : ApplyWindow::Group, id, name, avatarUrl);

    window->show();
}

QSize SearchResultItem::sizeHint() const {
    return (QSize(144, 72));
}

void SearchResultItem::enterEvent(QEnterEvent*) {
    hovered = true;
    update();
}

void SearchResultItem::leaveEvent(QEvent*) {
    hovered = false;
    update();
}

void SearchResultItem::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    if (hovered) {
        painter.fillRect(rect(), QColor(0xf0f0f0));
    } else {
        painter.fillRect(rect(), QColor(0xffffff));
    }
}

void SearchResultItem::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    int w = width();
    int h = height();
    int cy = h / 2;

    // 头像
    avatarLabel->setGeometry(LEFT_PADDING, cy - AVATAR_SIZE / 2, AVATAR_SIZE, AVATAR_SIZE);

    // 计算文字区域起始X
    const int contentX = LEFT_PADDING + AVATAR_SIZE + SPACING;

    // 两行文字的高度与偏移
    QFontMetrics fmName(nameLabel->font());
    QFontMetrics fmText(statusTextLabel->font());
    const int nameH = fmName.height();
    const int textH = fmText.height();

    // 第一行：姓名
    const int nameY_bottom = cy - BETWEEN / 2;
    const int nameY_top = nameY_bottom - nameH;

    nameLabel->setGeometry(contentX, nameY_top, w - contentX - LEFT_PADDING - BUTTON_WIDTH - SPACING, nameH);

    // 按钮位置
    actionButton->setGeometry(w - LEFT_PADDING - BUTTON_WIDTH, cy - BUTTON_HEIGHT / 2, BUTTON_WIDTH, BUTTON_HEIGHT);

    // 第二行：状态/成员数
    const int textY_top = cy + BETWEEN / 2;
    const int textW_available = w - contentX - LEFT_PADDING - BUTTON_WIDTH - SPACING;

    statusIconLabel->setGeometry(contentX, textY_top + 1, statusIconLabel->width(), statusIconLabel->height());
    statusTextLabel->setGeometry(contentX + statusIconLabel->width() + SPACING - 6, textY_top + 1, textW_available - statusIconLabel->width() - SPACING, textH);

    // 处理文本省略
    {
        QFontMetrics fm(nameLabel->font());

        nameLabel->setText(fm.elidedText(fullNameText, Qt::ElideRight, nameLabel->width()));
    }
    {
        QFontMetrics fm(statusTextLabel->font());

        statusTextLabel->setText(fm.elidedText(fullStatusAndSignText, Qt::ElideRight, statusTextLabel->width()));
    }
}
