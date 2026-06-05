#include "MessageRepository.h"

#include <QMetaObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>
#include <QTime>
#include <QUuid>

#include <algorithm>
#include <random>

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/ConversationRemoteDataSource.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/network/AppEventBus.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"

namespace {

constexpr int kFriendConversationSampleCount = 8;
constexpr int kGroupConversationSampleCount = 6;

QString userNameForIdentity(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    return currentUser.isCurrentUserId(userId)
            ? currentUser.getUserName()
            : UserRepository::instance().requestUserName(userId);
}

GroupRole groupRoleForUser(const Group& group, const QString& userId)
{
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

struct SampleParticipant {
    QString userId;
    QString displayName;
    GroupRole role = GroupRole::Member;
};

void appendGroupParticipant(QVector<SampleParticipant>& participants,
                            QSet<QString>& seen,
                            const Group& group,
                            const QString& userId)
{
    if (userId.isEmpty() || seen.contains(userId)) {
        return;
    }

    const CurrentUser& currentUser = CurrentUser::instance();
    QString displayName = group.memberNicknames.value(userId).trimmed();
    if (displayName.isEmpty() && currentUser.isCurrentUserId(userId) &&
        !group.currentUserNickname.trimmed().isEmpty()) {
        displayName = group.currentUserNickname.trimmed();
    }
    if (displayName.isEmpty() && currentUser.isCurrentUserId(userId)) {
        displayName = currentUser.getUserName();
    }
    if (displayName.isEmpty()) {
        displayName = userNameForIdentity(userId);
    }
    if (displayName.isEmpty()) {
        displayName = userId;
    }

    participants.push_back(SampleParticipant{
            userId,
            displayName,
            groupRoleForUser(group, userId)
    });
    seen.insert(userId);
}

bool isKnownNonFriendUser(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        return false;
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    return !user.id.isEmpty() && !user.isFriend;
}

bool isGroupSystemEventMessage(const QSharedPointer<ChatMessage>& message)
{
    if (!message) {
        return false;
    }
    return message->getType() == MessageType::GroupMemberJoined ||
           message->getType() == MessageType::GroupSystemEvent;
}

QVector<SampleParticipant> sampleParticipantsForGroup(const Group& group, int ordinal)
{
    QVector<SampleParticipant> candidates;
    QSet<QString> seen;
    const CurrentUser& currentUser = CurrentUser::instance();

    appendGroupParticipant(candidates, seen, group, currentUser.getUserId());
    appendGroupParticipant(candidates, seen, group, group.ownerId);
    for (const QString& adminId : group.adminsID) {
        appendGroupParticipant(candidates, seen, group, adminId);
    }
    for (const QString& memberId : group.membersID) {
        appendGroupParticipant(candidates, seen, group, memberId);
    }

    if (candidates.size() <= 2) {
        return candidates;
    }

    const SampleParticipant currentUserParticipant = candidates.takeFirst();
    std::mt19937 generator(20240521 + ordinal * 97);
    std::shuffle(candidates.begin(), candidates.end(), generator);

    QVector<SampleParticipant> activeParticipants;
    activeParticipants.reserve(qMin(7, candidates.size() + 1));
    activeParticipants.push_back(currentUserParticipant);

    const int targetCount = qMin(candidates.size() + 1, qBound(3, 3 + (ordinal % 5), 7));
    QSet<QString> activeUserIds;
    activeUserIds.insert(currentUserParticipant.userId);
    for (const SampleParticipant& candidate : candidates) {
        if (activeParticipants.size() >= targetCount) {
            break;
        }
        if (!isKnownNonFriendUser(candidate.userId)) {
            continue;
        }
        activeParticipants.push_back(candidate);
        activeUserIds.insert(candidate.userId);
        break;
    }
    for (int index = 0; index < candidates.size() && activeParticipants.size() < targetCount; ++index) {
        if (activeUserIds.contains(candidates.at(index).userId)) {
            continue;
        }
        activeParticipants.push_back(candidates.at(index));
        activeUserIds.insert(candidates.at(index).userId);
    }

    std::shuffle(activeParticipants.begin(), activeParticipants.end(), generator);
    return activeParticipants;
}

QString buildPreviewText(const QSharedPointer<ChatMessage>& message,
                         bool isGroup)
{
    if (!message) {
        return {};
    }

    if (message->getType() == MessageType::Recall) {
        return message->getContent();
    }

    if (isGroupSystemEventMessage(message)) {
        return message->getContent();
    }

    if (isGroup) {
        QString senderName = message->getSenderName();
        if (senderName.isEmpty()) {
            senderName = userNameForIdentity(message->getSenderId());
        }
        return QString("%1：%2").arg(senderName, message->getContent());
    }
    return message->getContent();
}

QString groupDisplayName(const Group& group)
{
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

QString userDisplayName(const User& user)
{
    return user.remark.isEmpty() ? user.nick : user.remark;
}

int sampleUnreadCount(int ordinal, int messageCount)
{
    switch (ordinal % 5) {
    case 0:
        return 3;
    case 1:
        return 18;
    case 2:
        return 128;
    case 3:
        return 7;
    default:
        return messageCount > 99 ? 101 : 12;
    }
}

QDateTime unreadMessageTimeAfter(const QVector<QDateTime>& timeline,
                                 int unreadIndex,
                                 int unreadCount)
{
    const QDateTime latestAllowedTime = QDateTime::currentDateTime().addSecs(-30);
    const QDateTime baseTime = timeline.isEmpty()
            ? latestAllowedTime.addSecs(-qMax(60, unreadCount * 60))
            : timeline.last();
    const qint64 availableSecs = qMax<qint64>(1, baseTime.secsTo(latestAllowedTime));
    const qint64 offsetSecs = qBound<qint64>(
            1,
            ((unreadIndex + 1) * availableSecs) / qMax(1, unreadCount + 1),
            availableSecs);
    const QDateTime timestamp = baseTime.addSecs(offsetSecs);
    return timestamp > latestAllowedTime ? latestAllowedTime : timestamp;
}

void appendSyntheticUnreadMessage(QVector<QSharedPointer<ChatMessage>>& messages,
                                  const QString& content,
                                  const QDateTime& timestamp,
                                  const QString& senderId,
                                  const QString& senderName,
                                  bool isGroupChat,
                                  GroupRole role)
{
    if (senderId.isEmpty()) {
        return;
    }

    auto msg = QSharedPointer<ChatMessage>(new TextMessage(
            content,
            false,
            senderId,
            isGroupChat,
            senderName,
            role));
    msg->setTimestamp(timestamp);
    messages.push_back(msg);
}

void appendSyntheticDirectUnreadMessages(QVector<QSharedPointer<ChatMessage>>& messages,
                                         const QVector<QDateTime>& timeline,
                                         const QString& peerId,
                                         const QString& peerName,
                                         int unreadCount)
{
    for (int index = 0; index < unreadCount; ++index) {
        appendSyntheticUnreadMessage(messages,
                                     QStringLiteral("未读消息%1").arg(index + 1),
                                     unreadMessageTimeAfter(timeline, index, unreadCount),
                                     peerId,
                                     peerName,
                                     false,
                                     GroupRole::Member);
    }
}

SampleParticipant syntheticUnreadSenderForGroup(const QVector<SampleParticipant>& participants)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    for (const SampleParticipant& participant : participants) {
        if (!currentUser.isCurrentUserId(participant.userId) &&
            isKnownNonFriendUser(participant.userId)) {
            return participant;
        }
    }
    for (const SampleParticipant& participant : participants) {
        if (!currentUser.isCurrentUserId(participant.userId)) {
            return participant;
        }
    }
    return {};
}

void appendSyntheticGroupUnreadMessages(QVector<QSharedPointer<ChatMessage>>& messages,
                                        const QVector<QDateTime>& timeline,
                                        const QVector<SampleParticipant>& participants,
                                        int unreadCount)
{
    const SampleParticipant sender = syntheticUnreadSenderForGroup(participants);
    if (sender.userId.isEmpty()) {
        return;
    }

    for (int index = 0; index < unreadCount; ++index) {
        appendSyntheticUnreadMessage(messages,
                                     QStringLiteral("未读消息%1").arg(index + 1),
                                     unreadMessageTimeAfter(timeline, index, unreadCount),
                                     sender.userId,
                                     sender.displayName,
                                     true,
                                     sender.role);
    }
}

void addLongGapReferenceSamples(QVector<QSharedPointer<ChatMessage>>& messages)
{
    const int messageCount = static_cast<int>(messages.size());
    if (messageCount < 42) {
        return;
    }

    struct ReferencePair {
        int referencedIndex = 0;
        int replyIndex = 0;
    };

    const QVector<ReferencePair> pairs = {
            {1, messageCount - 3},
            {qMax(2, messageCount / 5), messageCount - 12},
            {qMax(3, messageCount / 2 - 8), messageCount - 24}
    };

    QSet<int> usedReplyIndexes;
    for (const ReferencePair& pair : pairs) {
        if (pair.referencedIndex < 0 ||
                pair.referencedIndex >= messages.size() ||
                pair.replyIndex < 0 ||
                pair.replyIndex >= messages.size() ||
                pair.referencedIndex >= pair.replyIndex ||
                usedReplyIndexes.contains(pair.replyIndex)) {
            continue;
        }

        const QSharedPointer<ChatMessage>& referencedMessage = messages.at(pair.referencedIndex);
        const QSharedPointer<ChatMessage>& replyMessage = messages.at(pair.replyIndex);
        if (referencedMessage.isNull() || replyMessage.isNull()) {
            continue;
        }

        replyMessage->setReferencedMessageId(referencedMessage->getMessageId());
        usedReplyIndexes.insert(pair.replyIndex);
    }
}

bool sampleDoNotDisturb(int ordinal)
{
    return ordinal % 5 == 1 || ordinal % 7 == 3;
}

bool matchesConversationKeyword(const Group& group, const QString& keyword)
{
    return keyword.isEmpty() ||
           group.groupName.contains(keyword, Qt::CaseInsensitive) ||
           group.remark.contains(keyword, Qt::CaseInsensitive);
}

bool matchesConversationKeyword(const FriendSummary& friendSummary, const QString& keyword)
{
    return keyword.isEmpty() ||
           friendSummary.nickName.contains(keyword, Qt::CaseInsensitive) ||
           friendSummary.remark.contains(keyword, Qt::CaseInsensitive);
}

QDateTime dateTimeAt(const QDate& date, int hour, int minute)
{
    return QDateTime(date, QTime(hour, minute));
}

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QDateTime firstDateTime(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString text = object.value(key).toString();
        if (text.isEmpty()) {
            continue;
        }
        QDateTime value = QDateTime::fromString(text, Qt::ISODateWithMs);
        if (!value.isValid()) {
            value = QDateTime::fromString(text, Qt::ISODate);
        }
        if (value.isValid()) {
            return value;
        }
    }
    return {};
}

QString conversationIdFromMessageObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("conversationId"),
                                QStringLiteral("conversationID"),
                                QStringLiteral("chatId"),
                                QStringLiteral("roomId")});
}

