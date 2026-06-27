#include "MessageRepository.h"

#include <QMetaObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#include <algorithm>

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/ChatRemoteDataSource.h"
#include "features/chat/data/ConversationRemoteDataSource.h"
#include "features/chat/data/MessageLocalDataSource.h"
#include "features/chat/data/MessageRemoteDataSource.h"
#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/network/AppEventBus.h"
#include "shared/network/ReferenceDataResolver.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"

namespace {

constexpr auto kLocalMessageListTimeKey = "localMessageListTime";

QString groupIdForConversationId(const QString& conversationId);
QDateTime deletionBarrierForConversation(const QString& conversationId,
                                         const QDateTime& remoteHiddenAt);
bool isHiddenByDeletionBarrier(const QJsonObject& object,
                               const QString& conversationId,
                               const QDateTime& barrier);

QString userNameForIdentity(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    return currentUser.isCurrentUserId(userId)
            ? currentUser.getUserName()
            : UserRepository::instance().requestUserName(userId);
}

QString groupMemberDisplayNameForMessage(const QString& conversationId, const QString& userId)
{
    if (userId.isEmpty()) {
        return {};
    }

    const QString groupId = groupIdForConversationId(conversationId);
    const CurrentUser& currentUser = CurrentUser::instance();
    const QString cachedNickname = GroupRepository::instance()
            .requestGroupMemberNickname(groupId, userId)
            .trimmed();
    if (!cachedNickname.isEmpty()) {
        return cachedNickname;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    const QString groupNickname = group.memberNicknames.value(userId).trimmed();
    if (!groupNickname.isEmpty()) {
        return groupNickname;
    }

    if (currentUser.isCurrentUserId(userId)) {
        return currentUser.getUserName();
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    if (!user.nick.trimmed().isEmpty()) {
        return user.nick.trimmed();
    }
    if (!user.userId.trimmed().isEmpty()) {
        return user.userId.trimmed();
    }
    return userId;
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

bool isGroupSystemEventMessage(const QSharedPointer<ChatMessage>& message)
{
    if (!message) {
        return false;
    }
    return message->getType() == MessageType::GroupMemberJoined ||
           message->getType() == MessageType::GroupSystemEvent;
}

QString buildPreviewText(const QString& conversationId,
                         const QSharedPointer<ChatMessage>& message,
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
        const QString senderName = groupMemberDisplayNameForMessage(conversationId,
                                                                    message->getSenderId());
        return QString("%1：%2").arg(senderName, message->getContent());
    }
    return message->getContent();
}

QString groupDisplayName(const Group& group)
{
    if (!group.remark.isEmpty()) {
        return group.remark;
    }
    if (!group.groupName.isEmpty()) {
        return group.groupName;
    }
    return group.groupPublicId.isEmpty() ? QStringLiteral("群聊") : group.groupPublicId;
}

QString userDisplayName(const User& user)
{
    if (!user.remark.isEmpty()) {
        return user.remark;
    }
    if (!user.nick.isEmpty()) {
        return user.nick;
    }
    return user.userId.isEmpty() ? user.id : user.userId;
}

bool matchesConversationKeyword(const Group& group, const QString& keyword)
{
    return keyword.isEmpty() ||
           group.groupPublicId.contains(keyword, Qt::CaseInsensitive) ||
           group.groupName.contains(keyword, Qt::CaseInsensitive) ||
           group.remark.contains(keyword, Qt::CaseInsensitive);
}

bool matchesConversationKeyword(const FriendSummary& friendSummary, const QString& keyword)
{
    return keyword.isEmpty() ||
           friendSummary.nickName.contains(keyword, Qt::CaseInsensitive) ||
           friendSummary.remark.contains(keyword, Qt::CaseInsensitive);
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

int messageSeqFromObject(const QJsonObject& object)
{
    int seq = object.value(QStringLiteral("messageSeq")).toInt();
    if (seq <= 0) {
        seq = object.value(QStringLiteral("seq")).toInt();
    }
    if (seq <= 0) {
        seq = object.value(QStringLiteral("serverSeq")).toInt();
    }
    return qMax(0, seq);
}

int conversationLastMessageSeq(const QString& conversationId)
{
    const QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    if (conversation.isEmpty()) {
        return 0;
    }

    return messageSeqFromObject(conversation.value(QStringLiteral("lastMessage")).toObject());
}

bool conversationTailHiddenByDeletionBarrier(const QString& conversationId,
                                             const QDateTime& remoteHiddenAt = {})
{
    if (conversationId.isEmpty()) {
        return false;
    }

    const QDateTime barrier = deletionBarrierForConversation(conversationId, remoteHiddenAt);
    if (!barrier.isValid()) {
        return false;
    }

    const QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    const QJsonObject lastMessage = conversation.value(QStringLiteral("lastMessage")).toObject();
    if (lastMessage.isEmpty()) {
        return true;
    }

    QJsonObject messageObject = lastMessage;
    if (!messageObject.contains(QStringLiteral("conversationId"))) {
        messageObject.insert(QStringLiteral("conversationId"), conversationId);
    }
    return isHiddenByDeletionBarrier(messageObject, conversationId, barrier);
}

QString chatMessageCacheKey(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return {};
    }
    return conversationId + QLatin1Char(':') + messageId;
}

QString messageFetchKey(const QString& conversationId, const QString& kind, int cursor = 0)
{
    return QStringLiteral("%1:%2:%3").arg(conversationId, kind, QString::number(qMax(0, cursor)));
}

bool isSameChatMessageIdentity(const QSharedPointer<ChatMessage>& lhs,
                               const QSharedPointer<ChatMessage>& rhs)
{
    if (!lhs || !rhs) {
        return false;
    }

    const bool sameMessageId = !lhs->getMessageId().isEmpty() &&
            !rhs->getMessageId().isEmpty() &&
            lhs->getMessageId() == rhs->getMessageId();
    const bool sameClientMessageId = !lhs->getClientMessageId().isEmpty() &&
            !rhs->getClientMessageId().isEmpty() &&
            lhs->getClientMessageId() == rhs->getClientMessageId();
    const bool sameMessageSeq = lhs->getMessageSeq() > 0 &&
            rhs->getMessageSeq() > 0 &&
            lhs->getMessageSeq() == rhs->getMessageSeq();
    return sameMessageId || sameClientMessageId || sameMessageSeq;
}

bool chatMessageLessThan(const QSharedPointer<ChatMessage>& lhs,
                         const QSharedPointer<ChatMessage>& rhs)
{
    if (!lhs || !rhs) {
        return !rhs.isNull();
    }
    if (lhs->getMessageSeq() > 0 &&
        rhs->getMessageSeq() > 0 &&
        lhs->getMessageSeq() != rhs->getMessageSeq()) {
        return lhs->getMessageSeq() < rhs->getMessageSeq();
    }
    if (lhs->getTimestamp() != rhs->getTimestamp()) {
        return lhs->getTimestamp() < rhs->getTimestamp();
    }
    return lhs->getMessageId() < rhs->getMessageId();
}

QDateTime localConversationClearTime(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }

    return MessageLocalDataSource::instance().conversationClearTime(conversationId);
}

int localConversationClearedThroughSeq(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return 0;
    }

    return MessageLocalDataSource::instance().conversationClearedThroughSeq(conversationId);
}

QDateTime deletionBarrierForConversation(const QString& conversationId,
                                         const QDateTime& remoteHiddenAt = {})
{
    QDateTime barrier = remoteHiddenAt;
    const QDateTime localClearedAt = localConversationClearTime(conversationId);
    if (localClearedAt.isValid() && (!barrier.isValid() || localClearedAt > barrier)) {
        barrier = localClearedAt;
    }
    return barrier;
}

bool isLocallyDeletedMessage(const QJsonObject& object)
{
    const QString conversationId = conversationIdFromMessageObject(object);
    if (conversationId.isEmpty()) {
        return false;
    }

    QStringList candidateIds;
    const QString messageId = messageIdFromObject(object);
    const QString clientMessageId = clientMessageIdFromObject(object);
    if (!messageId.isEmpty()) {
        candidateIds.push_back(messageId);
    }
    if (!clientMessageId.isEmpty() && clientMessageId != messageId) {
        candidateIds.push_back(clientMessageId);
    }

    return MessageLocalDataSource::instance().hasMessageDeletionMarker(conversationId, candidateIds);
}

bool isHiddenByDeletionBarrier(const QJsonObject& object,
                               const QString& conversationId,
                               const QDateTime& barrier)
{
    if (conversationId.isEmpty() || !barrier.isValid()) {
        return false;
    }

    QJsonObject messageObject = object;
    if (!messageObject.contains(QStringLiteral("conversationId"))) {
        messageObject.insert(QStringLiteral("conversationId"), conversationId);
    }
    const QDateTime messageTime = firstDateTime(messageObject,
                                                {QStringLiteral("createdAt"),
                                                 QStringLiteral("serverReceivedAt"),
                                                 QStringLiteral("clientSentAt"),
                                                 QStringLiteral("timestamp")});
    return messageTime.isValid() && messageTime <= barrier;
}

bool isLocallyVisibleMessageObject(const QJsonObject& object,
                                   const QString& conversationId,
                                   const QDateTime& remoteHiddenAt = {})
{
    QJsonObject messageObject = object;
    if (!conversationId.isEmpty() && !messageObject.contains(QStringLiteral("conversationId"))) {
        messageObject.insert(QStringLiteral("conversationId"), conversationId);
    }
    if (isLocallyDeletedMessage(messageObject)) {
        return false;
    }
    return !isHiddenByDeletionBarrier(messageObject,
                                      conversationIdFromMessageObject(messageObject),
                                      deletionBarrierForConversation(
                                              conversationIdFromMessageObject(messageObject),
                                              remoteHiddenAt));
}

void persistConversationClearMarker(const QString& conversationId,
                                    const QDateTime& clearedAt,
                                    int clearedThroughSeq)
{
    if (conversationId.isEmpty() || !clearedAt.isValid()) {
        return;
    }

    MessageLocalDataSource::instance().persistConversationClearMarker(conversationId,
                                                                      clearedAt,
                                                                      clearedThroughSeq);
}

void persistMessageDeletionMarker(const QString& conversationId,
                                  const QSharedPointer<ChatMessage>& message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return;
    }

