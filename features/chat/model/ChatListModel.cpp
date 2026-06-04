#include "ChatListModel.h"
#include <QDateTime>
#include <QSize>

Q_DECLARE_METATYPE(TimeHeader*)
Q_DECLARE_METATYPE(ChatMessage*)
Q_DECLARE_METATYPE(NewMessageDivider*)
Q_DECLARE_METATYPE(LoadingPlaceholder*)

namespace {

bool isGroupSystemEventMessage(const ChatMessage* message)
{
    if (!message) {
        return false;
    }
    return message->getType() == MessageType::GroupMemberJoined ||
           message->getType() == MessageType::GroupSystemEvent;
}

} // namespace

ChatListModel::ChatListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<TimeHeader*>();
    qRegisterMetaType<ChatMessage*>();
    qRegisterMetaType<NewMessageDivider*>();
    qRegisterMetaType<LoadingPlaceholder*>();
}

int ChatListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(items.size());
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items.size()))
        return QVariant();

    const ListItem& item = items[index.row()];
    
    if (role == Qt::UserRole) {
        if (item.isHeader) {
            return QVariant::fromValue<TimeHeader*>(item.timeHeader.get());
        } else if (item.isNewMessageDivider) {
            return QVariant::fromValue<NewMessageDivider*>(item.newMessageDivider.get());
        } else if (item.isLoadingPlaceholder) {
            return QVariant::fromValue<LoadingPlaceholder*>(item.loadingPlaceholder.get());
        } else if (item.isBottomSpace) {
            return QVariant::fromValue<int>(item.bottomSpaceHeight);
        } else {
            return QVariant::fromValue<ChatMessage*>(item.message.get());
        }
    } else if (role == Qt::UserRole + 2 &&
               !item.isHeader &&
               !item.isBottomSpace &&
               !item.isNewMessageDivider &&
               !item.isLoadingPlaceholder &&
               item.message) {
        return item.rowHighlighted;
    } else if (role == Qt::SizeHintRole && item.isBottomSpace) {
        return QSize(0, item.bottomSpaceHeight);
    }
    
    return QVariant();
}

bool ChatListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items.size()))
        return false;

    if (role == Qt::UserRole + 1 &&
        !items[index.row()].isHeader &&
        !items[index.row()].isBottomSpace &&
        !items[index.row()].isNewMessageDivider &&
        !items[index.row()].isLoadingPlaceholder &&
        items[index.row()].message) {
        bool selected = value.toBool();
        if (selected && selectedMessageIndex != index.row()) {
            // 清除之前选中的消息
            if (selectedMessageIndex >= 0) {
                items[selectedMessageIndex].message->setSelected(false);
                QModelIndex prevIndex = this->index(selectedMessageIndex);
                emit dataChanged(prevIndex, prevIndex);
            }
            // 设置新选中的消息
            items[index.row()].message->setSelected(true);
            selectedMessageIndex = index.row();
        } else if (!selected) {
            // 清除选中状态
            if (selectedMessageIndex >= 0) {
                items[selectedMessageIndex].message->setSelected(false);
                selectedMessageIndex = -1;
            }
        }
        emit dataChanged(index, index);
        return true;
    }
    if (role == Qt::UserRole + 2 &&
        !items[index.row()].isHeader &&
        !items[index.row()].isBottomSpace &&
        !items[index.row()].isNewMessageDivider &&
        !items[index.row()].isLoadingPlaceholder &&
        items[index.row()].message) {
        const bool highlighted = value.toBool();
        if (highlighted && highlightedRowIndex != index.row()) {
            clearRowHighlight();
            items[index.row()].rowHighlighted = true;
            highlightedRowIndex = index.row();
        } else if (!highlighted && highlightedRowIndex == index.row()) {
            clearRowHighlight();
            return true;
        } else if (!highlighted && !items[index.row()].rowHighlighted) {
            return true;
        } else {
            items[index.row()].rowHighlighted = highlighted;
            highlightedRowIndex = highlighted ? index.row() : -1;
        }
        emit dataChanged(index, index, {Qt::UserRole + 2});
        return true;
    }
    return false;
}