QString messageIdFromObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("messageId"),
                                QStringLiteral("id"),
                                QStringLiteral("clientMessageId")});
}

QString clientMessageIdFromObject(const QJsonObject& object)
{
    return firstString(object, {QStringLiteral("clientMessageId"),
                                QStringLiteral("client_message_id")});
}

QString chatMessageCacheKey(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return {};
    }
    return conversationId + QLatin1Char(':') + messageId;
}

QString messageTypeToString(MessageType type)
{
    switch (type) {
    case MessageType::Image:
        return QStringLiteral("image");
    case MessageType::Recall:
        return QStringLiteral("recall");
    case MessageType::GroupMemberJoined:
        return QStringLiteral("group_member_joined");
    case MessageType::GroupSystemEvent:
        return QStringLiteral("group_system_event");
    case MessageType::File:
        return QStringLiteral("file");
    case MessageType::Voice:
        return QStringLiteral("voice");
    case MessageType::Text:
    default:
        return QStringLiteral("text");
    }
}

QString textContentFromMessageObject(const QJsonObject& object)
{
    const QJsonObject content = object.value(QStringLiteral("content")).toObject();
    const QString nestedText = content.value(QStringLiteral("text")).toString();
    if (!nestedText.isEmpty()) {
        return nestedText;
    }
    return firstString(object, {QStringLiteral("text"),
                                QStringLiteral("message"),
                                QStringLiteral("body"),
                                QStringLiteral("previewText")});
}