    QStringList messageIds;
    if (!message->getMessageId().isEmpty()) {
        messageIds.push_back(message->getMessageId());
    }
    if (!message->getClientMessageId().isEmpty() &&
        message->getClientMessageId() != message->getMessageId()) {
        messageIds.push_back(message->getClientMessageId());
    }
    if (messageIds.isEmpty()) {
        return;
    }

    MessageLocalDataSource::instance().persistMessageDeletionMarkers(conversationId, messageIds);
}

void removePersistedMessagesForConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    MessageLocalDataSource::instance().removeMessagesForConversation(conversationId);
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

QString sendStateToString(MessageSendState state)
{
    switch (state) {
    case MessageSendState::Uploading:
        return QStringLiteral("uploading");
    case MessageSendState::Sending:
        return QStringLiteral("sending");
    case MessageSendState::Failed:
        return QStringLiteral("failed");
    case MessageSendState::Sent:
    default:
        return QStringLiteral("sent");
    }
}

MessageSendState sendStateFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("uploading")) {
        return MessageSendState::Uploading;
    }
    if (normalized == QStringLiteral("sending")) {
        return MessageSendState::Sending;
    }
    if (normalized == QStringLiteral("failed")) {
        return MessageSendState::Failed;
    }
    return MessageSendState::Sent;
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

QSize imageSizeFromMessageObject(const QJsonObject& object)
{
    auto sizeFromObject = [](const QJsonObject& source) {
        const int width = source.value(QStringLiteral("width")).toInt();
        const int height = source.value(QStringLiteral("height")).toInt();
        return width > 0 && height > 0 ? QSize(width, height) : QSize();
    };

    const QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    for (const QJsonValue& value : attachments) {
        const QSize size = sizeFromObject(value.toObject());
        if (size.isValid()) {
            return size;
        }
    }

    const QJsonObject content = object.value(QStringLiteral("content")).toObject();
    return sizeFromObject(content);
}

