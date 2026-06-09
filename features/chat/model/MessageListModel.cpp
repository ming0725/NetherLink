#include "MessageListModel.h"

#include <QSize>

#include <algorithm>

namespace {

QVector<int> changedRolesFor(const ConversationSummary& previous,
                             const ConversationSummary& next)
{
    QVector<int> roles;
    if (previous.title != next.title) {
        roles.push_back(MessageListModel::TitleRole);
        roles.push_back(Qt::DisplayRole);
    }
    if (previous.avatarPath != next.avatarPath) {
        roles.push_back(MessageListModel::AvatarPathRole);
    }
    if (previous.previewText != next.previewText) {
        roles.push_back(MessageListModel::PreviewTextRole);
    }
    if (previous.messageListTime != next.messageListTime) {
        roles.push_back(MessageListModel::LastTimeRole);
    }
    if (previous.unreadCount != next.unreadCount) {
        roles.push_back(MessageListModel::UnreadCountRole);
    }
    if (previous.isDoNotDisturb != next.isDoNotDisturb) {
        roles.push_back(MessageListModel::DoNotDisturbRole);
    }
    if (previous.isPinned != next.isPinned) {
        roles.push_back(MessageListModel::IsPinnedRole);
    }
    if (previous.isGroup != next.isGroup) {
        roles.push_back(MessageListModel::IsGroupRole);
    }
    if (previous.memberCount != next.memberCount) {
        roles.push_back(MessageListModel::MemberCountRole);
    }
    return roles;
}

bool hasSameConversationOrder(const QVector<ConversationSummary>& previous,
                              const QVector<ConversationSummary>& next)
{
    if (previous.size() != next.size()) {
        return false;
    }
    for (int row = 0; row < previous.size(); ++row) {
        if (previous.at(row).conversationId != next.at(row).conversationId) {
            return false;
        }
    }
    return true;
}

} // namespace

MessageListModel::MessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_conversations.size());
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }

    const ConversationSummary& conversation = m_conversations.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return conversation.title;
    case ConversationIdRole:
        return conversation.conversationId;
    case AvatarPathRole:
        return conversation.avatarPath;
    case PreviewTextRole:
        return conversation.previewText;
    case LastTimeRole:
        return conversation.messageListTime;
    case UnreadCountRole:
        return conversation.unreadCount;
    case DoNotDisturbRole:
        return conversation.isDoNotDisturb;
    case IsPinnedRole:
        return conversation.isPinned;
    case IsGroupRole:
        return conversation.isGroup;
    case MemberCountRole:
        return conversation.memberCount;
    case ContextMenuActiveRole:
        return !m_contextMenuConversationId.isEmpty() &&
               conversation.conversationId == m_contextMenuConversationId;
    case Qt::SizeHintRole:
        return QSize(0, 72);
    default:
        return {};
    }
}

Qt::ItemFlags MessageListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void MessageListModel::setConversations(QVector<ConversationSummary> conversations)
{
    sortConversationVector(conversations);
    if (hasSameConversationOrder(m_conversations, conversations)) {
        for (int row = 0; row < conversations.size(); ++row) {
            const QVector<int> roles = changedRolesFor(m_conversations.at(row), conversations.at(row));
            if (roles.isEmpty()) {
                continue;
            }
            m_conversations[row] = conversations.at(row);
            const QModelIndex modelIndex = index(row, 0);
            emit dataChanged(modelIndex, modelIndex, roles);
        }
        return;
    }

    beginResetModel();
    m_conversations = std::move(conversations);
    endResetModel();
}

void MessageListModel::markConversationRead(const QString& conversationId)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    if (m_conversations[row].unreadCount == 0) {
        return;
    }

    m_conversations[row].unreadCount = 0;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {UnreadCountRole});
}

void MessageListModel::markConversationUnread(const QString& conversationId, int unreadCount)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    const int nextUnreadCount = qMax(1, unreadCount);
    if (m_conversations[row].unreadCount == nextUnreadCount) {
        return;
    }

    m_conversations[row].unreadCount = nextUnreadCount;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {UnreadCountRole});
}

void MessageListModel::setConversationDoNotDisturb(const QString& conversationId, bool enabled)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    if (m_conversations[row].isDoNotDisturb == enabled) {
        return;
    }

    m_conversations[row].isDoNotDisturb = enabled;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {DoNotDisturbRole, UnreadCountRole});
}

void MessageListModel::setConversationPinned(const QString& conversationId, bool pinned)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    if (m_conversations[row].isPinned == pinned) {
        return;
    }

    beginResetModel();
    m_conversations[row].isPinned = pinned;
    sortConversations();
    endResetModel();
}

void MessageListModel::setContextMenuConversation(const QString& conversationId)
{
    if (m_contextMenuConversationId == conversationId) {
        return;
    }

    const QString previousId = m_contextMenuConversationId;
    m_contextMenuConversationId = conversationId;

    const int previousRow = indexOfConversation(previousId);
    if (previousRow >= 0) {
        const QModelIndex modelIndex = index(previousRow, 0);
        emit dataChanged(modelIndex, modelIndex, {ContextMenuActiveRole});
    }

    const int currentRow = indexOfConversation(m_contextMenuConversationId);
    if (currentRow >= 0) {
        const QModelIndex modelIndex = index(currentRow, 0);
        emit dataChanged(modelIndex, modelIndex, {ContextMenuActiveRole});
    }
}

bool MessageListModel::removeConversation(const QString& conversationId)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_conversations.removeAt(row);
    if (m_contextMenuConversationId == conversationId) {
        m_contextMenuConversationId.clear();
    }
    endRemoveRows();
    return true;
}

void MessageListModel::updateConversationPreview(const QString& conversationId,
                                                 const QString& previewText,
                                                 const QDateTime& lastMessageTime)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    m_conversations[row].previewText = previewText;
    m_conversations[row].lastMessageTime = lastMessageTime;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {PreviewTextRole});
}

QString MessageListModel::conversationIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }
    return m_conversations.at(index.row()).conversationId;
}

ConversationSummary MessageListModel::conversationAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }
    return m_conversations.at(index.row());
}

ConversationSummary MessageListModel::conversationById(const QString& conversationId) const
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return {};
    }
    return m_conversations.at(row);
}

int MessageListModel::indexOfConversation(const QString& conversationId) const
{
    for (int row = 0; row < m_conversations.size(); ++row) {
        if (m_conversations.at(row).conversationId == conversationId) {
            return row;
        }
    }
    return -1;
}

void MessageListModel::sortConversations()
{
    sortConversationVector(m_conversations);
}

void MessageListModel::sortConversationVector(QVector<ConversationSummary>& conversations)
{
    std::sort(conversations.begin(), conversations.end(),
              [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
                  if (lhs.isPinned != rhs.isPinned) {
                      return lhs.isPinned;
                  }
                  return lhs.messageListTime > rhs.messageListTime;
              });
}