Qt::ItemFlags ChatListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ChatListModel::addMessage(QSharedPointer<ChatMessage> message)
{
    // 确保消息有有效的时间戳
    if (!message || !message->getTimestamp().isValid()) {
        return;
    }

    beginResetModel();
    messages.push_back(std::move(message));
    rebuildItems();
    endResetModel();
}

bool ChatListModel::replaceMessage(int index, QSharedPointer<ChatMessage> message)
{
    if (!message || index < 0 || index >= static_cast<int>(items.size())) {
        return false;
    }
    if (items[index].isBottomSpace ||
            items[index].isHeader ||
            items[index].isNewMessageDivider ||
            items[index].isLoadingPlaceholder) {
        return false;
    }

    const int messageIndex = messageIndexForRow(index);
    if (messageIndex < 0 || messageIndex >= messages.size()) {
        return false;
    }

    beginResetModel();
    messages[messageIndex] = std::move(message);
    selectedMessageIndex = -1;
    rebuildItems();
    endResetModel();
    return true;
}

void ChatListModel::notifyMessageChanged(const ChatMessage* message)
{
    const QModelIndex messageIndex = indexForMessage(message);
    if (!messageIndex.isValid()) {
        return;
    }
    emit dataChanged(messageIndex,
                     messageIndex,
                     {Qt::DisplayRole, Qt::SizeHintRole, Qt::UserRole, Qt::UserRole + 2});
}

void ChatListModel::setMessages(QVector<QSharedPointer<ChatMessage>> nextMessages)
{
    beginResetModel();
    messages = std::move(nextMessages);
    rebuildItems();
    endResetModel();
}

void ChatListModel::prependMessages(QVector<QSharedPointer<ChatMessage>> olderMessages)
{
    if (olderMessages.isEmpty()) {
        return;
    }

    beginResetModel();
    messages = olderMessages + messages;
    rebuildItems();
    endResetModel();
}

const ChatMessage* ChatListModel::messageAt(int index) const
{
    if (index >= 0 &&
        index < static_cast<int>(items.size()) &&
        !items[index].isHeader &&
        !items[index].isBottomSpace &&
        !items[index].isNewMessageDivider &&
        !items[index].isLoadingPlaceholder) {
        return items[index].message.get();
    }
    return nullptr;
}

QSharedPointer<ChatMessage> ChatListModel::sharedMessageAt(int index) const
{
    if (index >= 0 &&
        index < static_cast<int>(items.size()) &&
        !items[index].isHeader &&
        !items[index].isBottomSpace &&
        !items[index].isNewMessageDivider &&
        !items[index].isLoadingPlaceholder) {
        return items[index].message;
    }
    return {};
}

const ChatMessage* ChatListModel::messageById(const QString& messageId) const
{
    if (messageId.isEmpty()) {
        return nullptr;
    }

    for (const QSharedPointer<ChatMessage>& message : messages) {
        if (message && message->getMessageId() == messageId) {
            return message.get();
        }
    }
    return nullptr;
}

const ChatMessage* ChatListModel::messageByClientMessageId(const QString& clientMessageId) const
{
    if (clientMessageId.isEmpty()) {
        return nullptr;
    }

    for (const QSharedPointer<ChatMessage>& message : messages) {
        if (message && message->getClientMessageId() == clientMessageId) {
            return message.get();
        }
    }
    return nullptr;
}

QModelIndex ChatListModel::indexForMessageId(const QString& messageId) const
{
    if (messageId.isEmpty()) {
        return {};
    }

    for (int row = 0; row < items.size(); ++row) {
        if (!items.at(row).isHeader &&
                !items.at(row).isBottomSpace &&
                !items.at(row).isNewMessageDivider &&
                !items.at(row).isLoadingPlaceholder &&
                items.at(row).message &&
                items.at(row).message->getMessageId() == messageId) {
            return index(row, 0);
        }
    }
    return {};
}

void ChatListModel::clearSelection()
{
    if (selectedMessageIndex >= 0) {
        items[selectedMessageIndex].message->setSelected(false);
        QModelIndex index = this->index(selectedMessageIndex);
        selectedMessageIndex = -1;
        emit dataChanged(index, index);
    }
}

void ChatListModel::clearRowHighlight()
{
    if (highlightedRowIndex >= 0 && highlightedRowIndex < items.size()) {
        items[highlightedRowIndex].rowHighlighted = false;
        const QModelIndex highlightedIndex = index(highlightedRowIndex);
        highlightedRowIndex = -1;
        emit dataChanged(highlightedIndex, highlightedIndex, {Qt::UserRole + 2});
    } else {
        highlightedRowIndex = -1;
    }
}