QString imageSourceFromMessageObject(const QJsonObject& object)
{
    const QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    for (const QJsonValue& value : attachments) {
        const QJsonObject attachment = value.toObject();
        const QString source = firstString(attachment, {QStringLiteral("url"),
                                                        QStringLiteral("fileUrl"),
                                                        QStringLiteral("thumbnailUrl"),
                                                        QStringLiteral("fileId")});
        if (!source.isEmpty()) {
            return source;
        }
    }
    const QJsonObject content = object.value(QStringLiteral("content")).toObject();
    return firstString(content, {QStringLiteral("url"),
                                 QStringLiteral("imageUrl"),
                                 QStringLiteral("fileId")});
}

GroupRole groupRoleForSender(const QString& conversationId, const QString& senderId)
{
    if (conversationId.isEmpty() || senderId.isEmpty()) {
        return GroupRole::Member;
    }
    const Group group = GroupRepository::instance().requestGroupDetail({conversationId});
    return group.groupId.isEmpty() ? GroupRole::Member : groupRoleForUser(group, senderId);
}

QSharedPointer<ChatMessage> chatMessageFromJson(const QJsonObject& object)
{
    const QString messageId = messageIdFromObject(object);
    const QString conversationId = conversationIdFromMessageObject(object);
    const QString senderId = firstString(object, {QStringLiteral("senderId"),
                                                  QStringLiteral("senderUuid"),
                                                  QStringLiteral("senderUserId"),
                                                  QStringLiteral("fromUserId"),
                                                  QStringLiteral("fromUserUuid"),
                                                  QStringLiteral("actorUserId"),
                                                  QStringLiteral("actorUuid")});
    const CurrentUser& currentUser = CurrentUser::instance();
    const bool isFromMe = !senderId.isEmpty() && currentUser.isCurrentUserId(senderId);
    const bool isGroupChat = object.value(QStringLiteral("isGroupChat")).toBool(GroupRepository::instance().contains(conversationId));
    QString senderName = firstString(object, {QStringLiteral("senderName"),
                                              QStringLiteral("senderNickName"),
                                              QStringLiteral("senderNickname"),
                                              QStringLiteral("fromUserName"),
                                              QStringLiteral("actorName")});
    if (senderName.isEmpty()) {
        senderName = userNameForIdentity(senderId);
    }
    const GroupRole role = isGroupChat ? groupRoleForSender(conversationId, senderId) : GroupRole::Member;

    QSharedPointer<ChatMessage> message;
    const QString type = object.value(QStringLiteral("type")).toString(QStringLiteral("text")).trimmed().toLower();
    if (type == QStringLiteral("image")) {
        message = QSharedPointer<ChatMessage>(new ImageMessage(imageSourceFromMessageObject(object),
                                                               isFromMe,
                                                               senderId,
                                                               isGroupChat,
                                                               senderName,
                                                               role));
    } else if (type == QStringLiteral("recall") || object.contains(QStringLiteral("recalledAt"))) {
        const QString displayText = firstString(object, {QStringLiteral("displayText"),
                                                         QStringLiteral("recallText")});
        message = QSharedPointer<ChatMessage>(new RecallMessage(displayText.isEmpty()
                                                                        ? QStringLiteral("消息已撤回")
                                                                        : displayText,
                                                                textContentFromMessageObject(object),
                                                                false,
                                                                isFromMe,
                                                                senderId,
                                                                isGroupChat,
                                                                senderName,
                                                                role));
    } else {
        message = QSharedPointer<ChatMessage>(new TextMessage(textContentFromMessageObject(object),
                                                              isFromMe,
                                                              senderId,
                                                              isGroupChat,
                                                              senderName,
                                                              role));
    }

    if (!messageId.isEmpty()) {
        message->setMessageId(messageId);
    }
    message->setClientMessageId(clientMessageIdFromObject(object));
    const QDateTime timestamp = firstDateTime(object, {QStringLiteral("createdAt"),
                                                       QStringLiteral("serverReceivedAt"),
                                                       QStringLiteral("clientSentAt"),
                                                       QStringLiteral("timestamp")});
    if (timestamp.isValid()) {
        message->setTimestamp(timestamp.toLocalTime());
    }
    message->setReferencedMessageId(firstString(object, {QStringLiteral("referencedMessageId"),
                                                         QStringLiteral("replyToMessageId")}));
    return message;
}

