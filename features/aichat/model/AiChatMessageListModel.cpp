#include "AiChatMessageListModel.h"

#include <QSize>
#include <QtGlobal>
#include <utility>

namespace {

bool isThinkingMessageId(const QString& messageId)
{
    if (messageId.startsWith(QStringLiteral("__ai_thinking_"))) {
        return true;
    }

    const bool isWorkRow = messageId.startsWith(QStringLiteral("__ai_work_"));
    const bool isTraceRow = messageId.startsWith(QStringLiteral("__ai_trace_"));
    if (!isWorkRow && !isTraceRow) {
        return false;
    }

    if (messageId.contains(QStringLiteral("_action_"))) {
        return false;
    }

    return messageId.contains(QStringLiteral("_tool_")) ||
            (isTraceRow && messageId.endsWith(QStringLiteral("_summary")));
}

bool isTraceMessageId(const QString& messageId)
{
    return messageId.startsWith(QStringLiteral("__ai_trace_"));
}

bool isTraceSummaryMessageId(const QString& messageId)
{
    return isTraceMessageId(messageId) && messageId.endsWith(QStringLiteral("_summary"));
}

bool isTraceDetailMessageId(const QString& messageId)
{
    return isTraceMessageId(messageId) && !isTraceSummaryMessageId(messageId);
}

bool isTransientMessageId(const QString& messageId)
{
    return messageId.startsWith(QStringLiteral("__ai_thinking_")) ||
            messageId.startsWith(QStringLiteral("__ai_work_")) ||
            messageId.startsWith(QStringLiteral("__ai_trace_")) ||
            messageId.startsWith(QStringLiteral("__ai_answer_segment_"));
}

} // namespace

AiChatMessageListModel::AiChatMessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AiChatMessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_messages.size() + 1;
}

QVariant AiChatMessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    if (isBottomSpace(index.row())) {
        switch (role) {
        case IsBottomSpaceRole:
            return true;
        case BottomSpaceHeightRole:
            return m_bottomSpaceHeight;
        case Qt::SizeHintRole:
            return QSize(0, m_bottomSpaceHeight);
        default:
            return {};
        }
    }

    const AiChatMessage message = messageAt(index);
    if (message.messageId.isEmpty()) {
        return {};
    }

    switch (role) {
    case IsBottomSpaceRole:
        return false;
    case MessageIdRole:
        return message.messageId;
    case ConversationIdRole:
        return message.conversationId;
    case Qt::DisplayRole:
    case TextRole:
        return message.text;
    case IsFromUserRole:
        return message.isFromUser;
    case TimeRole:
        return message.time;
    case IsThinkingRole:
        return isThinkingMessageId(message.messageId);
    case IsThinkingActiveRole:
        return m_activeThinkingMessageIds.contains(message.messageId);
    case TraceExpansionProgressRole:
        return isTraceDetailMessageId(message.messageId)
                ? m_traceExpansionProgress.value(message.messageId, 1.0)
                : 1.0;
    case IsTraceDetailRole:
        return isTraceDetailMessageId(message.messageId);
    default:
        return {};
    }
}

Qt::ItemFlags AiChatMessageListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled;
}

void AiChatMessageListModel::setMessages(QVector<AiChatMessage> messages)
{
    beginResetModel();
    m_messages = std::move(messages);
    m_activeThinkingMessageIds.clear();
    m_traceExpansionProgress.clear();
    endResetModel();
}

void AiChatMessageListModel::appendMessage(const AiChatMessage& message)
{
    if (message.messageId.isEmpty()) {
        return;
    }

    const int row = m_messages.size();
    beginInsertRows(QModelIndex(), row, row);
    m_messages.push_back(message);
    if (isThinkingMessageId(message.messageId)) {
        m_activeThinkingMessageIds.insert(message.messageId);
    }
    endInsertRows();
}

bool AiChatMessageListModel::insertMessageBefore(const AiChatMessage& message,
                                                 const QString& beforeMessageId)
{
    if (message.messageId.isEmpty() || beforeMessageId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId == beforeMessageId) {
            beginInsertRows(QModelIndex(), row, row);
            m_messages.insert(row, message);
            if (isThinkingMessageId(message.messageId)) {
                m_activeThinkingMessageIds.insert(message.messageId);
            }
            endInsertRows();
            return true;
        }
    }
    return false;
}

