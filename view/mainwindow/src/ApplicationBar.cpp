#include "../include/ApplicationBar.h"
#include "../include/ApplicationBarItem.h"
#include "AvatarLoader.h"
#include "CurrentUser.h"
#include "UserRepository.h"
#include <QGraphicsBlurEffect>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>

ApplicationBar::ApplicationBar(QWidget* parent) : QWidget(parent) {
    noiseTexture = QImage(100, 100, QImage::Format_ARGB32);

    auto*rng = QRandomGenerator::global();   // 获取全局随机数生成器

    for (int x = 0; x < noiseTexture.width(); ++x) {
        for (int y = 0; y < noiseTexture.height(); ++y) {
            int gray = rng->bounded(56) + 200;   // 200~255
            int alpha = rng->bounded(51) + 30; // 30~80

            noiseTexture.setPixelColor(x, y, QColor(gray, gray, gray, alpha));
        }
    }

    // 创建头像标签并设置大小
    userHead = new AvatarLabel(this);
    userHead->setAvatarSize(avatarSize);

    // 加载当前用户头像
    QString userId = CurrentUser::instance().getUserId();
    QString avatarUrl = UserRepository::instance().getUser(userId).avatarPath;

    userHead->loadAvatar(userId, QUrl(avatarUrl));

    auto msgItem = new ApplicationBarItem(QPixmap(":/icon/unselected_message.png"), QPixmap(":/icon/selected_message.png"));

    msgItem->setPixmapScale(0.65);
    addItem(msgItem);

    auto friendItem = new ApplicationBarItem(QPixmap(":/icon/friend_unselected.png"), QPixmap(":/icon/friend_selected.png"));

    friendItem->setPixmapScale(0.62);
    addItem(friendItem);

    auto momentItem = new ApplicationBarItem(QPixmap(":/icon/unselected_blazer.png"), QPixmap(":/icon/blazer.png"));

    momentItem->setPixmapScale(0.68);
    addItem(momentItem);

    auto aiChat = new ApplicationBarItem(QPixmap(":/icon/unselected_aichat.png"), QPixmap(":/icon/aichat.png"));

    aiChat->setPixmapScale(0.77);
    addItem(aiChat);

    auto notItem = new ApplicationBarItem(QPixmap(":/icon/unselected_nether.png"), QPixmap(":/icon/nether.png"));

    notItem->setPixmapScale(0.72);
    addItem(notItem);

    auto menuItem = new ApplicationBarItem(QPixmap(":/icon/menu.png"));

    menuItem->setPixmapScale(0.57);
    addBottomItem(menuItem);

    auto starItem = new ApplicationBarItem(QPixmap(":/icon/bookmark.png"));

    starItem->setPixmapScale(0.57);
    addBottomItem(starItem);

    auto folderItem = new ApplicationBarItem(QPixmap(":/icon/folder.png"));

    folderItem->setPixmapScale(0.57);
    addBottomItem(folderItem);

    if (!topItems.empty()) {
        selectedItem = topItems[0];
        topItems[0]->setSelected(true);
        highlightPosY = selectedItem->y();
    }
    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(250);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged, this, [this] (const QVariant &value) {
        highlightPosY = value.toInt();
        update();
    });
}

void ApplicationBar::resizeEvent(QResizeEvent*) {
    layoutItems();
}

void ApplicationBar::paintEvent(QPaintEvent*) {
    QPainter p(this);

    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    p.save();
    p.fillRect(rect(), QColor(0xc4, 0xdd, 0xe6, 180));
    p.setOpacity(0.5);
    p.drawTiledPixmap(rect(), QPixmap::fromImage(noiseTexture));
    p.restore();

    // 绘制选中项高亮
    if (selectedItem) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xc8, 0xcf, 0xcd, 192));

        int x = (width() - iconSize) / 2;
        QRect r(x, highlightPosY, iconSize, iconSize);

        p.drawRoundedRect(r, 10, 10);
    }
}

void ApplicationBar::addItem(ApplicationBarItem* item) {
    topItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::itemClicked, this, &ApplicationBar::onItemClicked);
    layoutItems();
}

void ApplicationBar::layoutItems() {
    int w = width();

    // 设置头像位置
    int avatarX = (w - avatarSize) / 2;
    int avatarY = marginTop + spacing;

    userHead->move(avatarX, avatarY);

    // 设置其他项的位置
    int y = marginTop + spacing + avatarSize + avatarAndItemDist;

    for (auto* item : topItems) {
        int x = (w - iconSize) / 2;

        item->setGeometry(x, y, iconSize, iconSize);
        y += iconSize + spacing;
    }

    int yb = height() - marginBottom - iconSize;

    for (auto* item : bottomItems) {
        int x = (w - iconSize) / 2;

        item->setGeometry(x, yb, iconSize, iconSize);
        yb -= (iconSize + spacing);
    }
}

void ApplicationBar::onItemClicked(ApplicationBarItem* item) {
    if (selectedItem == item)
        return;


    if (selectedItem)
        selectedItem->setSelected(false);
    item->setSelected(true);

    int startY = highlightPosY;
    int endY = item->y();

    highlightAnim->stop();
    highlightAnim->setStartValue(startY);
    highlightAnim->setEndValue(endY);
    highlightAnim->start();
    selectedItem = item;

    emit applicationClicked(item);
}

void ApplicationBar::addBottomItem(ApplicationBarItem* item) {
    bottomItems.append(item);
    item->setParent(this);
    layoutItems();
}