QJsonObject chatMessageToJson(const QString& conversationId, const QSharedPointer<ChatMessage>& message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return {};
    }

    QJsonObject content;
    if (message->getType() == MessageType::Text) {
        content.insert(QStringLiteral("text"), static_cast<const TextMessage*>(message.data())->getText());
    } else if (message->getType() == MessageType::Image) {
        content.insert(QStringLiteral("url"), static_cast<const ImageMessage*>(message.data())->getImageSource());
    } else if (message->getType() == MessageType::Recall) {
        content.insert(QStringLiteral("text"), static_cast<const RecallMessage*>(message.data())->getOriginalText());
    } else {
        content.insert(QStringLiteral("text"), message->getContent());
    }

    QJsonObject object{
            {QStringLiteral("messageId"), message->getMessageId()},
            {QStringLiteral("conversationId"), conversationId},
            {QStringLiteral("senderId"), message->getSenderId()},
            {QStringLiteral("senderName"), message->getSenderName()},
            {QStringLiteral("type"), messageTypeToString(message->getType())},
            {QStringLiteral("content"), content},
            {QStringLiteral("createdAt"), message->getTimestamp().toUTC().toString(Qt::ISODateWithMs)},
            {QStringLiteral("isGroupChat"), message->isInGroupChat()}
    };
    if (!message->getClientMessageId().isEmpty()) {
        object.insert(QStringLiteral("clientMessageId"), message->getClientMessageId());
    }
    if (!message->getReferencedMessageId().isEmpty()) {
        object.insert(QStringLiteral("referencedMessageId"), message->getReferencedMessageId());
    }
    if (message->getType() == MessageType::Recall) {
        object.insert(QStringLiteral("displayText"), message->getContent());
        object.insert(QStringLiteral("recalledAt"), message->getTimestamp().toUTC().toString(Qt::ISODateWithMs));
    }
    return object;
}

ConversationSyncState conversationSyncStateFromJson(const QJsonObject& object)
{
    QJsonObject syncState = object.value(QStringLiteral("syncState")).toObject();
    if (syncState.isEmpty()) {
        syncState = object;
    }

    ConversationSyncState state;
    state.conversationId = firstString(object, {QStringLiteral("conversationId"), QStringLiteral("id")});
    state.unreadCount = syncState.value(QStringLiteral("unreadCount")).toInt();
    state.isDoNotDisturb = syncState.value(QStringLiteral("isDnd")).toBool(
            syncState.value(QStringLiteral("isDoNotDisturb")).toBool());
    state.isPinned = syncState.value(QStringLiteral("isPinned")).toBool();
    state.messageListTime = firstDateTime(object, {QStringLiteral("lastMessageAt"),
                                                   QStringLiteral("updatedAt"),
                                                   QStringLiteral("createdAt")});
    state.lastReadAt = firstDateTime(syncState, {QStringLiteral("lastReadAt")});
    return state;
}

QJsonObject conversationObjectFromPayload(QJsonObject payload)
{
    QJsonObject object = payload.value(QStringLiteral("conversation")).toObject();
    if (object.isEmpty()) {
        object = payload;
    }
    const QString conversationId = firstString(payload, {QStringLiteral("conversationId"), QStringLiteral("id")});
    if (!conversationId.isEmpty() && !object.contains(QStringLiteral("conversationId"))) {
        object.insert(QStringLiteral("conversationId"), conversationId);
    }
    return object;
}

enum class SampleLatestBucket {
    Today,
    Yesterday,
    DaysAgo,
    OneMonthAgo,
    MonthsAgo,
    OneYearAgo,
    YearsAgo
};

SampleLatestBucket sampleLatestBucket(int ordinal)
{
    switch (ordinal % 7) {
    case 0:
        return SampleLatestBucket::Today;
    case 1:
        return SampleLatestBucket::Yesterday;
    case 2:
        return SampleLatestBucket::DaysAgo;
    case 3:
        return SampleLatestBucket::OneMonthAgo;
    case 4:
        return SampleLatestBucket::MonthsAgo;
    case 5:
        return SampleLatestBucket::OneYearAgo;
    default:
        return SampleLatestBucket::YearsAgo;
    }
}

int sampleHistoryMessageCount(int ordinal)
{
    return 50 + (ordinal * 37) % 151;
}

QDateTime sampleLatestMessageTime(int ordinal)
{
    const QDateTime now = QDateTime::currentDateTime();
    const int hourOffset = (ordinal % 3) * 2;
    const int minuteOffset = (ordinal % 4) * 3;

    switch (sampleLatestBucket(ordinal)) {
    case SampleLatestBucket::Today:
        return now.addSecs((-12 - ordinal % 9) * 60);
    case SampleLatestBucket::Yesterday:
        return dateTimeAt(now.date().addDays(-1), 18 + hourOffset, 10 + minuteOffset);
    case SampleLatestBucket::DaysAgo:
        return dateTimeAt(now.date().addDays(-3), 15 + hourOffset, 8 + minuteOffset);
    case SampleLatestBucket::OneMonthAgo:
        return dateTimeAt(now.date().addMonths(-1), 11 + hourOffset, 9 + minuteOffset);
    case SampleLatestBucket::MonthsAgo:
        return dateTimeAt(now.date().addMonths(-4), 10 + hourOffset, 18 + minuteOffset);
    case SampleLatestBucket::OneYearAgo:
        return dateTimeAt(now.date().addYears(-1), 9 + hourOffset, 6 + minuteOffset);
    case SampleLatestBucket::YearsAgo:
    default:
        return dateTimeAt(now.date().addYears(-2), 8 + hourOffset, 12 + minuteOffset);
    }
}

QVector<QDateTime> buildHistoryTimeline(int ordinal)
{
    const int messageCount = sampleHistoryMessageCount(ordinal);
    QVector<QDateTime> timeline;
    timeline.resize(messageCount);

    QDateTime cursor = sampleLatestMessageTime(ordinal);
    for (int reverseIndex = 0; reverseIndex < messageCount; ++reverseIndex) {
        timeline[messageCount - reverseIndex - 1] = cursor;

        int gapMinutes = 3 + ((ordinal + reverseIndex * 7) % 11);
        if (reverseIndex % 12 == 11) {
            gapMinutes += 35 + (ordinal + reverseIndex) % 75;
        }
        if (reverseIndex % 37 == 36) {
            gapMinutes += (24 + (ordinal + reverseIndex) % 48) * 60;
        }
        cursor = cursor.addSecs(-gapMinutes * 60);
    }

    return timeline;
}