bool AiChatMessageListModel::removeMessage(const QString& messageId)
{
    if (messageId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId == messageId) {
            beginRemoveRows(QModelIndex(), row, row);
            m_activeThinkingMessageIds.remove(messageId);
            m_traceExpansionProgress.remove(messageId);
            m_messages.removeAt(row);
            endRemoveRows();
            return true;
        }
    }

    return false;
}

bool AiChatMessageListModel::removeMessageWithAdjacentTransientRows(const QString& messageId)
{
    if (messageId.isEmpty()) {
        return false;
    }

    int targetRow = -1;
    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId == messageId) {
            targetRow = row;
            break;
        }
    }
    if (targetRow < 0) {
        return false;
    }

    int firstRow = targetRow;
    while (firstRow > 0 &&
           isTransientMessageId(m_messages.at(firstRow - 1).messageId)) {
        --firstRow;
    }

    int lastRow = targetRow;
    while (lastRow + 1 < m_messages.size() &&
           isTransientMessageId(m_messages.at(lastRow + 1).messageId)) {
        ++lastRow;
    }

    beginRemoveRows(QModelIndex(), firstRow, lastRow);
    for (int row = firstRow; row <= lastRow; ++row) {
        m_activeThinkingMessageIds.remove(m_messages.at(row).messageId);
        m_traceExpansionProgress.remove(m_messages.at(row).messageId);
    }
    m_messages.remove(firstRow, lastRow - firstRow + 1);
    endRemoveRows();
    return true;
}

bool AiChatMessageListModel::replaceMessage(const QString& messageId, const AiChatMessage& replacement)
{
    if (messageId.isEmpty() || replacement.messageId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId == messageId) {
            const bool wasActive = m_activeThinkingMessageIds.remove(messageId);
            const bool hadTraceProgress = m_traceExpansionProgress.contains(messageId);
            const qreal previousTraceProgress = m_traceExpansionProgress.take(messageId);
            m_messages[row] = replacement;
            if (wasActive && isThinkingMessageId(replacement.messageId)) {
                m_activeThinkingMessageIds.insert(replacement.messageId);
            }
            if (isTraceDetailMessageId(replacement.messageId) &&
                    hadTraceProgress &&
                    previousTraceProgress < 0.999) {
                m_traceExpansionProgress.insert(replacement.messageId, previousTraceProgress);
            }
            const QModelIndex changedIndex = index(row, 0);
            emit dataChanged(changedIndex,
                             changedIndex,
                             QVector<int>{MessageIdRole,
                                          ConversationIdRole,
                                          Qt::DisplayRole,
                                          TextRole,
                                          IsFromUserRole,
                                          TimeRole,
                                          IsThinkingRole,
                                          IsThinkingActiveRole,
                                          TraceExpansionProgressRole,
                                          IsTraceDetailRole,
                                          Qt::SizeHintRole});
            return true;
        }
    }

    return false;
}

bool AiChatMessageListModel::updateMessageText(const QString& messageId, const QString& text)
{
    if (messageId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].messageId == messageId) {
            m_messages[row].text = text;
            const QModelIndex changedIndex = index(row, 0);
            emit dataChanged(changedIndex, changedIndex, QVector<int>{Qt::DisplayRole, TextRole, Qt::SizeHintRole});
            return true;
        }
    }

    return false;
}