GroupRole groupRoleForSender(const QString& conversationId, const QString& senderId)
{
    if (conversationId.isEmpty() || senderId.isEmpty()) {
        return GroupRole::Member;
    }
    const Group group = GroupRepository::instance().requestGroupDetail({groupIdForConversationId(conversationId)});
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
    const User currentUserDetail = UserRepository::instance().requestUserDetail({currentUser.getUserId()});
    const bool isFromMe = !senderId.isEmpty() &&
            (currentUser.isCurrentUserId(senderId) ||
             (!currentUserDetail.userUuid.isEmpty() && senderId == currentUserDetail.userUuid) ||
             (!currentUserDetail.userId.isEmpty() && senderId == currentUserDetail.userId));
    const QString messageGroupId = firstString(object, {QStringLiteral("groupId")});
    const bool isGroupChat = object.value(QStringLiteral("isGroupChat")).toBool(
            !messageGroupId.isEmpty() || !groupIdForConversationId(conversationId).isEmpty());
    QString senderName = firstString(object, {QStringLiteral("senderName"),
                                              QStringLiteral("senderNickName"),
                                              QStringLiteral("senderNickname"),
                                              QStringLiteral("fromUserName"),
                                              QStringLiteral("actorName")});
    if (isGroupChat) {
        senderName = groupMemberDisplayNameForMessage(conversationId, senderId);
    } else if (senderName.isEmpty()) {
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
                                                               role,
                                                               imageSizeFromMessageObject(object)));
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
    message->setMessageSeq(messageSeqFromObject(object));
    message->setSendState(sendStateFromString(object.value(QStringLiteral("sendState")).toString()));
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
        const auto* imageMessage = static_cast<const ImageMessage*>(message.data());
        content.insert(QStringLiteral("url"), imageMessage->getImageSource());
        const QSize imageSize = imageMessage->getImageSize();
        if (imageSize.isValid()) {
            content.insert(QStringLiteral("width"), imageSize.width());
            content.insert(QStringLiteral("height"), imageSize.height());
        }
    } else if (message->getType() == MessageType::Recall) {
        content.insert(QStringLiteral("text"), static_cast<const RecallMessage*>(message.data())->getOriginalText());
    } else {
        content.insert(QStringLiteral("text"), message->getContent());
    }

    QJsonObject object{
            {QStringLiteral("messageId"), message->getMessageId()},
            {QStringLiteral("conversationId"), conversationId},
            {QStringLiteral("messageSeq"), message->getMessageSeq()},
            {QStringLiteral("senderId"), message->getSenderId()},
            {QStringLiteral("senderName"), message->getSenderName()},
            {QStringLiteral("type"), messageTypeToString(message->getType())},
            {QStringLiteral("content"), content},
            {QStringLiteral("createdAt"), message->getTimestamp().toUTC().toString(Qt::ISODateWithMs)},
            {QStringLiteral("isGroupChat"), message->isInGroupChat()}
    };
    if (message->isInGroupChat()) {
        const QString groupId = groupIdForConversationId(conversationId);
        if (!groupId.isEmpty()) {
            object.insert(QStringLiteral("groupId"), groupId);
        }
    }
    if (!message->getClientMessageId().isEmpty()) {
        object.insert(QStringLiteral("clientMessageId"), message->getClientMessageId());
    }
    if (!message->getReferencedMessageId().isEmpty()) {
        object.insert(QStringLiteral("referencedMessageId"), message->getReferencedMessageId());
    }
    if (message->getSendState() != MessageSendState::Sent) {
        object.insert(QStringLiteral("sendState"), sendStateToString(message->getSendState()));
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
    const QDateTime localMessageListTime =
            firstDateTime(object, {QString::fromLatin1(kLocalMessageListTimeKey)}).toLocalTime();
    const QDateTime remoteMessageListTime = firstDateTime(object, {QStringLiteral("lastMessageAt"),
                                                                   QStringLiteral("updatedAt"),
                                                                   QStringLiteral("createdAt")}).toLocalTime();
    state.messageListTime = localMessageListTime > remoteMessageListTime
            ? localMessageListTime
            : remoteMessageListTime;
    state.lastReadAt = firstDateTime(syncState, {QStringLiteral("lastReadAt")});
    state.hiddenAt = firstDateTime(syncState, {QStringLiteral("hiddenAt"),
                                               QStringLiteral("clearedAt"),
                                               QStringLiteral("messagesClearedAt"),
                                               QStringLiteral("deletedAt")});
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

bool conversationObjectHasUnreadCount(const QJsonObject& object)
{
    const QJsonObject syncState = object.value(QStringLiteral("syncState")).toObject();
    return object.contains(QStringLiteral("unreadCount")) ||
           syncState.contains(QStringLiteral("unreadCount"));
}

QJsonObject mergedConversationObject(QJsonObject existing, const QJsonObject& incoming)
{
    if (existing.isEmpty()) {
        return incoming;
    }

    QJsonObject merged = existing;
    for (auto it = incoming.constBegin(); it != incoming.constEnd(); ++it) {
        if ((it.key() == QStringLiteral("syncState") ||
             it.key() == QStringLiteral("summary")) &&
            it.value().isObject() &&
            merged.value(it.key()).isObject()) {
            QJsonObject nested = merged.value(it.key()).toObject();
            const QJsonObject incomingNested = it.value().toObject();
            for (auto nestedIt = incomingNested.constBegin(); nestedIt != incomingNested.constEnd(); ++nestedIt) {
                if ((nestedIt.key() == QStringLiteral("group") ||
                     nestedIt.key() == QStringLiteral("peerUser")) &&
                    nestedIt.value().isObject() &&
                    nested.value(nestedIt.key()).isObject()) {
                    QJsonObject child = nested.value(nestedIt.key()).toObject();
                    const QJsonObject incomingChild = nestedIt.value().toObject();
                    for (auto childIt = incomingChild.constBegin(); childIt != incomingChild.constEnd(); ++childIt) {
                        child.insert(childIt.key(), childIt.value());
                    }
                    nested.insert(nestedIt.key(), child);
                    continue;
                }
                nested.insert(nestedIt.key(), nestedIt.value());
            }
            merged.insert(it.key(), nested);
            continue;
        }
        merged.insert(it.key(), it.value());
    }
    return merged;
}

void setConversationUnreadCount(QJsonObject& object, int unreadCount)
{
    if (object.isEmpty()) {
        return;
    }

    object.insert(QStringLiteral("unreadCount"), unreadCount);
    if (object.value(QStringLiteral("syncState")).isObject()) {
        QJsonObject syncState = object.value(QStringLiteral("syncState")).toObject();
        syncState.insert(QStringLiteral("unreadCount"), unreadCount);
        object.insert(QStringLiteral("syncState"), syncState);
    }
}

QJsonObject directPeerUserObject(const QJsonObject& conversation)
{
    QJsonObject summary = conversation.value(QStringLiteral("summary")).toObject();
    QJsonObject peer = summary.value(QStringLiteral("peerUser")).toObject();
    if (!peer.isEmpty()) {
        return peer;
    }
    peer = conversation.value(QStringLiteral("peerUser")).toObject();
    if (!peer.isEmpty()) {
        return peer;
    }
    return conversation.value(QStringLiteral("user")).toObject();
}

QString directPeerIdFromConversation(const QJsonObject& conversation)
{
    const QString type = conversation.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QJsonObject peer = directPeerUserObject(conversation);
    if (type != QStringLiteral("direct") && peer.isEmpty()) {
        return {};
    }

    return firstString(peer, {QStringLiteral("userUuid"),
                              QStringLiteral("uuid"),
                              QStringLiteral("id"),
                              QStringLiteral("friendUserUuid"),
                              QStringLiteral("userId")});
}

QJsonObject directConversationObjectForPeer(QJsonObject conversation,
                                            const QString& conversationId,
                                            const QString& peerUserId)
{
    if (conversationId.isEmpty()) {
        return conversation;
    }

    if (!conversation.contains(QStringLiteral("conversationId")) &&
        !conversation.contains(QStringLiteral("id"))) {
        conversation.insert(QStringLiteral("conversationId"), conversationId);
    }
    if (!conversation.contains(QStringLiteral("type"))) {
        conversation.insert(QStringLiteral("type"), QStringLiteral("direct"));
    }

    QJsonObject summary = conversation.value(QStringLiteral("summary")).toObject();
    QJsonObject peer = summary.value(QStringLiteral("peerUser")).toObject();
    if (peer.isEmpty()) {
        peer = conversation.value(QStringLiteral("peerUser")).toObject();
    }
    if (!peer.isEmpty()) {
        summary.insert(QStringLiteral("peerUser"), peer);
        conversation.insert(QStringLiteral("summary"), summary);
        return conversation;
    }

    const User user = UserRepository::instance().requestUserDetail({peerUserId});
    const QString userUuid = user.userUuid.isEmpty()
            ? (user.id.isEmpty() ? peerUserId : user.id)
            : user.userUuid;
    if (!userUuid.isEmpty()) {
        peer.insert(QStringLiteral("userUuid"), userUuid);
    }
    if (!user.userId.isEmpty()) {
        peer.insert(QStringLiteral("userId"), user.userId);
    }
    if (!user.nick.isEmpty()) {
        peer.insert(QStringLiteral("nickName"), user.nick);
    }
    if (!user.avatarPath.isEmpty()) {
        peer.insert(QStringLiteral("avatarPath"), user.avatarPath);
    }
    if (user.version > 0) {
        peer.insert(QStringLiteral("version"), user.version);
    }
    if (peer.isEmpty() && !peerUserId.isEmpty()) {
        peer.insert(QStringLiteral("userUuid"), peerUserId);
    }
    if (!peer.isEmpty()) {
        summary.insert(QStringLiteral("peerUser"), peer);
        conversation.insert(QStringLiteral("summary"), summary);
    }

    return conversation;
}

QJsonObject groupObjectFromConversation(const QJsonObject& conversation)
{
    QJsonObject summary = conversation.value(QStringLiteral("summary")).toObject();
    QJsonObject group = summary.value(QStringLiteral("group")).toObject();
    if (!group.isEmpty()) {
        return group;
    }
    group = conversation.value(QStringLiteral("group")).toObject();
    if (!group.isEmpty()) {
        return group;
    }
    return {};
}

QString groupIdFromConversation(const QJsonObject& conversation)
{
    const QString type = conversation.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QJsonObject group = groupObjectFromConversation(conversation);
    if (type != QStringLiteral("group") && group.isEmpty()) {
        return {};
    }

    const QString nestedGroupId = firstString(group, {QStringLiteral("groupId"),
                                                      QStringLiteral("id")});
    if (!nestedGroupId.isEmpty()) {
        return nestedGroupId;
    }
    return firstString(conversation, {QStringLiteral("groupId")});
}

QJsonObject groupUpdateObjectFromConversation(const QJsonObject& conversation,
                                              const QString& fallbackGroupId = {})
{
    const QString type = conversation.value(QStringLiteral("type")).toString().trimmed().toLower();
    QJsonObject summary = conversation.value(QStringLiteral("summary")).toObject();
    QJsonObject group = groupObjectFromConversation(conversation);
    QString groupId = groupIdFromConversation(conversation);
    if (groupId.isEmpty()) {
        groupId = fallbackGroupId;
    }
    if (groupId.isEmpty() ||
        (type != QStringLiteral("group") && group.isEmpty() && fallbackGroupId.isEmpty())) {
        return {};
    }

    QJsonObject object = group;
    object.insert(QStringLiteral("groupId"), groupId);

    const QString groupPublicId = firstString(group, {QStringLiteral("groupPublicId"),
                                                      QStringLiteral("publicId"),
                                                      QStringLiteral("public_id")});
    if (!groupPublicId.isEmpty()) {
        object.insert(QStringLiteral("groupPublicId"), groupPublicId);
    }

    const QString title = firstString(summary, {QStringLiteral("title"),
                                                QStringLiteral("name"),
                                                QStringLiteral("groupName")});
    if (!title.isEmpty() &&
        !object.contains(QStringLiteral("groupName")) &&
        !object.contains(QStringLiteral("name"))) {
        object.insert(QStringLiteral("groupName"), title);
    }

    const QString avatarUrl = firstString(summary, {QStringLiteral("avatarUrl"),
                                                    QStringLiteral("groupAvatarPath")});
    if (!avatarUrl.isEmpty()) {
        object.insert(QStringLiteral("avatarUrl"), avatarUrl);
    }
    for (const QString& key : {QStringLiteral("avatarVersion"),
                               QStringLiteral("avatarEtag"),
                               QStringLiteral("avatarContentHash")}) {
        if (summary.contains(key)) {
            object.insert(key, summary.value(key));
        }
    }

    const int memberCount = summary.value(QStringLiteral("memberCount")).toInt();
    if (memberCount > 0 && !object.contains(QStringLiteral("memberCount"))) {
        object.insert(QStringLiteral("memberCount"), memberCount);
    }
    return object;
}

QString groupIdForConversationId(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return {};
    }
    if (GroupRepository::instance().contains(conversationId)) {
        return conversationId;
    }
    return groupIdFromConversation(MessageLocalDataSource::instance().conversation(conversationId));
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
    ConversationMetaRequestOperation(const QMap<QString, ConversationSyncState>& conversationStates,
                                     const QMap<QString, QString>& directConversationPeers,
                                     const QMap<QString, QString>& groupConversationGroups)
        : m_conversationStates(conversationStates)
        , m_directConversationPeers(directConversationPeers)
        , m_groupConversationGroups(groupConversationGroups)
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
        QString mappedGroupId = m_groupConversationGroups.value(query.conversationId);
        if (mappedGroupId.isEmpty()) {
            mappedGroupId = groupIdFromConversation(
                    MessageLocalDataSource::instance().conversation(query.conversationId));
        }
        const QString groupId = !mappedGroupId.isEmpty()
                ? mappedGroupId
                : (GroupRepository::instance().contains(query.conversationId) ? query.conversationId : QString());
        if (!groupId.isEmpty()) {
            const Group group = GroupRepository::instance().requestGroupDetail({groupId});
            const QString conversationId = query.conversationId;
            const ConversationSyncState state = stateFor(conversationId);
            return ConversationMeta{
                    conversationId,
                    group.groupId.isEmpty() ? QStringLiteral("群聊") : groupDisplayName(group),
                    group.groupAvatarPath,
                    true,
                    group.memberNum,
                    Offline,
                    hasState(conversationId) ? state.isDoNotDisturb : group.isDnd,
                    state.isPinned,
                    {},
                    group.groupId.isEmpty() ? groupId : group.groupId
            };
        }

        const QString peerId = m_directConversationPeers.value(query.conversationId, query.conversationId);
        const User user = UserRepository::instance().requestUserDetail({peerId});
        const QString conversationId = query.conversationId;
        const ConversationSyncState state = stateFor(conversationId);
        const QString title = user.id.isEmpty()
                ? query.conversationId
                : userDisplayName(user);
        return ConversationMeta{
                conversationId,
                title,
                user.avatarPath,
                false,
                0,
                user.status,
                hasState(conversationId) ? state.isDoNotDisturb : user.isDnd,
                state.isPinned,
                user.id.isEmpty() ? peerId : user.id,
                {}
        };
    }

    const QMap<QString, ConversationSyncState>& m_conversationStates;
    const QMap<QString, QString>& m_directConversationPeers;
    const QMap<QString, QString>& m_groupConversationGroups;
};