QVector<FriendSummary> sampledFriendsForMessages()
{
    QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
    std::mt19937 generator(20240521);
    std::shuffle(friends.begin(), friends.end(), generator);
    friends.resize(qMin(kFriendConversationSampleCount, friends.size()));
    return friends;
}

class ConversationMessagesRequestOperation final
    : public RepositoryTemplate<ConversationMessagesRequest, ChatMessageList> {
public:
    explicit ConversationMessagesRequestOperation(const QMap<QString, ChatMessageList>& store)
        : m_store(store)
    {
    }

private:
    ChatMessageList doRequest(const ConversationMessagesRequest& query) const override
    {
        const auto it = m_store.constFind(query.conversationId);
        if (it == m_store.constEnd() || it.value().isEmpty()) {
            return {};
        }

        const ChatMessageList& messages = it.value();
        const int total = messages.size();
        const int offset = qBound(0, query.offsetFromLatest, total);
        const int end = total - offset;
        if (end <= 0 || query.limit == 0) {
            return {};
        }

        const int start = query.limit < 0
                ? 0
                : qMax(0, end - qMax(0, query.limit));
        return messages.mid(start, end - start);
    }

    const QMap<QString, ChatMessageList>& m_store;
};

class ConversationMetaRequestOperation final
    : public RepositoryTemplate<ConversationMetaRequest, ConversationMeta> {
public:
    explicit ConversationMetaRequestOperation(const QMap<QString, ConversationSyncState>& conversationStates)
        : m_conversationStates(conversationStates)
    {
    }

private:
    bool hasState(const QString& conversationId) const
    {
        return m_conversationStates.contains(conversationId);
    }

    ConversationSyncState stateFor(const QString& conversationId) const
    {
        ConversationSyncState state = m_conversationStates.value(conversationId);
        state.conversationId = conversationId;
        return state;
    }

    ConversationMeta doRequest(const ConversationMetaRequest& query) const override
    {
        if (GroupRepository::instance().contains(query.conversationId)) {
            const Group group = GroupRepository::instance().requestGroupDetail({query.conversationId});
            const ConversationSyncState state = stateFor(group.groupId);
            return ConversationMeta{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    true,
                    group.memberNum,
                    Offline,
                    hasState(group.groupId) ? state.isDoNotDisturb : group.isDnd,
                    state.isPinned
            };
        }

        const User user = UserRepository::instance().requestUserDetail({query.conversationId});
        const ConversationSyncState state = stateFor(user.id);
        return ConversationMeta{
                user.id,
                userDisplayName(user),
                user.avatarPath,
                false,
                0,
                user.status,
                hasState(user.id) ? state.isDoNotDisturb : user.isDnd,
                state.isPinned
        };
    }

    const QMap<QString, ConversationSyncState>& m_conversationStates;
};

