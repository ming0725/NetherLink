#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QString>
#include <QDateTime>
#include <QUuid>
#include <memory>

enum class MessageType {
    Text,
    Image,
    File,
    Voice,
    Recall,
    GroupMemberJoined,
    GroupSystemEvent
};

enum class GroupRole {
    Owner,      // 群主
    Admin,      // 管理员
    Member      // 普通成员
};

class ChatMessage {
public:
    ChatMessage(bool isFromMe, const QString& senderId, bool isGroupChat = false, 
               const QString& senderName = QString(), GroupRole role = GroupRole::Member)
        : messageId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
          fromMe(isFromMe), senderId(senderId), timestamp(QDateTime::currentDateTime()),
          isSelected(false), isGroupChat(isGroupChat), senderName(senderName), role(role) {}
    
    virtual ~ChatMessage() = default;
    virtual QString getContent() const = 0;
    virtual MessageType getType() const = 0;
    
    QString getMessageId() const { return messageId; }
    void setMessageId(const QString& id)
    {
        if (!id.isEmpty()) {
            messageId = id;
        }
    }
    bool isFromMe() const { return fromMe; }
    QString getSenderId() const { return senderId; }
    QDateTime getTimestamp() const { return timestamp; }
    void setTimestamp(const QDateTime& newTimestamp) { timestamp = newTimestamp; }
    
    bool getIsSelected() const { return isSelected; }
    void setSelected(bool selected) { isSelected = selected; }

    // 群聊相关
    bool isInGroupChat() const { return isGroupChat; }
    QString getSenderName() const { return senderName; }
    GroupRole getRole() const { return role; }
    void setSenderName(const QString& name) { senderName = name; }
    void setRole(GroupRole nextRole) { role = nextRole; }
    QString getReferencedMessageId() const { return referencedMessageId; }
    void setReferencedMessageId(const QString& id) { referencedMessageId = id; }

protected:
    QString messageId;
    bool fromMe;
    QString senderId;
    QDateTime timestamp;
    bool isSelected;
    
    // 群聊相关属性
    bool isGroupChat;
    QString senderName;
    GroupRole role;
    QString referencedMessageId;
};

class TextMessage : public ChatMessage {
public:
    TextMessage(const QString& text, bool isFromMe, const QString& senderId,
               bool isGroupChat = false, const QString& senderName = QString(),
               GroupRole role = GroupRole::Member)
        : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), text(text) {}

    QString getContent() const override { return text; }
    MessageType getType() const override { return MessageType::Text; }
    QString getText() const { return text; }

private:
    QString text;
};

class ImageMessage : public ChatMessage {
public:
    ImageMessage(const QString& imageSource, bool isFromMe, const QString& senderId,
                bool isGroupChat = false, const QString& senderName = QString(),
                GroupRole role = GroupRole::Member)
        : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), imageSource(imageSource) {}
    
    QString getContent() const override { return "[图片]"; }
    MessageType getType() const override { return MessageType::Image; }
    QString getImageSource() const { return imageSource; }

private:
    QString imageSource;
};

class RecallMessage : public ChatMessage {
public:
    RecallMessage(const QString& displayText,
                  const QString& originalText,
                  bool allowReedit,
                  bool originalFromMe,
                  const QString& originalSenderId,
                  bool isGroupChat = false,
                  const QString& originalSenderName = QString(),
                  GroupRole originalSenderRole = GroupRole::Member,
                  const QString& actorId = QString(),
                  const QString& actorName = QString(),
                  GroupRole actorRole = GroupRole::Member,
                  bool moderatorRecall = false)
        : ChatMessage(originalFromMe,
                      originalSenderId,
                      isGroupChat,
                      originalSenderName,
                      originalSenderRole)
        , displayText(displayText)
        , originalText(originalText)
        , allowReedit(allowReedit)
        , actorId(actorId)
        , actorName(actorName)
        , actorRole(actorRole)
        , moderatorRecall(moderatorRecall)
    {
    }

    QString getContent() const override { return displayText; }
    MessageType getType() const override { return MessageType::Recall; }
    QString getDisplayText() const { return displayText; }
    QString getOriginalText() const { return originalText; }
    bool canReedit() const { return allowReedit && !originalText.isEmpty(); }
    void clearReeditText()
    {
        originalText.clear();
        allowReedit = false;
    }
    QString getActorId() const { return actorId; }
    QString getActorName() const { return actorName; }
    GroupRole getActorRole() const { return actorRole; }
    bool isModeratorRecall() const { return moderatorRecall; }
    void setActorName(const QString& name) { actorName = name; }
    void setActorRole(GroupRole role) { actorRole = role; }

private:
    QString displayText;
    QString originalText;
    bool allowReedit = false;
    QString actorId;
    QString actorName;
    GroupRole actorRole = GroupRole::Member;
    bool moderatorRecall = false;
};

class GroupMemberJoinedMessage : public ChatMessage {
public:
    GroupMemberJoinedMessage(const QString& memberId,
                             const QString& memberName,
                             const QString& inviterId = QString(),
                             const QString& inviterName = QString())
        : ChatMessage(false,
                      memberId,
                      true,
                      memberName,
                      GroupRole::Member)
        , memberId(memberId)
        , memberName(memberName)
        , inviterId(inviterId)
        , inviterName(inviterName)
    {
    }

    QString getContent() const override
    {
        if (!inviterName.isEmpty()) {
            return QStringLiteral("%1邀请了%2加入群聊").arg(inviterName, memberName);
        }
        return QStringLiteral("%1加入了群聊").arg(memberName);
    }

    MessageType getType() const override { return MessageType::GroupMemberJoined; }
    QString getMemberId() const { return memberId; }
    QString getMemberName() const { return memberName; }
    QString getInviterId() const { return inviterId; }
    QString getInviterName() const { return inviterName; }
    void setMemberName(const QString& name) { memberName = name; senderName = name; }
    void setInviterName(const QString& name) { inviterName = name; }

private:
    QString memberId;
    QString memberName;
    QString inviterId;
    QString inviterName;
};

class GroupSystemEventMessage : public ChatMessage {
public:
    GroupSystemEventMessage(const QString& highlightedName,
                            const QString& suffix,
                            const QString& prefix = QString(),
                            const QString& highlightedUserId = QString())
        : ChatMessage(false,
                      QString(),
                      true,
                      QString(),
                      GroupRole::Member)
        , prefix(prefix)
        , highlightedName(highlightedName)
        , highlightedUserId(highlightedUserId)
        , suffix(suffix)
    {
    }

    QString getContent() const override
    {
        QString content;
        if (!prefix.isEmpty()) {
            content += prefix;
        }
        content += highlightedName;
        if (!suffix.isEmpty()) {
            content += suffix;
        }
        return content;
    }

    MessageType getType() const override { return MessageType::GroupSystemEvent; }
    QString getPrefix() const { return prefix; }
    QString getHighlightedName() const { return highlightedName; }
    QString getHighlightedUserId() const { return highlightedUserId; }
    QString getSuffix() const { return suffix; }
    void setHighlightedName(const QString& name) { highlightedName = name; }

private:
    QString prefix;
    QString highlightedName;
    QString highlightedUserId;
    QString suffix;
};

#endif // CHATMESSAGE_H