bool ChatListModel::removeMessage(int index)
{
    if (index < 0 || index >= static_cast<int>(items.size())) {
        return false;
    }

    // 如果要删除的是底部空白或时间标识，直接返回
    if (items[index].isBottomSpace ||
            items[index].isHeader ||
            items[index].isNewMessageDivider ||
            items[index].isLoadingPlaceholder) {
        return false;
    }

    const int messageIndex = messageIndexForRow(index);
    if (messageIndex < 0 || messageIndex >= messages.size()) {
        return false;
    }

    beginResetModel();
    messages.removeAt(messageIndex);
    selectedMessageIndex = -1;
    highlightedRowIndex = -1;
    rebuildItems();
    endResetModel();
    return true;
}

bool ChatListModel::isTimeHeader(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size()))
        return items[index].isHeader;
    return false;
}

const TimeHeader* ChatListModel::getTimeHeader(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size()) && items[index].isHeader)
        return items[index].timeHeader.get();
    return nullptr;
}

bool ChatListModel::shouldAddTimeHeader(const QDateTime& prevTime, const QDateTime& currTime) const
{
    // 计算时间间隔（秒）
    qint64 interval = prevTime.secsTo(currTime);
    
    // 如果时间间隔为负数（可能是由于系统时间调整），则添加时间标识
    if (interval < 0) {
        interval = -interval;  // 使用绝对值
    }
    
    // 如果时间间隔超过设定值，则添加时间标识
    return interval >= TimeSettings::MESSAGE_TIME_INTERVAL;
}

TimeHeaderType ChatListModel::getTimeHeaderType(const QDateTime& timestamp) const
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 daysTo = timestamp.daysTo(now);

    if (daysTo == 0) {
        return TimeHeaderType::Time;
    } else if (daysTo == 1) {
        return TimeHeaderType::Yesterday;
    } else if (daysTo <= 7) {
        return TimeHeaderType::DayOfWeek;
    } else if (timestamp.date().year() == now.date().year()) {
        return TimeHeaderType::ThisYear;
    } else {
        return TimeHeaderType::FullDate;
    }
}

QString ChatListModel::formatTimeHeader(const QDateTime& timestamp) const
{
    QString timeStr = timestamp.toString("HH:mm");
    
    switch (getTimeHeaderType(timestamp)) {
        case TimeHeaderType::Time:
            return timeStr;
        case TimeHeaderType::Yesterday:
            return QString("昨天 %1").arg(timeStr);
        case TimeHeaderType::DayOfWeek: {
            static const QStringList weekDays = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
            return QString("%1 %2").arg(weekDays[timestamp.date().dayOfWeek() % 7], timeStr);
        }
        case TimeHeaderType::ThisYear:
            return timestamp.toString("MM-dd HH:mm");
        case TimeHeaderType::FullDate:
            return timestamp.toString("yyyy-MM-dd HH:mm");
    }
    return QString();
}

bool ChatListModel::isBottomSpace(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index].isBottomSpace;
    }
    return false;
}

bool ChatListModel::isNewMessageDivider(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index].isNewMessageDivider;
    }
    return false;
}

bool ChatListModel::isLoadingPlaceholder(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index].isLoadingPlaceholder;
    }
    return false;
}

QModelIndex ChatListModel::newMessageDividerIndex() const
{
    for (int row = 0; row < items.size(); ++row) {
        if (items.at(row).isNewMessageDivider) {
            return index(row, 0);
        }
    }
    return {};
}

QModelIndex ChatListModel::indexForMessage(const ChatMessage* message) const
{
    if (!message) {
        return {};
    }

    for (int row = 0; row < items.size(); ++row) {
        if (!items.at(row).isHeader &&
                !items.at(row).isBottomSpace &&
                !items.at(row).isNewMessageDivider &&
                !items.at(row).isLoadingPlaceholder &&
                items.at(row).message.get() == message) {
            return index(row, 0);
        }
    }
    return {};
}