class ConversationListRequestOperation final
    : public RepositoryTemplate<ConversationListRequest, QVector<ConversationSummary>> {
public:
    ConversationListRequestOperation(const QMap<QString, ChatMessageList>& store,
                                     const QMap<QString, ConversationSyncState>& conversationStates,
                                     const QMap<QString, QString>& directConversationPeers,
                                     const QMap<QString, QString>& groupConversationGroups)
        : m_store(store)
        , m_conversationStates(conversationStates)
        , m_directConversationPeers(directConversationPeers)
        , m_groupConversationGroups(groupConversationGroups)
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
        QSet<QString> remoteDirectPeerIds;
        for (const QString& peerId : m_directConversationPeers) {
            if (!peerId.isEmpty()) {
                remoteDirectPeerIds.insert(peerId);
            }
        }

        const QVector<Group> groups = GroupRepository::instance().requestGroupList();
        for (const Group& group : groups) {
            if (!matchesConversationKeyword(group, query.keyword)) {
                continue;
            }

            QString conversationId;
            for (auto it = m_groupConversationGroups.cbegin(); it != m_groupConversationGroups.cend(); ++it) {
                if (it.value() == group.groupId &&
                    (m_store.contains(it.key()) || m_conversationStates.contains(it.key()))) {
                    conversationId = it.key();
                    break;
                }
            }
            if (conversationId.isEmpty() &&
                (m_store.contains(group.groupId) || m_conversationStates.contains(group.groupId))) {
                conversationId = group.groupId;
            }
            if (conversationId.isEmpty()) {
                continue;
            }

            if (seenConversationIds.contains(conversationId)) {
                continue;
            }

            seenConversationIds.insert(conversationId);
            const auto messagesIt = m_store.constFind(conversationId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(conversationId);
            result.push_back(ConversationSummary{
                    conversationId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    buildPreviewText(conversationId, lastMessage, true),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(conversationId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    true,
                    group.memberNum,
                    {},
                    group.groupId
            });
        }

        const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
        for (const FriendSummary& friendSummary : friends) {
            if (remoteDirectPeerIds.contains(friendSummary.userId)) {
                continue;
            }
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
                    buildPreviewText(friendSummary.userId, lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(friendSummary.userId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    false,
                    0,
                    friendSummary.userId,
                    {}
            });
        }

        for (auto it = m_directConversationPeers.cbegin(); it != m_directConversationPeers.cend(); ++it) {
            const QString& conversationId = it.key();
            const QString& peerId = it.value();
            if (conversationId.isEmpty() ||
                seenConversationIds.contains(conversationId) ||
                GroupRepository::instance().contains(conversationId) ||
                m_groupConversationGroups.contains(conversationId)) {
                continue;
            }
            const User user = UserRepository::instance().requestUserDetail({peerId});
            if (user.id.isEmpty()) {
                continue;
            }
            const QString displayName = userDisplayName(user);
            if (!query.keyword.isEmpty() &&
                !displayName.contains(query.keyword, Qt::CaseInsensitive) &&
                !user.userId.contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }
            const auto messagesIt = m_store.constFind(conversationId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(conversationId);
            result.push_back(ConversationSummary{
                    conversationId,
                    displayName,
                    user.avatarPath,
                    buildPreviewText(conversationId, lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(conversationId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    false,
                    0,
                    user.id.isEmpty() ? peerId : user.id,
                    {}
            });
            seenConversationIds.insert(conversationId);
        }

        for (const QString& conversationId : m_conversationStates.keys()) {
            if (seenConversationIds.contains(conversationId) ||
                GroupRepository::instance().contains(conversationId) ||
                m_groupConversationGroups.contains(conversationId)) {
                continue;
            }
            const User user = UserRepository::instance().requestUserDetail({conversationId});
            if (user.id.isEmpty()) {
                continue;
            }
            const QString displayName = userDisplayName(user);
            if (!query.keyword.isEmpty() &&
                !displayName.contains(query.keyword, Qt::CaseInsensitive) &&
                !user.userId.contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }
            const auto messagesIt = m_store.constFind(conversationId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(conversationId);
            result.push_back(ConversationSummary{
                    conversationId,
                    displayName,
                    user.avatarPath,
                    buildPreviewText(conversationId, lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(conversationId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    false,
                    0,
                    user.id.isEmpty() ? conversationId : user.id,
                    {}
            });
            seenConversationIds.insert(conversationId);
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
    const QMap<QString, QString>& m_directConversationPeers;
    const QMap<QString, QString>& m_groupConversationGroups;
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
                if (domain == MessageLocalDataSource::messagesDomain() ||
                    domain == MessageLocalDataSource::conversationsDomain()) {
                    if (shouldIgnoreStoreChange(domain)) {
                        return;
                    }
                    scheduleReloadFromStore();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (type == QStringLiteral("chat.message.created") ||
                    type == QStringLiteral("chat.message.sent")) {
                    ReferenceDataResolver::instance().consumePayload(payload);
                    QJsonObject object = payload.value(QStringLiteral("message")).toObject();
                    if (object.isEmpty()) {
                        object = payload;
                    }
                    QJsonObject conversation = conversationObjectFromPayload(payload);
                    const QString payloadGroupId = groupIdFromConversation(conversation);
                    if (!payloadGroupId.isEmpty()) {
                        object.insert(QStringLiteral("groupId"), payloadGroupId);
                        object.insert(QStringLiteral("isGroupChat"), true);
                    }
                    const QString messageConversationId = conversationIdFromMessageObject(object);
                    cacheRemoteMessageObject(object);

                    const QString stateConversationId = firstString(conversation, {QStringLiteral("conversationId"), QStringLiteral("id")});
                    bool markReadRequested = false;
                    if (!stateConversationId.isEmpty()) {
                        const bool activeVisible = isActiveVisibleConversation(stateConversationId);
                        const QJsonObject existing = MessageLocalDataSource::instance().conversation(stateConversationId);
                        QJsonObject merged = mergedConversationObject(existing, conversation);
                        if (activeVisible) {
                            setConversationUnreadCount(merged, 0);
                        }
                        int overrideUnread = 0;
                        if (localUnreadOverride(stateConversationId, &overrideUnread)) {
                            setConversationUnreadCount(merged, overrideUnread);
                        }
                        applyConversationStateObject(merged, false);
                        ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
                        MessageLocalDataSource::instance().upsertConversation(stateConversationId, merged);
                        if (activeVisible) {
                            ConversationRemoteDataSource::instance().markRead(stateConversationId);
                            markReadRequested = true;
                        }
                    }
                    if (!markReadRequested &&
                        !messageConversationId.isEmpty() &&
                        isActiveVisibleConversation(messageConversationId)) {
                        ConversationRemoteDataSource::instance().markRead(messageConversationId);
                    }
                    return;
                }

                if (type == QStringLiteral("chat.message.recalled")) {
                    ReferenceDataResolver::instance().consumePayload(payload);
                    const QString conversationId = firstString(payload, {QStringLiteral("conversationId"),
                                                                         QStringLiteral("id")});
                    QJsonObject object = payload.value(QStringLiteral("replacementMessage")).toObject();
                    if (object.isEmpty()) {
                        object = payload.value(QStringLiteral("message")).toObject();
                    }
                    if (object.isEmpty()) {
                        object = payload;
                    }
                    cacheRemoteMessageObject(object, conversationId);
                    return;
                }

                if (type == QStringLiteral("chat.conversation.updated") ||
                    type == QStringLiteral("chat.conversation.sync.updated") ||
                    type == QStringLiteral("chat.read.updated")) {
                    ReferenceDataResolver::instance().consumePayload(payload);
                    QJsonObject conversation = conversationObjectFromPayload(payload);
                    const QString conversationId = firstString(conversation, {QStringLiteral("conversationId"), QStringLiteral("id")});
                    if (!conversationId.isEmpty()) {
                        const bool activeVisible = isActiveVisibleConversation(conversationId);
                        const bool remoteHasUnread = conversationSyncStateFromJson(conversation).unreadCount > 0;
                        const QJsonObject existing = MessageLocalDataSource::instance().conversation(conversationId);
                        QJsonObject merged = mergedConversationObject(existing, conversation);
                        if ((type == QStringLiteral("chat.conversation.sync.updated") ||
                             type == QStringLiteral("chat.read.updated")) &&
                            conversationObjectHasUnreadCount(conversation)) {
                            clearLocalUnreadOverride(conversationId);
                        }
                        if (activeVisible) {
                            setConversationUnreadCount(merged, 0);
                        }
                        int overrideUnread = 0;
                        if (localUnreadOverride(conversationId, &overrideUnread)) {
                            setConversationUnreadCount(merged, overrideUnread);
                        }
                        applyConversationStateObject(merged);
                        const QJsonObject lastMessage = merged.value(QStringLiteral("lastMessage")).toObject();
                        if (!lastMessage.isEmpty()) {
                            QJsonObject messageObject = lastMessage;
                            if (!messageObject.contains(QStringLiteral("conversationId"))) {
                                messageObject.insert(QStringLiteral("conversationId"), conversationId);
                            }
                            cacheRemoteMessageObject(messageObject, conversationId, false);
                        }
                        ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
                        MessageLocalDataSource::instance().upsertConversation(conversationId, merged);
                        if (activeVisible &&
                            remoteHasUnread &&
                            type != QStringLiteral("chat.read.updated")) {
                            ConversationRemoteDataSource::instance().markRead(conversationId);
                        }
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
                markConversationReadLocal(conversationId, false);
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
            [this](const QString&, const QString&) {
                // Clearing is device-scoped and applied locally before the request is sent.
                // The success signal only confirms the backend watermark for this device.
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::messageRecallSucceeded,
            this,
            [this](const QString&, const QString& conversationId, const QJsonObject& message) {
                cacheRemoteMessageObject(message, conversationId);
            });
}

MessageRepository& MessageRepository::instance()
{
    static MessageRepository repo;
    return repo;
}

void MessageRepository::reloadFromStore()
{
    {
        QMutexLocker locker(&m_mutex);
        m_reloadFromStoreScheduled = false;
    }

    QMap<QString, QVector<QSharedPointer<ChatMessage>>> nextStore;
    QMap<QString, ConversationSyncState> nextStates;
    QMap<QString, QString> nextDirectPeers;
    QMap<QString, QString> nextGroupConversations;

    for (const QJsonObject& object : MessageLocalDataSource::instance().conversations()) {
        ConversationSyncState state = conversationSyncStateFromJson(object);
        if (state.conversationId.isEmpty()) {
            continue;
        }
        nextStates.insert(state.conversationId, state);

        const QJsonObject peer = directPeerUserObject(object);
        if (!peer.isEmpty()) {
            UserRepository::instance().upsertUserProfile(peer);
        }
        const QString peerId = directPeerIdFromConversation(object);
        if (!peerId.isEmpty()) {
            const User peerUser = UserRepository::instance().requestUserDetail({peerId});
            nextDirectPeers.insert(state.conversationId,
                                   peerUser.id.isEmpty() ? peerId : peerUser.id);
        }

        const QString groupId = groupIdFromConversation(object);
        if (!groupId.isEmpty()) {
            nextGroupConversations.insert(state.conversationId, groupId);
            GroupRepository::instance().upsertGroup(groupUpdateObjectFromConversation(object));
        }

        const QJsonObject lastMessage = object.value(QStringLiteral("lastMessage")).toObject();
        if (!lastMessage.isEmpty()) {
            QJsonObject messageObject = lastMessage;
            if (!messageObject.contains(QStringLiteral("conversationId"))) {
                messageObject.insert(QStringLiteral("conversationId"), state.conversationId);
            }
            const QSharedPointer<ChatMessage> message = isLocallyVisibleMessageObject(messageObject,
                                                                                      state.conversationId,
                                                                                      state.hiddenAt)
                    ? chatMessageFromJson(messageObject)
                    : QSharedPointer<ChatMessage>();
            if (!message.isNull()) {
                nextStore[state.conversationId].push_back(message);
            }
        }
    }

    for (const QJsonObject& object : MessageLocalDataSource::instance().messages()) {
        const QString conversationId = conversationIdFromMessageObject(object);
        const QDateTime hiddenAt = nextStates.value(conversationId).hiddenAt;
        const QSharedPointer<ChatMessage> message = isLocallyVisibleMessageObject(object,
                                                                                  conversationId,
                                                                                  hiddenAt)
                ? chatMessageFromJson(object)
                : QSharedPointer<ChatMessage>();
        if (conversationId.isEmpty() || message.isNull()) {
            continue;
        }

        QVector<QSharedPointer<ChatMessage>>& messages = nextStore[conversationId];
        const auto duplicate = std::find_if(messages.cbegin(), messages.cend(), [&message](const QSharedPointer<ChatMessage>& existing) {
            return isSameChatMessageIdentity(existing, message);
        });
        if (duplicate == messages.cend()) {
            messages.push_back(message);
        }
    }

    for (auto it = nextStore.begin(); it != nextStore.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), chatMessageLessThan);
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
        m_directConversationPeers = nextDirectPeers;
        m_groupConversationGroups = nextGroupConversations;
    }

    emit lastMessageChanged({}, {});
    emit conversationListChanged({});
}

void MessageRepository::scheduleReloadFromStore()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_reloadFromStoreScheduled) {
            return;
        }
        m_reloadFromStoreScheduled = true;
    }

    QTimer::singleShot(0, this, [this]() {
        reloadFromStore();
    });
}

bool MessageRepository::shouldIgnoreStoreChange(const QString& domain)
{
    QMutexLocker locker(&m_mutex);
    const int count = m_ignoredStoreChangeCounts.value(domain);
    if (count <= 0) {
        return false;
    }

    if (count == 1) {
        m_ignoredStoreChangeCounts.remove(domain);
    } else {
        m_ignoredStoreChangeCounts.insert(domain, count - 1);
    }
    return true;
}

void MessageRepository::ignoreNextStoreChange(const QString& domain)
{
    if (domain.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_ignoredStoreChangeCounts.insert(domain, m_ignoredStoreChangeCounts.value(domain) + 1);
}

bool MessageRepository::localUnreadOverride(const QString& conversationId, int* unreadCount) const
{
    if (conversationId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    if (!m_localUnreadOverrides.contains(conversationId)) {
        return false;
    }
    if (unreadCount) {
        *unreadCount = m_localUnreadOverrides.value(conversationId);
    }
    return true;
}

void MessageRepository::clearLocalUnreadOverride(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_localUnreadOverrides.remove(conversationId);
}

void MessageRepository::applyConversationStateObject(const QJsonObject& conversation, bool emitChange)
{
    ConversationSyncState incoming = conversationSyncStateFromJson(conversation);
    if (incoming.conversationId.isEmpty()) {
        return;
    }

    const QJsonObject peer = directPeerUserObject(conversation);
    const QString peerId = directPeerIdFromConversation(conversation);
    QString groupId = groupIdFromConversation(conversation);
    if (groupId.isEmpty() && !incoming.conversationId.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        groupId = m_groupConversationGroups.value(incoming.conversationId);
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_localUnreadOverrides.contains(incoming.conversationId)) {
            incoming.unreadCount = m_localUnreadOverrides.value(incoming.conversationId);
        }
        ConversationSyncState& state = m_conversationStates[incoming.conversationId];
        state.conversationId = incoming.conversationId;
        state.unreadCount = qMax(0, incoming.unreadCount);
        state.isDoNotDisturb = incoming.isDoNotDisturb;
        state.isPinned = incoming.isPinned;
        if (incoming.messageListTime.isValid()) {
            state.messageListTime = incoming.messageListTime;
        }
        if (incoming.lastReadAt.isValid()) {
            state.lastReadAt = incoming.lastReadAt;
        }
        if (incoming.hiddenAt.isValid() &&
            (!state.hiddenAt.isValid() || incoming.hiddenAt > state.hiddenAt)) {
            state.hiddenAt = incoming.hiddenAt;
        }
    }

    if (!peer.isEmpty()) {
        UserRepository::instance().upsertUserProfile(peer);
    }
    if (!groupId.isEmpty()) {
        GroupRepository::instance().upsertGroup(groupUpdateObjectFromConversation(conversation, groupId));
    }
    if (!peerId.isEmpty()) {
        const User peerUser = UserRepository::instance().requestUserDetail({peerId});
        {
            QMutexLocker locker(&m_mutex);
            m_directConversationPeers.insert(incoming.conversationId,
                                            peerUser.id.isEmpty() ? peerId : peerUser.id);
        }
    }
    if (!groupId.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_groupConversationGroups.insert(incoming.conversationId, groupId);
    }

    if (emitChange) {
        emit conversationListChanged(incoming.conversationId);
    }
}

void MessageRepository::cacheRemoteMessageObject(QJsonObject object,
                                                 const QString& fallbackConversationId,
                                                 bool updateUnread)
{
    QString conversationId = conversationIdFromMessageObject(object);
    if (conversationId.isEmpty()) {
        conversationId = fallbackConversationId;
        if (!conversationId.isEmpty() && !object.contains(QStringLiteral("conversationId"))) {
            object.insert(QStringLiteral("conversationId"), conversationId);
        }
    }
    if (conversationId.isEmpty()) {
        return;
    }

    QDateTime hiddenAt;
    {
        QMutexLocker locker(&m_mutex);
        hiddenAt = m_conversationStates.value(conversationId).hiddenAt;
    }
    if (!isLocallyVisibleMessageObject(object, conversationId, hiddenAt)) {
        return;
    }

    const QString cachedGroupId = groupIdForConversationId(conversationId);
    if (!cachedGroupId.isEmpty()) {
        object.insert(QStringLiteral("groupId"), cachedGroupId);
        object.insert(QStringLiteral("isGroupChat"), true);
    }

    if (!object.contains(QStringLiteral("type")) && object.contains(QStringLiteral("recalledAt"))) {
        object.insert(QStringLiteral("type"), QStringLiteral("recall"));
    }

    const QSharedPointer<ChatMessage> message = chatMessageFromJson(object);
    if (message.isNull()) {
        return;
    }

    QSharedPointer<ChatMessage> lastMsg;
    bool replaced = false;
    bool added = false;
    bool affectedLast = false;
    {
        QMutexLocker locker(&m_mutex);
        QVector<QSharedPointer<ChatMessage>>& messages = m_store[conversationId];
        for (int index = 0; index < messages.size(); ++index) {
            const QSharedPointer<ChatMessage>& existing = messages.at(index);
            if (!isSameChatMessageIdentity(existing, message)) {
                continue;
            }

            if (!message->getClientMessageId().isEmpty() &&
                existing &&
                existing->getClientMessageId() == message->getClientMessageId() &&
                existing->isFromMe() != message->isFromMe()) {
                message->setFromMe(existing->isFromMe());
            }
            messages[index] = message;
            replaced = true;
            affectedLast = index == messages.size() - 1;
            break;
        }

        if (!replaced) {
            messages.push_back(message);
            added = true;
        }

        std::sort(messages.begin(), messages.end(), chatMessageLessThan);

        if (!messages.isEmpty()) {
            lastMsg = messages.last();
            affectedLast = affectedLast || lastMsg.data() == message.data();
            ConversationSyncState& state = m_conversationStates[conversationId];
            state.conversationId = conversationId;
            state.messageListTime = lastMsg->getTimestamp();
            const bool activeVisible = m_hasActiveVisibleConversation &&
                    m_activeVisibleConversationId == conversationId;
            if (activeVisible) {
                m_localUnreadOverrides.insert(conversationId, 0);
                state.unreadCount = 0;
                state.lastReadAt = QDateTime::currentDateTime();
            } else if (updateUnread &&
                       added &&
                       !message->isFromMe()) {
                const int unreadBase = m_localUnreadOverrides.contains(conversationId)
                        ? m_localUnreadOverrides.value(conversationId)
                        : state.unreadCount;
                const int nextUnread = qMax(0, unreadBase) + 1;
                m_localUnreadOverrides.insert(conversationId, nextUnread);
                state.unreadCount = nextUnread;
            }
        }
    }

    const QJsonObject cachedObject = chatMessageToJson(conversationId, message);
    const QString key = chatMessageCacheKey(conversationId, message->getMessageId());
    if (!cachedObject.isEmpty() && !key.isEmpty()) {
        ignoreNextStoreChange(MessageLocalDataSource::messagesDomain());
        MessageLocalDataSource::instance().upsertMessage(key, cachedObject);
    }

    if (replaced) {
        emit messageUpdated(conversationId, message);
    }
    if (added || affectedLast) {
        emit lastMessageChanged(conversationId, lastMsg);
    }
    if (added || replaced) {
        emit conversationListChanged(conversationId);
    }
}

bool MessageRepository::fetchOlderMessagesBlocking(const QString& conversationId, int beforeMessageSeq, int limit)
{
    if (conversationId.isEmpty() || beforeMessageSeq <= 0 || limit <= 0) {
        return false;
    }

    const QString fetchKey = messageFetchKey(conversationId, QStringLiteral("older"), beforeMessageSeq);
    {
        QMutexLocker locker(&m_mutex);
        if (m_messageFetchesInFlight.contains(fetchKey)) {
            return false;
        }
        m_messageFetchesInFlight.insert(fetchKey);
    }

    const MessageRemoteDataSource::FetchResult result =
            MessageRemoteDataSource::instance().fetchOlderMessagesBlocking(conversationId, beforeMessageSeq, limit);
    for (const QJsonObject& object : result.messages) {
        cacheRemoteMessageObject(object, conversationId);
    }
    if (result.completed) {
        QMutexLocker locker(&m_mutex);
        m_conversationHasMoreBefore.insert(conversationId, result.hasMoreBefore);
    }
    {
        QMutexLocker locker(&m_mutex);
        m_messageFetchesInFlight.remove(fetchKey);
    }
    return result.completed && !result.messages.isEmpty();
}

bool MessageRepository::fetchLatestMessagesBlocking(const QString& conversationId, int limit)
{
    if (conversationId.isEmpty() || limit <= 0) {
        return false;
    }

    const QString fetchKey = messageFetchKey(conversationId, QStringLiteral("latest"));
    {
        QMutexLocker locker(&m_mutex);
        if (m_messageFetchesInFlight.contains(fetchKey)) {
            return false;
        }
        m_messageFetchesInFlight.insert(fetchKey);
    }

    const MessageRemoteDataSource::FetchResult result =
            MessageRemoteDataSource::instance().fetchLatestMessagesBlocking(conversationId, limit);
    for (const QJsonObject& object : result.messages) {
        cacheRemoteMessageObject(object, conversationId);
    }
    if (result.completed) {
        QMutexLocker locker(&m_mutex);
        m_conversationHasMoreBefore.insert(conversationId, result.hasMoreBefore);
    }
    {
        QMutexLocker locker(&m_mutex);
        m_messageFetchesInFlight.remove(fetchKey);
    }
    return result.completed && !result.messages.isEmpty();
}

bool MessageRepository::fetchNewerMessagesBlocking(const QString& conversationId, int afterMessageSeq, int limit)
{
    if (conversationId.isEmpty() || afterMessageSeq <= 0 || limit <= 0) {
        return false;
    }

    const QString fetchKey = messageFetchKey(conversationId, QStringLiteral("newer"), afterMessageSeq);
    {
        QMutexLocker locker(&m_mutex);
        if (m_messageFetchesInFlight.contains(fetchKey)) {
            return false;
        }
        m_messageFetchesInFlight.insert(fetchKey);
    }

    bool fetchedAny = false;
    int cursor = afterMessageSeq;
    bool hasMoreAfter = true;
    constexpr int kMaxFollowupPages = 10;

    for (int page = 0; page < kMaxFollowupPages && hasMoreAfter; ++page) {
        bool pageFetchedAny = false;
        int nextCursor = cursor;
        const MessageRemoteDataSource::FetchResult result =
                MessageRemoteDataSource::instance().fetchNewerMessagesBlocking(conversationId, cursor, limit);
        hasMoreAfter = result.hasMoreAfter;
        for (const QJsonObject& object : result.messages) {
            nextCursor = qMax(nextCursor, messageSeqFromObject(object));
            cacheRemoteMessageObject(object, conversationId);
            pageFetchedAny = true;
        }

        fetchedAny = fetchedAny || (result.completed && pageFetchedAny);
        if (!result.completed || !pageFetchedAny || nextCursor <= cursor) {
            break;
        }
        cursor = nextCursor;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_messageFetchesInFlight.remove(fetchKey);
    }
    return fetchedAny;
}

QVector<ConversationSummary> MessageRepository::requestConversationList(const ConversationListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationListRequestOperation(m_store,
                                            m_conversationStates,
                                            m_directConversationPeers,
                                            m_groupConversationGroups).request(query);
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
    return ConversationMetaRequestOperation(m_conversationStates,
                                            m_directConversationPeers,
                                            m_groupConversationGroups).request(query);
}

ConversationThreadData MessageRepository::requestConversationThread(const ConversationThreadRequest& query) const
{
    ConversationThreadData thread;
    thread.meta = requestConversationMeta({query.conversationId});
    if (query.conversationId.isEmpty()) {
        return thread;
    }

    int cachedCount = 0;
    int oldestMessageSeq = 0;
    int latestMessageSeq = 0;
    QDateTime hiddenAt;
    int clearedThroughSeq = 0;
    {
        QMutexLocker locker(&m_mutex);
        const ChatMessageList allMessages = m_store.value(query.conversationId);
        cachedCount = allMessages.size();
        if (!allMessages.isEmpty() && allMessages.first()) {
            oldestMessageSeq = allMessages.first()->getMessageSeq();
        }
        hiddenAt = m_conversationStates.value(query.conversationId).hiddenAt;
        int checkedTailCount = 0;
        const int requiredTailCount = qMax(0, query.limit);
        for (auto it = allMessages.crbegin(); it != allMessages.crend(); ++it) {
            if (requiredTailCount > 0 && checkedTailCount >= requiredTailCount) {
                break;
            }
            const QSharedPointer<ChatMessage>& message = *it;
            if (!message) {
                continue;
            }

            const int seq = message->getMessageSeq();
            if (seq <= 0) {
                continue;
            }
            if (latestMessageSeq <= 0) {
                latestMessageSeq = seq;
            }
            ++checkedTailCount;
        }
    }
    clearedThroughSeq = localConversationClearedThroughSeq(query.conversationId);

    if (query.offsetFromLatest == 0 && query.limit > 0) {
        const int latestPageLimit = qMax(query.limit, 50);
        const bool localTailKnownEmpty = cachedCount <= 0 &&
                conversationTailHiddenByDeletionBarrier(query.conversationId, hiddenAt);

        if (cachedCount > 0) {
            if (latestMessageSeq > 0) {
                const_cast<MessageRepository*>(this)->fetchNewerMessagesBlocking(
                        query.conversationId,
                        latestMessageSeq,
                        latestPageLimit);
            } else {
                const_cast<MessageRepository*>(this)->fetchLatestMessagesBlocking(query.conversationId,
                                                                                  latestPageLimit);
            }
        } else if (clearedThroughSeq > 0) {
            const_cast<MessageRepository*>(this)->fetchNewerMessagesBlocking(query.conversationId,
                                                                             clearedThroughSeq,
                                                                             latestPageLimit);
        } else if (localTailKnownEmpty) {
            // The local deletion barrier hides the server's current tail; avoid refetching it on every switch.
        } else {
            const_cast<MessageRepository*>(this)->fetchLatestMessagesBlocking(query.conversationId,
                                                                              latestPageLimit);
        }

        QMutexLocker locker(&m_mutex);
        const ChatMessageList allMessages = m_store.value(query.conversationId);
        cachedCount = allMessages.size();
        oldestMessageSeq = (!allMessages.isEmpty() && allMessages.first())
                ? allMessages.first()->getMessageSeq()
                : 0;
    }

    const bool requestBeyondCachedOlderEdge = query.offsetFromLatest >= cachedCount && cachedCount > 0;
    if (requestBeyondCachedOlderEdge && oldestMessageSeq > 1 && query.limit > 0) {
        const_cast<MessageRepository*>(this)->fetchOlderMessagesBlocking(query.conversationId,
                                                                         oldestMessageSeq,
                                                                         query.limit);
    }

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
        if (!thread.hasMoreBefore &&
            query.offsetFromLatest == 0 &&
            query.limit > 0 &&
            thread.messages.size() >= query.limit &&
            !thread.messages.isEmpty() &&
            thread.messages.first() &&
            thread.messages.first()->getMessageSeq() > 1) {
            thread.hasMoreBefore = true;
        }
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
    int oldestMessageSeq = 0;
    if (!allMessages.isEmpty() && allMessages.first()) {
        oldestMessageSeq = allMessages.first()->getMessageSeq();
    }
    locker.unlock();

    if (offset >= total && total > 0 && oldestMessageSeq > 1) {
        const_cast<MessageRepository*>(this)->fetchOlderMessagesBlocking(query.conversationId,
                                                                         oldestMessageSeq,
                                                                         qMax(30, total));
    }

    locker.relock();
    const ChatMessageList refreshedMessages = m_store.value(query.conversationId);
    const int refreshedTotal = refreshedMessages.size();
    const int refreshedOffset = qBound(0, query.offsetFromLatest, refreshedTotal);
    const int refreshedEnd = refreshedTotal - refreshedOffset;
    thread.unreadCount = m_conversationStates.value(query.conversationId).unreadCount;
    thread.loadedMessageCount = refreshedOffset;
    thread.hasMoreBefore = refreshedOffset < refreshedTotal;
    if (refreshedEnd <= 0) {
        return thread;
    }

    bool targetInUnloadedRange = false;
    for (int index = 0; index < refreshedEnd; ++index) {
        const QSharedPointer<ChatMessage>& message = refreshedMessages.at(index);
        if (message && message->getMessageId() == query.messageId) {
            targetInUnloadedRange = true;
            break;
        }
    }
    if (!targetInUnloadedRange) {
        return thread;
    }

    thread.messages = refreshedMessages.mid(0, refreshedEnd);
    thread.loadedMessageCount = refreshedTotal;
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

    QTimer::singleShot(0, this, [this, requestId, query, unreadCountAtRequest]() {
        ConversationThreadData thread = requestConversationThread(query);
        thread.unreadCount = unreadCountAtRequest;
        emit conversationThreadReady(requestId, thread);
    });
    return requestId;
}

void MessageRepository::setActiveVisibleConversation(const QString& conversationId, bool visible)
{
    bool shouldMarkRead = false;
    {
        QMutexLocker locker(&m_mutex);
        const QString previousActiveConversationId = m_activeVisibleConversationId;
        if (!previousActiveConversationId.isEmpty() &&
            previousActiveConversationId != conversationId &&
            m_localUnreadOverrides.value(previousActiveConversationId) == 0) {
            m_localUnreadOverrides.remove(previousActiveConversationId);
        }
        m_activeVisibleConversationId = conversationId;
        m_hasActiveVisibleConversation = visible && !conversationId.isEmpty();
        if (m_hasActiveVisibleConversation) {
            m_localUnreadOverrides.insert(conversationId, 0);
            ConversationSyncState& state = m_conversationStates[conversationId];
            state.conversationId = conversationId;
            shouldMarkRead = state.unreadCount > 0;
        } else if (!previousActiveConversationId.isEmpty() &&
                   m_localUnreadOverrides.value(previousActiveConversationId) == 0) {
            m_localUnreadOverrides.remove(previousActiveConversationId);
        }
    }

    if (shouldMarkRead) {
        markConversationRead(conversationId);
    } else if (visible && !conversationId.isEmpty()) {
        emit conversationListChanged(conversationId);
    }
}

bool MessageRepository::isActiveVisibleConversation(const QString& conversationId) const
{
    if (conversationId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    return m_hasActiveVisibleConversation &&
           m_activeVisibleConversationId == conversationId;
}

void MessageRepository::touchConversation(const QString& conversationId,
                                          const QDateTime& timestamp)
{
    if (conversationId.isEmpty() || !timestamp.isValid()) {
        return;
    }

    QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    touchConversationObject(conversationId, conversation, timestamp);
}

void MessageRepository::touchDirectConversation(const QString& conversationId,
                                                const QString& peerUserId,
                                                const QDateTime& timestamp)
{
    if (conversationId.isEmpty() || !timestamp.isValid()) {
        return;
    }

    QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    conversation = directConversationObjectForPeer(conversation, conversationId, peerUserId);
    touchConversationObject(conversationId, conversation, timestamp);
}

void MessageRepository::touchConversationObject(const QString& conversationId,
                                                QJsonObject conversation,
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

    if (conversation.isEmpty()) {
        conversation.insert(QStringLiteral("conversationId"), conversationId);
    } else if (!conversation.contains(QStringLiteral("conversationId")) &&
               !conversation.contains(QStringLiteral("id"))) {
        conversation.insert(QStringLiteral("conversationId"), conversationId);
    }
    conversation.insert(QString::fromLatin1(kLocalMessageListTimeKey),
                        timestamp.toUTC().toString(Qt::ISODateWithMs));
    ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
    MessageLocalDataSource::instance().upsertConversation(conversationId, conversation);

    const QString peerId = directPeerIdFromConversation(conversation);
    if (!peerId.isEmpty()) {
        const User peerUser = UserRepository::instance().requestUserDetail({peerId});
        QMutexLocker locker(&m_mutex);
        m_directConversationPeers.insert(conversationId,
                                        peerUser.id.isEmpty() ? peerId : peerUser.id);
    }

    const QString mappedGroupId = groupIdFromConversation(conversation);
    if (!mappedGroupId.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_groupConversationGroups.insert(conversationId, mappedGroupId);
    }

    emit conversationListChanged(conversationId);
}

void MessageRepository::markConversationRead(const QString& conversationId)
{
    markConversationReadLocal(conversationId, true);
}

void MessageRepository::markConversationReadLocal(const QString& conversationId, bool notifyRemote)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool activeVisible = m_hasActiveVisibleConversation &&
                m_activeVisibleConversationId == conversationId;
        if (activeVisible) {
            m_localUnreadOverrides.insert(conversationId, 0);
        } else {
            m_localUnreadOverrides.remove(conversationId);
        }
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        changed = state.unreadCount != 0;
        state.unreadCount = 0;
        state.lastReadAt = QDateTime::currentDateTime();
    }

    QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    if (!conversation.isEmpty()) {
        setConversationUnreadCount(conversation, 0);
        ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
        MessageLocalDataSource::instance().upsertConversation(conversationId, conversation);
    }

    if (changed) {
        emit conversationListChanged(conversationId);
        if (notifyRemote) {
            ConversationRemoteDataSource::instance().markRead(conversationId);
        }
    }
}

void MessageRepository::markConversationUnread(const QString& conversationId, int unreadCount)
{
    if (conversationId.isEmpty()) {
        return;
    }

    const int normalizedUnreadCount = qMax(1, unreadCount);
    {
        QMutexLocker locker(&m_mutex);
        m_localUnreadOverrides.insert(conversationId, normalizedUnreadCount);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.unreadCount = normalizedUnreadCount;
    }

    QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    if (!conversation.isEmpty()) {
        setConversationUnreadCount(conversation, normalizedUnreadCount);
        ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
        MessageLocalDataSource::instance().upsertConversation(conversationId, conversation);
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

    const QDateTime clearedAt = QDateTime::currentDateTime();
    int clearedThroughSeq = conversationLastMessageSeq(conversationId);
    {
        QMutexLocker locker(&m_mutex);
        const ChatMessageList messages = m_store.value(conversationId);
        for (const QSharedPointer<ChatMessage>& message : messages) {
            if (message) {
                clearedThroughSeq = qMax(clearedThroughSeq, message->getMessageSeq());
            }
        }
    }
    persistConversationClearMarker(conversationId, clearedAt, clearedThroughSeq);
    removePersistedMessagesForConversation(conversationId);

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
        state.hiddenAt = clearedAt;
    }

    QJsonObject conversation = MessageLocalDataSource::instance().conversation(conversationId);
    if (!conversation.isEmpty()) {
        conversation.remove(QStringLiteral("lastMessage"));
        QJsonObject syncState = conversation.value(QStringLiteral("syncState")).toObject();
        syncState.insert(QStringLiteral("hiddenAt"), clearedAt.toUTC().toString(Qt::ISODateWithMs));
        syncState.insert(QStringLiteral("unreadCount"), 0);
        conversation.insert(QStringLiteral("syncState"), syncState);
        conversation.insert(QStringLiteral("unreadCount"), 0);
        ignoreNextStoreChange(MessageLocalDataSource::conversationsDomain());
        MessageLocalDataSource::instance().upsertConversation(conversationId, conversation);
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

    MessageLocalDataSource::instance().removeConversation(conversationId);

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_conversationStates.remove(conversationId) > 0;
        changed = m_directConversationPeers.remove(conversationId) > 0 || changed;
        changed = m_groupConversationGroups.remove(conversationId) > 0 || changed;
        m_localUnreadOverrides.remove(conversationId);
    }

    if (!changed) {
        return;
    }

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
    if (message->getSendState() == MessageSendState::Sent ||
        message->getSendState() == MessageSendState::Failed) {
        persistMessage(conversationId, message);
    }
    emit lastMessageChanged(conversationId, message);
    emit conversationListChanged(conversationId);
}

void MessageRepository::persistMessage(const QString& conversationId,
                                       const QSharedPointer<ChatMessage>& message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return;
    }

    const QJsonObject object = chatMessageToJson(conversationId, message);
    const QString key = chatMessageCacheKey(conversationId, message->getMessageId());
    if (object.isEmpty() || key.isEmpty()) {
        return;
    }

    ignoreNextStoreChange(MessageLocalDataSource::messagesDomain());
    MessageLocalDataSource::instance().upsertMessage(key, object);
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
    QSharedPointer<ChatMessage> removedMessage;
    {
        QMutexLocker locker(&m_mutex);
        auto& vec = m_store[conversationId];
        if (index >= 0 && index < vec.size()) {
            removedMessage = vec.at(index);
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

    if (!removedMessage.isNull()) {
        persistMessageDeletionMarker(conversationId, removedMessage);
        const QString messageId = removedMessage->getMessageId().isEmpty()
                ? removedMessage->getClientMessageId()
                : removedMessage->getMessageId();
        const QString key = chatMessageCacheKey(conversationId, messageId);
        if (!key.isEmpty()) {
            MessageLocalDataSource::instance().removeMessage(key);
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