void AiChatMessageListModel::setTransientMessages(const QString& messageIdPrefix,
                                                  const QVector<AiChatMessage>& messages,
                                                  const QSet<QString>& activeMessageIds,
                                                  const QString& beforeMessageId,
                                                  qreal traceExpansionProgress)
{
    if (messageIdPrefix.isEmpty()) {
        return;
    }

    const qreal boundedTraceProgress = qBound<qreal>(0.0, traceExpansionProgress, 1.0);
    auto addTransientMessageState = [this, &activeMessageIds, boundedTraceProgress](
                                            const AiChatMessage& message) {
        if (activeMessageIds.contains(message.messageId) &&
                isThinkingMessageId(message.messageId)) {
            m_activeThinkingMessageIds.insert(message.messageId);
        }
        if (isTraceDetailMessageId(message.messageId) &&
                boundedTraceProgress < 0.999) {
            m_traceExpansionProgress.insert(message.messageId, boundedTraceProgress);
        }
    };

    auto removePrefixedState = [this, &messageIdPrefix]() {
        for (auto it = m_activeThinkingMessageIds.begin();
             it != m_activeThinkingMessageIds.end();) {
            if (it->startsWith(messageIdPrefix)) {
                it = m_activeThinkingMessageIds.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_traceExpansionProgress.begin();
             it != m_traceExpansionProgress.end();) {
            if (it.key().startsWith(messageIdPrefix)) {
                it = m_traceExpansionProgress.erase(it);
            } else {
                ++it;
            }
        }
    };

    int firstExistingRow = -1;
    int lastExistingRow = -1;
    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId.startsWith(messageIdPrefix)) {
            if (firstExistingRow < 0) {
                firstExistingRow = row;
            }
            lastExistingRow = row;
        }
    }

    if (firstExistingRow >= 0) {
        for (int row = firstExistingRow; row <= lastExistingRow; ++row) {
            if (!m_messages.at(row).messageId.startsWith(messageIdPrefix)) {
                QVector<AiChatMessage> nextMessages;
                nextMessages.reserve(m_messages.size() + messages.size());
                int insertionRow = -1;
                for (const AiChatMessage& message : m_messages) {
                    if (message.messageId.startsWith(messageIdPrefix)) {
                        if (insertionRow < 0) {
                            insertionRow = nextMessages.size();
                        }
                        continue;
                    }
                    if (insertionRow < 0 &&
                            !beforeMessageId.isEmpty() &&
                            message.messageId == beforeMessageId) {
                        insertionRow = nextMessages.size();
                    }
                    nextMessages.push_back(message);
                }
                if (insertionRow < 0) {
                    insertionRow = nextMessages.size();
                }
                for (int index = 0; index < messages.size(); ++index) {
                    nextMessages.insert(insertionRow + index, messages.at(index));
                }

                beginResetModel();
                m_messages = std::move(nextMessages);
                removePrefixedState();
                for (const AiChatMessage& message : m_messages) {
                    if (message.messageId.startsWith(messageIdPrefix)) {
                        addTransientMessageState(message);
                    }
                }
                endResetModel();
                return;
            }
        }
    }

    int insertionRow = firstExistingRow;
    if (insertionRow < 0) {
        for (int row = 0; row < m_messages.size(); ++row) {
            if (!beforeMessageId.isEmpty() &&
                    m_messages.at(row).messageId == beforeMessageId) {
                insertionRow = row;
                break;
            }
        }
    }
    if (insertionRow < 0) {
        insertionRow = m_messages.size();
    }

    if (firstExistingRow >= 0) {
        beginRemoveRows(QModelIndex(), firstExistingRow, lastExistingRow);
        removePrefixedState();
        m_messages.remove(firstExistingRow, lastExistingRow - firstExistingRow + 1);
        endRemoveRows();
        insertionRow = firstExistingRow;
    } else {
        removePrefixedState();
    }

    if (messages.isEmpty()) {
        return;
    }

    insertionRow = qBound(0, insertionRow, m_messages.size());
    beginInsertRows(QModelIndex(), insertionRow, insertionRow + messages.size() - 1);
    for (int index = 0; index < messages.size(); ++index) {
        const AiChatMessage& message = messages.at(index);
        m_messages.insert(insertionRow + index, message);
        addTransientMessageState(message);
    }
    endInsertRows();
}

