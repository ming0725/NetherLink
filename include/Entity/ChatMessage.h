/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_ENTITY_CHAT_MESSAGE
#define INCLUDE_ENTITY_CHAT_MESSAGE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPixmap>

#include "Data/MessageData.h"

/* enum ------------------------------------------------------------------- 80 // ! ----------------------------- 120 */
enum class GroupRole {
    Owner,      // 群主
    Admin, // 管理员
    Member // 普通成员
};

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ChatMessage {
    public:
        ChatMessage(bool isFromMe, const QString& senderId, bool isGroupChat = false, const QString& senderName = QString(), GroupRole role = GroupRole::Member) : timestamp(QDateTime::currentDateTime()), senderId(senderId), senderName(senderName), isFromMe_(isFromMe), isSelected_(false), inGroupChat_(isGroupChat), role(role) {}

        virtual ~ChatMessage() = default;

        virtual QString getContent() const = 0;

        virtual MessageType getType() const = 0;

        void setTimestamp(const QDateTime& time) {
            timestamp = time;
        }

        void setSenderId(const QString& id) {
            senderId = id;
        }

        void setSenderName(const QString& name) {
            senderName = name;
        }

        void setIsFromMe(bool from) {
            isFromMe_ = from;
        }

        void setIsSelected(bool selected) {
            isSelected_ = selected;
        }

        void setInGroupChat(bool inGroup) {
            inGroupChat_ = inGroup;
        }

        QDateTime getTimestamp() const {
            return (timestamp);
        }

        QString getSenderId() const {
            return (senderId);
        }

        QString getSenderName() const {
            return (senderName);
        }

        bool isFromMe() const {
            return (isFromMe_);
        }

        bool getIsSelected() const {
            return (isSelected_);
        }

        bool isInGroupChat() const {
            return (inGroupChat_);
        }

        // 群聊相关
        GroupRole getRole() const {
            return (role);
        }

    protected:
        QDateTime timestamp;
        QString senderId;
        QString senderName;
        bool isFromMe_ = false;
        bool isSelected_ = false;
        bool inGroupChat_ = false;

        // 群聊相关属性
        GroupRole role;

};

class TextMessage : public ChatMessage {
    public:
        TextMessage(const QString& text, bool isFromMe, const QString& senderId, bool isGroupChat = false, const QString& senderName = QString(), GroupRole role = GroupRole::Member) : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), text(text) {}

        QString getContent() const override {
            return (text);
        }

        MessageType getType() const override {
            return (MessageType::Text);
        }

        QString getText() const {
            return (text);
        }

    private:
        QString text;

};

class ImageMessage : public ChatMessage {
    public:
        ImageMessage(const QPixmap& image, bool isFromMe, const QString& senderId, bool isGroupChat = false, const QString& senderName = QString(), GroupRole role = GroupRole::Member) : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), image(image) {}

        QString getContent() const override {
            return ("[图片]");
        }

        MessageType getType() const override {
            return (MessageType::Image);
        }

        QPixmap getImage() const {
            return (image);
        }

        // 获取图片本地缓存路径
        QString getLocalPath() const {
            return (localPath);
        }

        void setLocalPath(const QString& path) {
            localPath = path;
        }

    private:
        QPixmap image;
        QString localPath; // 图片本地缓存路径
};

#endif /* INCLUDE_ENTITY_CHAT_MESSAGE */
