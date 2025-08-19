/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QVBoxLayout>

#include "View/Friend/FriendListModel.h"
#include "View/Friend/FriendItemDelegate.h"
#include "View/Friend/FriendListView.h"
#include "View/Friend/FriendListWidget.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

FriendListWidget::FriendListWidget(QWidget* parent) : QWidget(parent), model(new FriendListModel(this)), delegate(new FriendItemDelegate(this)), listView(new FriendListView(this)) {
    // 设置背景为白色
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);

    // 设置模型和代理
    listView->setModel(model);
    listView->setItemDelegate(delegate);

    // 连接信号
    connect(delegate, &FriendItemDelegate::itemClicked, this, &FriendListWidget::onItemClicked);

    // 设置布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(listView);
}

FriendListWidget::~FriendListWidget() {}

void FriendListWidget::addItem(const User& user) {
    model->addUser(user);
}

void FriendListWidget::removeItemAt(int index) {
    model->removeUser(index);
}

void FriendListWidget::reloadFriendList() {
    model->reloadUsers();
}

void FriendListWidget::onItemClicked(int index) {
    // 处理项目点击，设置选中状态
    if (model->getSelectedIndex() == index) {
        model->clearSelection();
    } else {
        model->setSelectedIndex(index);
    }
}
