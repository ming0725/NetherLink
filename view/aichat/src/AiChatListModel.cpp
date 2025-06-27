#include "AiChatListModel.h"
#include <QSize>

AiChatListModel::AiChatListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    // 初始化时添加底部空白
    ensureBottomSpace();
}

int AiChatListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant AiChatListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();

    const ListItem& item = m_items.at(index.row());
    
    if (role == Qt::UserRole) {
        if (item.isBottomSpace) {
            return QVariant::fromValue<int>(item.bottomSpaceHeight);
        } else {
            return QVariant::fromValue(item.message);
        }
    } else if (role == Qt::SizeHintRole && item.isBottomSpace) {
        return QSize(0, item.bottomSpaceHeight);
    }
    
    return QVariant();
}

bool AiChatListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_items.size())
        return false;

    if (role == Qt::UserRole + 1 && !m_items[index.row()].isBottomSpace) {
        m_items[index.row()].message->setSelected(value.toBool());
        emit dataChanged(index, index, {Qt::UserRole});
        return true;
    }
    return false;
}

void AiChatListModel::appendMessage(AiChatMessage* message)
{
    // 如果有底部空白，先移除它
    if (!m_items.empty() && m_items.back().isBottomSpace) {
        beginRemoveRows(QModelIndex(), m_items.size() - 1, m_items.size() - 1);
        m_items.pop_back();
        endRemoveRows();
    }

    // 添加消息
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    ListItem item;
    item.message = message;
    item.isBottomSpace = false;
    m_items.append(item);
    endInsertRows();

    // 确保底部空白存在
    ensureBottomSpace();
}

void AiChatListModel::appendContentToLastMessage(const QString& content)
{
    if (!m_items.isEmpty()) {
        int lastMessageIndex = -1;
        // 找到最后一条消息（跳过底部空白）
        for (int i = m_items.size() - 1; i >= 0; --i) {
            if (!m_items[i].isBottomSpace) {
                lastMessageIndex = i;
                break;
            }
        }
        
        if (lastMessageIndex >= 0) {
            m_items[lastMessageIndex].message->appendContent(content);
            QModelIndex lastIndex = index(lastMessageIndex);
            emit dataChanged(lastIndex, lastIndex, {Qt::UserRole});
        }
    }
}

void AiChatListModel::clearSelection()
{
    if (m_currentSelectedIndex.isValid()) {
        QModelIndex oldIndex = m_currentSelectedIndex;
        m_currentSelectedIndex = QModelIndex();
        if (oldIndex.row() < m_items.size() && !m_items[oldIndex.row()].isBottomSpace) {
            m_items[oldIndex.row()].message->setSelected(false);
            emit dataChanged(oldIndex, oldIndex, {Qt::UserRole});
        }
    }
}

void AiChatListModel::setSingleSelection(const QModelIndex& index)
{
    // 如果点击的是同一个项，不做任何处理
    if (m_currentSelectedIndex == index) {
        return;
    }

    // 清除之前的选中状态
    clearSelection();

    // 设置新的选中状态
    if (index.isValid() && index.row() < m_items.size() && !m_items[index.row()].isBottomSpace) {
        m_currentSelectedIndex = index;
        m_items[index.row()].message->setSelected(true);
        emit dataChanged(index, index, {Qt::UserRole});
    }
}

AiChatMessage* AiChatListModel::messageAt(int row) const
{
    if (row >= 0 && row < m_items.size() && !m_items[row].isBottomSpace)
        return m_items[row].message;
    return nullptr;
}

bool AiChatListModel::isBottomSpace(int index) const
{
    if (index >= 0 && index < m_items.size())
        return m_items[index].isBottomSpace;
    return false;
}

void AiChatListModel::ensureBottomSpace()
{
    // 如果列表为空或最后一项不是底部空白，添加底部空白
    if (m_items.isEmpty() || !m_items.back().isBottomSpace) {
        beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
        ListItem item;
        item.isBottomSpace = true;
        item.bottomSpaceHeight = AiBottomSpace::DEFAULT_HEIGHT;
        m_items.append(item);
        endInsertRows();
    }
}

void AiChatListModel::setBottomSpaceHeight(int height)
{
    // 如果有底部空白，更新其高度
    if (!m_items.isEmpty() && m_items.back().isBottomSpace) {
        m_items.back().bottomSpaceHeight = height;
        QModelIndex lastIndex = index(m_items.size() - 1);
        emit dataChanged(lastIndex, lastIndex, {Qt::UserRole, Qt::SizeHintRole});
    }
} 