int ChatListModel::nearestPeerMessageRowAtOrAfter(int row) const
{
    if (items.isEmpty()) {
        return -1;
    }

    const int boundedRow = qBound(0, row, items.size() - 1);
    return items.at(boundedRow).nearestPeerRowAfter;
}

int ChatListModel::nearestPeerMessageRowAtOrBefore(int row) const
{
    if (items.isEmpty()) {
        return -1;
    }

    const int boundedRow = qBound(0, row, items.size() - 1);
    return items.at(boundedRow).nearestPeerRowBefore;
}

void ChatListModel::ensureBottomSpace()
{
    // 如果没有底部空白，添加一个
    if (items.empty() || !items.back().isBottomSpace) {
        beginInsertRows(QModelIndex(), items.size(), items.size());
        ListItem bottomSpace;
        bottomSpace.isBottomSpace = true;
        bottomSpace.bottomSpaceHeight = bottomSpaceHeight;
        items.push_back(std::move(bottomSpace));
        refreshPeerMessageRows();
        endInsertRows();
    }
}

bool ChatListModel::setBottomSpaceHeight(int height)
{
    if (bottomSpaceHeight == height) {
        return false;
    }

    bottomSpaceHeight = height;
    // 更新底部空白的高度
    if (!items.empty() && items.back().isBottomSpace) {
        items.back().bottomSpaceHeight = bottomSpaceHeight;
        QModelIndex lastIndex = index(items.size() - 1, 0);
        emit dataChanged(lastIndex, lastIndex);
    }
    return true;
}

void ChatListModel::setNewMessageDividerBefore(const ChatMessage* message)
{
    if (newMessageDividerBefore == message) {
        return;
    }

    beginResetModel();
    newMessageDividerBefore = message;
    rebuildItems();
    endResetModel();
}

void ChatListModel::clearNewMessageDivider()
{
    if (!newMessageDividerBefore) {
        return;
    }

    beginResetModel();
    newMessageDividerBefore = nullptr;
    rebuildItems();
    endResetModel();
}

void ChatListModel::showLoadingPlaceholderAtTop()
{
    if (!items.isEmpty() && items.first().isLoadingPlaceholder) {
        return;
    }

    clearSelection();
    beginInsertRows(QModelIndex(), 0, 0);
    ListItem placeholderItem;
    placeholderItem.isLoadingPlaceholder = true;
    placeholderItem.loadingPlaceholder = QSharedPointer<LoadingPlaceholder>::create();
    placeholderItem.loadingPlaceholder->shimmerStartedAtMs = QDateTime::currentMSecsSinceEpoch();
    items.insert(0, std::move(placeholderItem));
    refreshPeerMessageRows();
    endInsertRows();
}

void ChatListModel::showInitialLoadingPlaceholders(int targetHeight)
{
    constexpr int kLoadingPlaceholderEstimatedHeight = 66;
    static constexpr bool kInitialLoadingPlaceholderDirections[] = {
        false,
        true,
        false,
        false,
        true,
    };
    constexpr int patternCount = static_cast<int>(
            sizeof(kInitialLoadingPlaceholderDirections) /
            sizeof(kInitialLoadingPlaceholderDirections[0]));
    const int placeholderCount = qMax(
            1,
            (qMax(0, targetHeight) + kLoadingPlaceholderEstimatedHeight - 1)
                    / kLoadingPlaceholderEstimatedHeight);

    beginResetModel();
    const qint64 shimmerStartedAtMs = QDateTime::currentMSecsSinceEpoch();
    items.clear();
    messages.clear();
    selectedMessageIndex = -1;
    highlightedRowIndex = -1;
    newMessageDividerBefore = nullptr;
    items.reserve(placeholderCount + 1);
    for (int i = 0; i < placeholderCount; ++i) {
        ListItem placeholderItem;
        placeholderItem.isLoadingPlaceholder = true;
        placeholderItem.loadingPlaceholder = QSharedPointer<LoadingPlaceholder>::create();
        placeholderItem.loadingPlaceholder->isFromMe =
                kInitialLoadingPlaceholderDirections[i % patternCount];
        placeholderItem.loadingPlaceholder->shimmerStartedAtMs = shimmerStartedAtMs;
        items.push_back(std::move(placeholderItem));
    }

    ListItem bottomSpace;
    bottomSpace.isBottomSpace = true;
    bottomSpace.bottomSpaceHeight = bottomSpaceHeight;
    items.push_back(std::move(bottomSpace));
    refreshPeerMessageRows();
    endResetModel();
}