class ConversationListRequestOperation final
    : public RepositoryTemplate<ConversationListRequest, QVector<ConversationSummary>> {
public:
    ConversationListRequestOperation(const QMap<QString, ChatMessageList>& store,
                                     const QMap<QString, ConversationSyncState>& conversationStates)
        : m_store(store)
        , m_conversationStates(conversationStates)
    {
    }

private:
    ConversationSyncState stateFor(const QString& conversationId) const
    {
        ConversationSyncState state = m_conversationStates.value(conversationId);
        state.conversationId = conversationId;
        return state;
    }

    QDateTime effectiveMessageListTime(const QString& conversationId,
                                       const QSharedPointer<ChatMessage>& lastMessage) const
    {
        const QDateTime messageTime = lastMessage ? lastMessage->getTimestamp() : QDateTime();
        const QDateTime recentTime = stateFor(conversationId).messageListTime;
        return recentTime > messageTime ? recentTime : messageTime;
    }

    QVector<ConversationSummary> doRequest(const ConversationListRequest& query) const override
    {
        QVector<ConversationSummary> result;
        QSet<QString> seenConversationIds;

        const QVector<Group> groups = GroupRepository::instance().requestGroupList();
        for (const Group& group : groups) {
            if (!matchesConversationKeyword(group, query.keyword)) {
                continue;
            }
            if (!m_store.contains(group.groupId) &&
                !m_conversationStates.contains(group.groupId)) {
                continue;
            }

            if (seenConversationIds.contains(group.groupId)) {
                continue;
            }

            seenConversationIds.insert(group.groupId);
            const auto messagesIt = m_store.constFind(group.groupId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(group.groupId);
            result.push_back(ConversationSummary{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    buildPreviewText(lastMessage, true),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(group.groupId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    true,
                    group.memberNum
            });
        }

        const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
        for (const FriendSummary& friendSummary : friends) {
            if (!matchesConversationKeyword(friendSummary, query.keyword)) {
                continue;
            }
            if (!m_store.contains(friendSummary.userId) &&
                !m_conversationStates.contains(friendSummary.userId)) {
                continue;
            }

            if (seenConversationIds.contains(friendSummary.userId)) {
                continue;
            }

            seenConversationIds.insert(friendSummary.userId);
            const auto messagesIt = m_store.constFind(friendSummary.userId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(friendSummary.userId);
            result.push_back(ConversationSummary{
                    friendSummary.userId,
                    friendSummary.displayName,
                    friendSummary.avatarPath,
                    buildPreviewText(lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(friendSummary.userId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    false,
                    0
            });
        }

        return result;
    }

    void onAfterRequest(const ConversationListRequest&, QVector<ConversationSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
            if (lhs.isPinned != rhs.isPinned) {
                return lhs.isPinned;
            }
            return lhs.messageListTime > rhs.messageListTime;
        });
    }

    const QMap<QString, ChatMessageList>& m_store;
    const QMap<QString, ConversationSyncState>& m_conversationStates;
};

} // namespace

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
    reloadFromStore();

    connect(&LocalDataStore::instance(),
            &LocalDataStore::activeAccountChanged,
            this,
            [this](const QString&) {
                reloadFromStore();
            });
    connect(&LocalDataStore::instance(),
            &LocalDataStore::domainChanged,
            this,
            [this](const QString& domain) {
                if (domain == QStringLiteral("chat_messages") ||
                    domain == QStringLiteral("conversations")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (type == QStringLiteral("chat.message.created") ||
                    type == QStringLiteral("chat.message.sent")) {
                    QJsonObject object = payload.value(QStringLiteral("message")).toObject();
                    if (object.isEmpty()) {
                        object = payload;
                    }
                    const QString conversationId = conversationIdFromMessageObject(object);
                    const QSharedPointer<ChatMessage> message = chatMessageFromJson(object);
                    if (conversationId.isEmpty() || message.isNull()) {
                        return;
                    }
                    addMessage(conversationId, message);

                    const QJsonObject conversation = conversationObjectFromPayload(payload);
                    const QString stateConversationId = firstString(conversation, {QStringLiteral("conversationId"), QStringLiteral("id")});
                    if (!stateConversationId.isEmpty()) {
                        LocalDataStore::instance().upsertValue(QStringLiteral("conversations"),
                                                               stateConversationId,
                                                               conversation);
                    }
                    return;
                }

                if (type == QStringLiteral("chat.conversation.updated") ||
                    type == QStringLiteral("chat.conversation.sync.updated") ||
                    type == QStringLiteral("chat.read.updated")) {
                    const QJsonObject conversation = conversationObjectFromPayload(payload);
                    const QString conversationId = firstString(conversation, {QStringLiteral("conversationId"), QStringLiteral("id")});
                    if (!conversationId.isEmpty()) {
                        LocalDataStore::instance().upsertValue(QStringLiteral("conversations"),
                                                               conversationId,
                                                               conversation);
                    }
                }
            });

    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::pinnedUpdated,
            this,
            [this](const QString&, const QString& conversationId, bool pinned) {
                setConversationPinned(conversationId, pinned);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::doNotDisturbUpdated,
            this,
            [this](const QString&, const QString& conversationId, bool enabled) {
                setConversationDoNotDisturb(conversationId, enabled);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::conversationHidden,
            this,
            [this](const QString&, const QString& conversationId) {
                removeConversation(conversationId);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::conversationMarkedRead,
            this,
            [this](const QString&, const QString& conversationId) {
                markConversationRead(conversationId);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::conversationMarkedUnread,
            this,
            [this](const QString&, const QString& conversationId) {
                markConversationUnread(conversationId);
            });
    connect(&ConversationRemoteDataSource::instance(),
            &ConversationRemoteDataSource::messagesCleared,
            this,
            [this](const QString&, const QString& conversationId) {
                clearConversationMessages(conversationId);
            });
}

MessageRepository& MessageRepository::instance()
{
    static MessageRepository repo;
    return repo;
}

void MessageRepository::reloadFromStore()
{
    QMap<QString, QVector<QSharedPointer<ChatMessage>>> nextStore;
    QMap<QString, ConversationSyncState> nextStates;

    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("conversations"))) {
        ConversationSyncState state = conversationSyncStateFromJson(object);
        if (state.conversationId.isEmpty()) {
            continue;
        }
        nextStates.insert(state.conversationId, state);

        const QJsonObject lastMessage = object.value(QStringLiteral("lastMessage")).toObject();
        if (!lastMessage.isEmpty()) {
            QJsonObject messageObject = lastMessage;
            if (!messageObject.contains(QStringLiteral("conversationId"))) {
                messageObject.insert(QStringLiteral("conversationId"), state.conversationId);
            }
            const QSharedPointer<ChatMessage> message = chatMessageFromJson(messageObject);
            if (!message.isNull()) {
                nextStore[state.conversationId].push_back(message);
            }
        }
    }

    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("chat_messages"))) {
        const QString conversationId = conversationIdFromMessageObject(object);
        const QSharedPointer<ChatMessage> message = chatMessageFromJson(object);
        if (conversationId.isEmpty() || message.isNull()) {
            continue;
        }

        QVector<QSharedPointer<ChatMessage>>& messages = nextStore[conversationId];
        const auto duplicate = std::find_if(messages.cbegin(), messages.cend(), [&message](const QSharedPointer<ChatMessage>& existing) {
            return existing &&
                   (existing->getMessageId() == message->getMessageId() ||
                    (!existing->getClientMessageId().isEmpty() &&
                     existing->getClientMessageId() == message->getClientMessageId()));
        });
        if (duplicate == messages.cend()) {
            messages.push_back(message);
        }
    }

    for (auto it = nextStore.begin(); it != nextStore.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), [](const QSharedPointer<ChatMessage>& lhs,
                                                           const QSharedPointer<ChatMessage>& rhs) {
            if (!lhs || !rhs) {
                return !rhs.isNull();
            }
            return lhs->getTimestamp() < rhs->getTimestamp();
        });
        if (!it.value().isEmpty()) {
            ConversationSyncState& state = nextStates[it.key()];
            state.conversationId = it.key();
            if (!state.messageListTime.isValid()) {
                state.messageListTime = it.value().last()->getTimestamp();
            }
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_store = nextStore;
        m_conversationStates = nextStates;
    }

    emit lastMessageChanged({}, {});
    emit conversationListChanged({});
}

QVector<ConversationSummary> MessageRepository::requestConversationList(const ConversationListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationListRequestOperation(m_store, m_conversationStates).request(query);
}

