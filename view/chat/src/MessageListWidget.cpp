// MessageListWidget.cpp
#include "MessageListWidget.h"
#include "MessageRepository.h"
#include "UserRepository.h"
#include "CurrentUser.h"
#include "GroupRepository.h"
#include "MessageHandler.h"
#include "NetworkManager.h"
#include <QEvent>
#include <QVBoxLayout>
#include <algorithm>

MessageListWidget::MessageListWidget(QWidget* parent)
    : CustomScrollArea(parent)
{
    installEventFilter(this);
    setMouseTracking(true);
    updateMessages();

    // 连接MessageHandler的信号
    auto& handler = MessageHandler::instance();
    connect(&handler, &MessageHandler::unreadMessageReceived,
            this, &MessageListWidget::onUnreadMessageReceived);
    connect(&handler, &MessageHandler::updateLastMessageTime,
            this, &MessageListWidget::onLastMessageTimeUpdated);
            
    // 连接好友请求接受信号
    connect(&NetworkManager::instance(), &NetworkManager::friendRequestAccepted,
            this, &MessageListWidget::updateMessages);
}

void MessageListWidget::updateMessages()
{
    clearMessages();

    // 获取所有会话
    auto& gr = GroupRepository::instance();
    auto& ur = UserRepository::instance();
    auto& mr = MessageRepository::instance();

    // 获取所有群组和用户
    auto groups = gr.getAllGroup();
    auto users = ur.getAllUser();
    qDebug() << "updateMessages users size " << users.size();
    // 创建所有消息项
    for (const auto& group : groups) {
        auto data = QSharedPointer<MessageData>::create();
        data->id = group.groupId;
        data->senderId = group.groupId;
        data->senderName = group.groupName;
        data->isGroup = true;
        // 获取最后一条消息
        auto lastMsg = mr.getLastMessage(group.groupId, true);
        if (lastMsg) {
            data->content = lastMsg->getContent();
            data->timestamp = lastMsg->getTimestamp();
        } else {
            data->content = "";
            data->timestamp = QDateTime::currentDateTime();
        }

        auto* item = new MessageListItem(data, contentWidget);
        connect(item, &MessageListItem::itemClicked,
                this, &MessageListWidget::onItemClicked);
        m_items.append(item);
    }

    for (const auto& user : users) {
        auto data = QSharedPointer<MessageData>::create();
        data->id = user.id;
        data->senderId = user.id;
        data->senderName = user.nick;
        data->isGroup = false;
        if (user.id == CurrentUser::instance().getUserId()) continue;
        // 获取最后一条消息
        auto lastMsg = mr.getLastMessage(user.id, false);
        if (lastMsg) {
            data->content = lastMsg->getContent();
            data->timestamp = lastMsg->getTimestamp();
        } else {
            data->content = "";
            data->timestamp = QDateTime::currentDateTime();
        }

        auto* item = new MessageListItem(data, contentWidget);
        item->show();
        connect(item, &MessageListItem::itemClicked,
                this, &MessageListWidget::onItemClicked);
        m_items.append(item);
    }

    // 按时间排序
    std::sort(m_items.begin(), m_items.end(), 
        [](MessageListItem* a, MessageListItem* b) {
            return a->getData()->timestamp > b->getData()->timestamp;
        });

    layoutContent();
}

void MessageListWidget::addMessage(const MessageDataPtr& data)
{
    auto* item = new MessageListItem(data, contentWidget);
    connect(item, &MessageListItem::itemClicked,
            this, &MessageListWidget::onItemClicked);
    m_items.append(item);

    // 按时间排序
    std::sort(m_items.begin(), m_items.end(), 
        [](MessageListItem* a, MessageListItem* b) {
            return a->getData()->timestamp > b->getData()->timestamp;
        });

    layoutContent();
}

void MessageListWidget::clearMessages()
{
    for (auto* item : m_items) {
        delete item;
    }
    m_items.clear();
    selectItem = nullptr;
}

void MessageListWidget::layoutContent()
{
    int y = 0;
    int w = contentWidget->width();

    for (auto* item : m_items) {
        int h = item->sizeHint().height();
        item->setGeometry(0, y, w, h);
        y += h;
    }

    contentWidget->resize(w, y);
}

void MessageListWidget::onItemClicked(MessageListItem* item)
{
    if (!item) return;
    if (item != selectItem) {
        emit itemClicked(item);
    }
    if (selectItem) {
        selectItem->setSelected(false);
        item->setSelected(true);
        selectItem = item;
    }
    else {
        selectItem = item;
        selectItem->setSelected(true);
    }
}

void MessageListWidget::onUnreadMessageReceived(const QString& conversationId,
                                        const QString& content,
                                        int unreadCount)
{
    // 查找对应的消息列表项
    MessageListItem* item = findItemById(conversationId);
    if (item) {
        // 如果不是当前选中的项，更新未读计数
        if (item != selectItem) {
            item->setUnreadCount(item->getUnreadCount() + 1);
            // 更新最后一条消息内容
            auto data = item->getData();
            data->content = content;
            item->update();
        }
    }

}

void MessageListWidget::onLastMessageTimeUpdated(const QString& chatId,
                                               const QDateTime& timestamp)
{
    MessageListItem* item = findItemById(chatId);
    if (item) {
        auto data = item->getData();
        data->timestamp = timestamp;
        item->update();
    }
    // 按时间排序
    std::sort(m_items.begin(), m_items.end(),
              [](MessageListItem* a, MessageListItem* b) {
                  return a->getData()->timestamp > b->getData()->timestamp;
              });
    layoutContent();
}

MessageListItem* MessageListWidget::findItemById(const QString& id)
{
    for (auto item : m_items) {
        if (item->getChatID() == id) {
            return item;
        }
    }
    return nullptr;
}