void ChatListModel::removeLoadingPlaceholder()
{
    for (int row = 0; row < items.size(); ++row) {
        if (!items.at(row).isLoadingPlaceholder) {
            continue;
        }

        beginRemoveRows(QModelIndex(), row, row);
        items.removeAt(row);
        refreshPeerMessageRows();
        endRemoveRows();
        return;
    }
}

void ChatListModel::clear() {
    beginResetModel();
    items.clear();
    messages.clear();
    selectedMessageIndex = -1;
    highlightedRowIndex = -1;
    newMessageDividerBefore = nullptr;
    endResetModel();
}

void ChatListModel::rebuildItems()
{
    items.clear();
    selectedMessageIndex = -1;
    highlightedRowIndex = -1;

    QDateTime previousTime;
    bool dividerInserted = false;
    for (const QSharedPointer<ChatMessage>& message : messages) {
        if (!message || !message->getTimestamp().isValid()) {
            continue;
        }

        if (message.get() == newMessageDividerBefore) {
            ListItem dividerItem;
            dividerItem.isNewMessageDivider = true;
            dividerItem.newMessageDivider = QSharedPointer<NewMessageDivider>::create();
            dividerItem.newMessageDivider->text = QStringLiteral("新消息");
            items.push_back(std::move(dividerItem));
            dividerInserted = true;
        }

        const bool needTimeHeader = !previousTime.isValid() ||
                shouldAddTimeHeader(previousTime, message->getTimestamp());
        if (needTimeHeader) {
            ListItem timeItem;
            timeItem.isHeader = true;
            timeItem.timeHeader = QSharedPointer<TimeHeader>::create();
            timeItem.timeHeader->timestamp = message->getTimestamp();
            timeItem.timeHeader->type = getTimeHeaderType(message->getTimestamp());
            timeItem.timeHeader->text = formatTimeHeader(message->getTimestamp());
            items.push_back(std::move(timeItem));
        }

        ListItem messageItem;
        messageItem.message = message;
        items.push_back(std::move(messageItem));
        previousTime = message->getTimestamp();
    }

    ListItem bottomSpace;
    bottomSpace.isBottomSpace = true;
    bottomSpace.bottomSpaceHeight = bottomSpaceHeight;
    items.push_back(std::move(bottomSpace));

    if (newMessageDividerBefore && !dividerInserted) {
        newMessageDividerBefore = nullptr;
    }

    refreshPeerMessageRows();
}

void ChatListModel::refreshPeerMessageRows()
{
    int previousPeerRow = -1;
    for (int row = 0; row < items.size(); ++row) {
        if (!items.at(row).isHeader &&
                !items.at(row).isBottomSpace &&
                !items.at(row).isNewMessageDivider &&
                !items.at(row).isLoadingPlaceholder &&
                items.at(row).message &&
                !items.at(row).message->isFromMe() &&
                !isGroupSystemEventMessage(items.at(row).message.get())) {
            previousPeerRow = row;
        }
        items[row].nearestPeerRowBefore = previousPeerRow;
    }

    int nextPeerRow = -1;
    for (int row = items.size() - 1; row >= 0; --row) {
        if (!items.at(row).isHeader &&
                !items.at(row).isBottomSpace &&
                !items.at(row).isNewMessageDivider &&
                !items.at(row).isLoadingPlaceholder &&
                items.at(row).message &&
                !items.at(row).message->isFromMe() &&
                !isGroupSystemEventMessage(items.at(row).message.get())) {
            nextPeerRow = row;
        }
        items[row].nearestPeerRowAfter = nextPeerRow;
    }
}

int ChatListModel::messageIndexForRow(int row) const
{
    if (row < 0 ||
            row >= items.size() ||
            items.at(row).isHeader ||
            items.at(row).isBottomSpace ||
            items.at(row).isNewMessageDivider ||
            items.at(row).isLoadingPlaceholder) {
        return -1;
    }

    const ChatMessage* target = items.at(row).message.get();
    for (int index = 0; index < messages.size(); ++index) {
        if (messages.at(index).get() == target) {
            return index;
        }
    }
    return -1;
}
