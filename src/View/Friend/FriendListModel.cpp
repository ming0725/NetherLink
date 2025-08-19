/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QCollator>
#include <QSize>

#include "Data/UserRepository.h"
#include "Network/NetworkManager.h"
#include "View/Friend/FriendListModel.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

static bool userLessThan(const User& a, const User& b) {
    // 非离线的排前面
    if ((a.status != Offline) && (b.status == Offline))
        return (true);

    if ((a.status == Offline) && (b.status != Offline))
        return (false);

    static QCollator collator;

    collator.setLocale(QLocale::Chinese);
    collator.setNumericMode(true);

    return (collator.compare(a.nick, b.nick) < 0);
}

FriendListModel::FriendListModel(QObject* parent) : QAbstractListModel(parent) {

    connect(&NetworkManager::instance(), &NetworkManager::friendRequestAccepted, this, &FriendListModel::reloadUsers);

    reloadUsers();
}

int FriendListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return users.size();
}

QVariant FriendListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= users.size())
        return QVariant();

    const User& user = users[index.row()];

    switch (role) {
        case Qt::DisplayRole:
            return user.nick;
        case Qt::UserRole:
            return QVariant::fromValue(user);
        case Qt::UserRole + 1: // 选中状态
            return index.row() == selectedIndex;
        case Qt::SizeHintRole:
            return QSize(144, 72); // 设置每个item的大小
        default:
            return QVariant();
    }
}

bool FriendListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() >= users.size())
        return false;

    switch (role) {
        case Qt::UserRole + 1: // 选中状态
            if (value.toBool()) {
                setSelectedIndex(index.row());
            } else if (selectedIndex == index.row()) {
                clearSelection();
            }
            return true;
        default:
            return false;
    }
}

Qt::ItemFlags FriendListModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void FriendListModel::addUser(const User& user) {
    beginInsertRows(QModelIndex(), users.size(), users.size());
    users.append(user);
    endInsertRows();
    sortUsers();
}

void FriendListModel::removeUser(int index) {
    if (index < 0 || index >= users.size())
        return;

    beginRemoveRows(QModelIndex(), index, index);
    users.removeAt(index);
    endRemoveRows();

    // 调整选中索引
    if (selectedIndex == index) {
        clearSelection();
    } else if (selectedIndex > index) {
        selectedIndex--;
    }
}

void FriendListModel::clear() {
    beginResetModel();
    users.clear();
    selectedIndex = -1;
    endResetModel();
}

void FriendListModel::reloadUsers() {
    beginResetModel();
    users = UserRepository::instance().getAllUser();
    selectedIndex = -1;
    endResetModel();
    sortUsers();
}

const User* FriendListModel::userAt(int index) const {
    if (index < 0 || index >= users.size())
        return nullptr;
    return &users[index];
}

void FriendListModel::setSelectedIndex(int index) {
    if (index == selectedIndex)
        return;

    int oldIndex = selectedIndex;
    selectedIndex = index;

    // 通知视图更新
    if (oldIndex >= 0 && oldIndex < users.size()) {
        QModelIndex oldModelIndex = this->index(oldIndex);
        emit dataChanged(oldModelIndex, oldModelIndex, {Qt::UserRole + 1});
    }
    if (selectedIndex >= 0 && selectedIndex < users.size()) {
        QModelIndex newModelIndex = this->index(selectedIndex);
        emit dataChanged(newModelIndex, newModelIndex, {Qt::UserRole + 1});
    }
}

int FriendListModel::getSelectedIndex() const {
    return selectedIndex;
}

void FriendListModel::clearSelection() {
    setSelectedIndex(-1);
}

void FriendListModel::sortUsers() {
    if (users.isEmpty())
        return;

    beginResetModel();
    std::sort(users.begin(), users.end(), userLessThan);
    endResetModel();
} 
