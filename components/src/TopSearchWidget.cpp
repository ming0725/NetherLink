#include "TopSearchWidget.h"
#include <QPainter>
#include <QAction>
#include "../view/friend/include/SearchFriendWindow.h"

TopSearchWidget::TopSearchWidget(QWidget *parent)
    : QWidget(parent)
{
    searchBox = new LineEditComponent(this);
    addButton = new QPushButton("+", this);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setStyleSheet(R"(
        QPushButton {
            background-color: #F5F5F5;
            border: none;
            border-radius: 8px;
            font-size: 18px;
        }
        QPushButton:pressed {
            background-color: #D7D7D7;
        }
        QPushButton:hover {
            background-color: #EBEBEB;
        }
    )");
    searchBox->setFixedHeight(26);
    addButton->setFixedHeight(26);
    setFixedHeight(topMargin + searchBox->height() + bottomMargin);

    // 连接按钮点击信号到显示菜单的槽
    connect(addButton, &QPushButton::clicked, this, &TopSearchWidget::showAddMenu);
}

void TopSearchWidget::showAddMenu()
{
    // 每次点击时创建新的菜单
    auto* menu = new TransparentMenu(this);
    QIcon icon(":/resources/icon/friend_selected.png");
    
    QAction* createGroupAction = new QAction(icon, "创建群聊", menu);
    QAction* addFriendAction = new QAction(icon, "添加好友/群", menu);
    
    menu->addAction(createGroupAction);
    menu->addAction(addFriendAction);

    // 连接添加好友/群的点击事件
    connect(addFriendAction, &QAction::triggered, this, []() {
        SearchFriendWindow::getInstance()->show();
    });

    // 修正菜单弹出位置
    QPoint buttonPos = addButton->mapToGlobal(addButton->rect().bottomLeft());
    menu->popup(QPoint(buttonPos.x(), buttonPos.y() + 2)); // 添加2像素的偏移，避免紧贴按钮

    // 菜单关闭后自动删除
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
}

void TopSearchWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    int W = width();
    int H = height();
    int contentH = H - topMargin - bottomMargin;
    int y = topMargin;

    // 1. 先放 addButton：右侧留白 rightMargin
    int btnX = W - rightMargin - contentH;
    addButton->setGeometry(btnX, y, contentH, contentH);

    // 2. 再放 searchBox：从 leftMargin 到 addButton 左边，减去 spacing
    int editX = leftMargin;
    int editW = btnX - spacing - leftMargin;
    searchBox->setGeometry(editX, y, editW, contentH);
}

void TopSearchWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFFFFFF));
    painter.drawRect(rect());
}