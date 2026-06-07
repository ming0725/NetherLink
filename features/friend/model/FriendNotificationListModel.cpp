#include "FriendNotificationListModel.h"

#include <QSize>
#include <QUuid>

#include "features/friend/data/UserRepository.h"

namespace {

QString notificationLookupId(const FriendNotification& notification)
{
    return notification.fromUserId.isEmpty() ? notification.fromUserUuid : notification.fromUserId;
}

QString readableUserId(const User& user, const FriendNotification& notification)
{
    if (!user.userId.isEmpty()) {
        return user.userId;
    }
    if (!notification.fromUserId.isEmpty() && QUuid::fromString(notification.fromUserId).isNull()) {
        return notification.fromUserId;
    }
    return {};
}

QString displayNameWithReadableId(const User& user, const FriendNotification& notification)
{
    const QString publicId = readableUserId(user, notification);
    const QString name = user.remark.isEmpty()
            ? (user.nick.isEmpty() ? publicId : user.nick)
            : user.remark;
    if (name.isEmpty()) {
        return QStringLiteral("未知用户");
    }
    if (publicId.isEmpty() || name == publicId) {
        return name;
    }
    return QStringLiteral("%1（%2）").arg(name, publicId);
}

} // namespace

FriendNotificationListModel::FriendNotificationListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FriendNotificationListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_notifications.size()) + 1;
}

QVariant FriendNotificationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() > m_notifications.size()) {
        return {};
    }

    if (index.row() == m_notifications.size()) {
        if (role == BottomSpaceRole) {
            return true;
        }
        if (role == Qt::SizeHintRole) {
            return QSize(0, kBottomSpaceHeight);
        }
        return {};
    }

    const FriendNotification& n = m_notifications.at(index.row());
    switch (role) {
    case IdRole:
        return n.id;
    case FromUserIdRole:
        return n.fromUserId;
    case DisplayNameRole: {
        const User user = UserRepository::instance().requestUserDetail({notificationLookupId(n)});
        if (!user.id.isEmpty()) {
            return displayNameWithReadableId(user, n);
        }
        return n.fromUserId.isEmpty() ? QStringLiteral("未知用户") : n.fromUserId;
    }
    case AvatarPathRole:
        return UserRepository::instance().requestUserAvatarPath(notificationLookupId(n));
    case MessageRole:
        return n.message;
    case RequestDateRole:
        return n.requestDate;
    case SourceTextRole:
        return n.sourceText();
    case StatusRole:
        return static_cast<int>(n.status);
    case HoveredButtonRole:
        return m_hoveredButtons.value(n.id, kNoHoveredButton);
    case BottomSpaceRole:
        return false;
    default:
        return {};
    }
}

void FriendNotificationListModel::setNotifications(QVector<FriendNotification> notifications)
{
    beginResetModel();
    m_notifications = std::move(notifications);
    m_hoveredButtons.clear();
    endResetModel();
}

void FriendNotificationListModel::appendNotifications(QVector<FriendNotification> notifications)
{
    if (notifications.isEmpty()) {
        return;
    }

    const int firstRow = m_notifications.size();
    beginInsertRows(QModelIndex(), firstRow, firstRow + notifications.size() - 1);
    m_notifications += std::move(notifications);
    endInsertRows();
}

void FriendNotificationListModel::setHoveredButton(const QModelIndex& index, int buttonIndex)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_notifications.size()) {
        return;
    }

    const QString& id = m_notifications.at(index.row()).id;
    int& current = m_hoveredButtons[id];
    if (current == buttonIndex) {
        return;
    }
    current = buttonIndex;
    emit dataChanged(index, index, {HoveredButtonRole});
}

FriendNotification FriendNotificationListModel::notificationAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_notifications.size()) {
        return {};
    }
    return m_notifications.at(index.row());
}

int FriendNotificationListModel::notificationCount() const
{
    return m_notifications.size();
}

bool FriendNotificationListModel::isBottomSpace(const QModelIndex& index) const
{
    return index.isValid() && index.row() == m_notifications.size();
}