void AiChatMessageListModel::setTraceExpansionProgress(const QString& messageIdPrefix, qreal progress)
{
    if (messageIdPrefix.isEmpty()) {
        return;
    }

    const qreal boundedProgress = qBound<qreal>(0.0, progress, 1.0);
    int firstChangedRow = -1;
    int lastChangedRow = -1;
    bool changed = false;
    for (int row = 0; row < m_messages.size(); ++row) {
        const QString messageId = m_messages.at(row).messageId;
        if (!messageId.startsWith(messageIdPrefix) || !isTraceDetailMessageId(messageId)) {
            continue;
        }

        const qreal previousProgress = m_traceExpansionProgress.value(messageId, 1.0);
        if (qAbs(previousProgress - boundedProgress) <= 0.0001) {
            continue;
        }

        if (boundedProgress >= 0.999) {
            m_traceExpansionProgress.remove(messageId);
        } else {
            m_traceExpansionProgress.insert(messageId, boundedProgress);
        }
        firstChangedRow = firstChangedRow < 0 ? row : firstChangedRow;
        lastChangedRow = row;
        changed = true;
    }

    if (!changed || firstChangedRow < 0) {
        return;
    }

    emit dataChanged(index(firstChangedRow, 0),
                     index(lastChangedRow, 0),
                     QVector<int>{TraceExpansionProgressRole, Qt::SizeHintRole});
}

qreal AiChatMessageListModel::traceExpansionProgress(const QString& messageIdPrefix) const
{
    if (messageIdPrefix.isEmpty()) {
        return 0.0;
    }

    for (const AiChatMessage& message : m_messages) {
        if (message.messageId.startsWith(messageIdPrefix) &&
                isTraceDetailMessageId(message.messageId)) {
            return m_traceExpansionProgress.value(message.messageId, 1.0);
        }
    }
    return 0.0;
}

bool AiChatMessageListModel::setThinkingMessageActive(const QString& messageId, bool active)
{
    if (messageId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).messageId != messageId ||
                !isThinkingMessageId(messageId)) {
            continue;
        }

        const bool wasActive = m_activeThinkingMessageIds.contains(messageId);
        if (wasActive == active) {
            return true;
        }
        if (active) {
            m_activeThinkingMessageIds.insert(messageId);
        } else {
            m_activeThinkingMessageIds.remove(messageId);
        }
        const QModelIndex changedIndex = index(row, 0);
        emit dataChanged(changedIndex,
                         changedIndex,
                         QVector<int>{IsThinkingActiveRole});
        return true;
    }
    return false;
}

void AiChatMessageListModel::deactivateAllThinkingMessages()
{
    if (m_activeThinkingMessageIds.isEmpty()) {
        return;
    }

    m_activeThinkingMessageIds.clear();
    if (!m_messages.isEmpty()) {
        emit dataChanged(index(0, 0),
                         index(m_messages.size() - 1, 0),
                         QVector<int>{IsThinkingActiveRole});
    }
}

bool AiChatMessageListModel::hasActiveThinkingMessages() const
{
    return !m_activeThinkingMessageIds.isEmpty();
}

bool AiChatMessageListModel::isBottomSpace(int row) const
{
    return row == m_messages.size();
}

void AiChatMessageListModel::setBottomSpaceHeight(int height)
{
    const int nextHeight = qMax(0, height);
    if (m_bottomSpaceHeight == nextHeight) {
        return;
    }

    m_bottomSpaceHeight = nextHeight;
    const QModelIndex bottomIndex = index(m_messages.size(), 0);
    if (bottomIndex.isValid()) {
        emit dataChanged(bottomIndex,
                         bottomIndex,
                         QVector<int>{Qt::SizeHintRole, BottomSpaceHeightRole});
    }
}

AiChatMessage AiChatMessageListModel::messageAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return {};
    }
    return m_messages.at(index.row());
}

AiChatMessage AiChatMessageListModel::messageById(const QString& messageId) const
{
    if (messageId.isEmpty()) {
        return {};
    }

    for (const AiChatMessage& message : m_messages) {
        if (message.messageId == messageId) {
            return message;
        }
    }
    return {};
}

void AiChatMessageListModel::clear()
{
    beginResetModel();
    m_messages.clear();
    m_activeThinkingMessageIds.clear();
    m_traceExpansionProgress.clear();
    endResetModel();
}