ChatMessageList MessageRepository::requestConversationMessages(const ConversationMessagesRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationMessagesRequestOperation(m_store).request(query);
}

QSharedPointer<ChatMessage> MessageRepository::requestMessageById(const QString& conversationId,
                                                                  const QString& messageId) const
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const auto it = m_store.constFind(conversationId);
    if (it == m_store.constEnd()) {
        return {};
    }

    for (const QSharedPointer<ChatMessage>& message : it.value()) {
        if (message && message->getMessageId() == messageId) {
            return message;
        }
    }
    return {};
}

ConversationMeta MessageRepository::requestConversationMeta(const ConversationMetaRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationMetaRequestOperation(m_conversationStates).request(query);
}

ConversationThreadData MessageRepository::requestConversationThread(const ConversationThreadRequest& query) const
{
    ConversationThreadData thread;
    thread.meta = requestConversationMeta({query.conversationId});
    {
        QMutexLocker locker(&m_mutex);
        const ChatMessageList allMessages = m_store.value(query.conversationId);
        thread.messages = ConversationMessagesRequestOperation(m_store).request({
                query.conversationId,
                query.offsetFromLatest,
                query.limit
        });
        thread.loadedMessageCount = qMin(allMessages.size(),
                                         qMax(0, query.offsetFromLatest) + thread.messages.size());
        thread.hasMoreBefore = thread.loadedMessageCount < allMessages.size();
        thread.unreadCount = m_conversationStates.value(query.conversationId).unreadCount;
    }
    return thread;
}

ConversationThreadData MessageRepository::requestConversationThreadUntilMessage(
        const ConversationThreadUntilMessageRequest& query) const
{
    ConversationThreadData thread;
    thread.meta = requestConversationMeta({query.conversationId});
    if (query.conversationId.isEmpty() || query.messageId.isEmpty()) {
        return thread;
    }

    QMutexLocker locker(&m_mutex);
    const ChatMessageList allMessages = m_store.value(query.conversationId);
    const int total = allMessages.size();
    const int offset = qBound(0, query.offsetFromLatest, total);
    const int end = total - offset;
    thread.unreadCount = m_conversationStates.value(query.conversationId).unreadCount;
    thread.loadedMessageCount = offset;
    thread.hasMoreBefore = offset < total;
    if (end <= 0) {
        return thread;
    }

    bool targetInUnloadedRange = false;
    for (int index = 0; index < end; ++index) {
        const QSharedPointer<ChatMessage>& message = allMessages.at(index);
        if (message && message->getMessageId() == query.messageId) {
            targetInUnloadedRange = true;
            break;
        }
    }
    if (!targetInUnloadedRange) {
        return thread;
    }

    thread.messages = allMessages.mid(0, end);
    thread.loadedMessageCount = total;
    thread.hasMoreBefore = false;
    return thread;
}

QString MessageRepository::requestConversationThreadAsync(const ConversationThreadRequest& query)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int unreadCountAtRequest = 0;
    {
        QMutexLocker locker(&m_mutex);
        unreadCountAtRequest = m_conversationStates.value(query.conversationId).unreadCount;
    }

    QThreadPool::globalInstance()->start(QRunnable::create([this, requestId, query, unreadCountAtRequest]() {
        ConversationThreadData thread = requestConversationThread(query);
        thread.unreadCount = unreadCountAtRequest;
        QMetaObject::invokeMethod(this, [this, requestId, thread]() {
            emit conversationThreadReady(requestId, thread);
        }, Qt::QueuedConnection);
    }));
    return requestId;
}

void MessageRepository::touchConversation(const QString& conversationId,
                                          const QDateTime& timestamp)
{
    if (conversationId.isEmpty() || !timestamp.isValid()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.messageListTime = timestamp;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::markConversationRead(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        changed = state.unreadCount != 0;
        state.unreadCount = 0;
        state.lastReadAt = QDateTime::currentDateTime();
    }
    if (changed) {
        emit conversationListChanged(conversationId);
    }
}

void MessageRepository::markConversationUnread(const QString& conversationId, int unreadCount)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.unreadCount = qMax(1, unreadCount);
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::setConversationDoNotDisturb(const QString& conversationId, bool enabled)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.isDoNotDisturb = enabled;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::setConversationPinned(const QString& conversationId, bool pinned)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        if (state.isPinned == pinned) {
            return;
        }
        state.isPinned = pinned;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::clearConversationMessages(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_store.remove(conversationId) > 0;
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        if (state.unreadCount != 0) {
            changed = true;
        }
        state.unreadCount = 0;
        state.lastReadAt = QDateTime::currentDateTime();
    }

    if (!changed) {
        return;
    }

    emit lastMessageChanged(conversationId, {});
    emit conversationListChanged(conversationId);
}

void MessageRepository::removeConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_store.remove(conversationId) > 0;
        changed = m_conversationStates.remove(conversationId) > 0 || changed;
    }

    if (!changed) {
        return;
    }

    emit lastMessageChanged(conversationId, {});
    emit conversationListChanged(conversationId);
}

void MessageRepository::addMessage(const QString& conversationId,
                                   QSharedPointer<ChatMessage> message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return;
    }

    bool added = false;
    {
        QMutexLocker locker(&m_mutex);
        QVector<QSharedPointer<ChatMessage>>& messages = m_store[conversationId];
        for (const QSharedPointer<ChatMessage>& existing : messages) {
            if (existing &&
                (existing->getMessageId() == message->getMessageId() ||
                 (!existing->getClientMessageId().isEmpty() &&
                  existing->getClientMessageId() == message->getClientMessageId()))) {
                return;
            }
        }
        messages.push_back(message);
        std::sort(messages.begin(), messages.end(), [](const QSharedPointer<ChatMessage>& lhs,
                                                       const QSharedPointer<ChatMessage>& rhs) {
            if (!lhs || !rhs) {
                return !rhs.isNull();
            }
            return lhs->getTimestamp() < rhs->getTimestamp();
        });
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.messageListTime = message ? message->getTimestamp() : QDateTime::currentDateTime();
        added = true;
    }

    if (!added) {
        return;
    }
    const QJsonObject object = chatMessageToJson(conversationId, message);
    const QString key = chatMessageCacheKey(conversationId, message->getMessageId());
    if (!object.isEmpty() && !key.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("chat_messages"), key, object);
    }
    emit lastMessageChanged(conversationId, message);
    emit conversationListChanged(conversationId);
}

void MessageRepository::refreshGroupMemberDisplayName(const QString& groupId,
                                                      const QString& userId,
                                                      const QString& displayName,
                                                      GroupRole role)
{
    if (groupId.isEmpty() || userId.isEmpty()) {
        return;
    }

    QSharedPointer<ChatMessage> lastMsg;
    bool changed = false;
    bool changedLast = false;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_store.find(groupId);
        if (it == m_store.end()) {
            return;
        }

        ChatMessageList& messages = it.value();
        for (int index = 0; index < messages.size(); ++index) {
            const QSharedPointer<ChatMessage>& message = messages.at(index);
            if (!message || !message->isInGroupChat()) {
                continue;
            }

            bool messageChanged = false;
            if (message->getSenderId() == userId &&
                (message->getSenderName() != displayName || message->getRole() != role)) {
                message->setSenderName(displayName);
                message->setRole(role);
                messageChanged = true;
            }

            if (message->getType() == MessageType::Recall) {
                auto* recallMessage = static_cast<RecallMessage*>(message.data());
                if (recallMessage->getActorId() == userId &&
                    (recallMessage->getActorName() != displayName || recallMessage->getActorRole() != role)) {
                    recallMessage->setActorName(displayName);
                    recallMessage->setActorRole(role);
                    messageChanged = true;
                }
            } else if (message->getType() == MessageType::GroupMemberJoined) {
                auto* joinedMessage = static_cast<GroupMemberJoinedMessage*>(message.data());
                if (joinedMessage->getMemberId() == userId && joinedMessage->getMemberName() != displayName) {
                    joinedMessage->setMemberName(displayName);
                    messageChanged = true;
                }
                if (joinedMessage->getInviterId() == userId && joinedMessage->getInviterName() != displayName) {
                    joinedMessage->setInviterName(displayName);
                    messageChanged = true;
                }
            } else if (message->getType() == MessageType::GroupSystemEvent) {
                auto* eventMessage = static_cast<GroupSystemEventMessage*>(message.data());
                if (eventMessage->getHighlightedUserId() == userId &&
                    eventMessage->getHighlightedName() != displayName) {
                    eventMessage->setHighlightedName(displayName);
                    messageChanged = true;
                }
            }

            if (messageChanged) {
                changed = true;
                changedLast = index == messages.size() - 1;
            }
        }
        if (changedLast && !messages.isEmpty()) {
            lastMsg = messages.last();
        }
    }

    if (!changed) {
        return;
    }

    if (changedLast) {
        emit lastMessageChanged(groupId, lastMsg);
    }
    emit conversationListChanged(groupId);
}

bool MessageRepository::replaceMessage(const QString& conversationId,
                                       const QSharedPointer<ChatMessage>& oldMessage,
                                       QSharedPointer<ChatMessage> newMessage)
{
    if (conversationId.isEmpty() || oldMessage.isNull() || newMessage.isNull()) {
        return false;
    }

    QSharedPointer<ChatMessage> lastMsg;
    bool replacedLast = false;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_store.find(conversationId);
        if (it == m_store.end()) {
            return false;
        }

        ChatMessageList& messages = it.value();
        for (int index = 0; index < messages.size(); ++index) {
            if (messages.at(index).data() != oldMessage.data()) {
                continue;
            }

            messages[index] = newMessage;
            replacedLast = index == messages.size() - 1;
            lastMsg = messages.isEmpty() ? QSharedPointer<ChatMessage>() : messages.last();
            if (replacedLast) {
                ConversationSyncState& state = m_conversationStates[conversationId];
                state.conversationId = conversationId;
                state.messageListTime = newMessage->getTimestamp();
            }
            break;
        }
    }

    if (lastMsg.isNull()) {
        return false;
    }

    if (replacedLast) {
        emit lastMessageChanged(conversationId, lastMsg);
    }
    emit conversationListChanged(conversationId);
    return true;
}

void MessageRepository::removeMessage(const QString& conversationId, int index)
{
    QSharedPointer<ChatMessage> lastMsg;
    {
        QMutexLocker locker(&m_mutex);
        auto& vec = m_store[conversationId];
        if (index >= 0 && index < vec.size()) {
            vec.removeAt(index);
        }
        if (!vec.isEmpty()) {
            lastMsg = vec.last();
            ConversationSyncState& state = m_conversationStates[conversationId];
            state.conversationId = conversationId;
            state.messageListTime = lastMsg->getTimestamp();
        } else {
            m_store.remove(conversationId);
        }
    }
    emit lastMessageChanged(conversationId, lastMsg);
    emit conversationListChanged(conversationId);
}

bool MessageRepository::removeMessage(const QString& conversationId,
                                      const QSharedPointer<ChatMessage>& message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return false;
    }

    int removeIndex = -1;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_store.constFind(conversationId);
        if (it == m_store.constEnd()) {
            return false;
        }

        const ChatMessageList& messages = it.value();
        for (int index = 0; index < messages.size(); ++index) {
            if (messages.at(index).data() == message.data()) {
                removeIndex = index;
                break;
            }
        }
    }

    if (removeIndex < 0) {
        return false;
    }

    removeMessage(conversationId, removeIndex);
    return true;
}
