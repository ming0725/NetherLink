/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QBoxLayout>
#include <QJsonDocument>
#include <QScrollBar>
#include <QTimer>

#include "Data/CurrentUser.h"
#include "Data/GroupRepository.h"
#include "Data/MessageRepository.h"
#include "Data/UserRepository.h"
#include "Network/NetworkManager.h"
#include "View/Chat/ChatArea.h"
#include "View/Chat/MessageHandler.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

ChatArea::ChatArea(QWidget*parent) : QWidget(parent), unreadMessageCount(0), isAtBottom(true), isGroupMode(false) {
    // 创建布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建视图组件
    chatView = new ChatListView(this);
    chatView->setSpacing(2);  // 设置消息间距
    // 创建模型和代理
    chatModel = new ChatListModel(this);
    chatDelegate = new ChatItemDelegate(this);

    // 设置模型和代理
    chatView->setModel(chatModel);
    chatView->setItemDelegate(chatDelegate);

    // 创建新消息提示组件
    newMessageNotifier = new NewMessageNotifier(this);
    newMessageNotifier->hide();

    // 创建悬浮输入栏
    inputBar = new FloatingInputBar(this);
    inputBar->show();

    QWidget* chatInfo = new QWidget(this);

    chatInfo->setFixedHeight(24 + 26 + 12);
    chatInfo->setObjectName("ChatInfo");
    chatInfo->setStyleSheet("#ChatInfo { background-color: #F2F2F2; border-bottom: 1px solid #E9E9E9; }");

    // 外层垂直布局：用于将内容推到底部
    QVBoxLayout* outerLayout = new QVBoxLayout(chatInfo);

    outerLayout->setContentsMargins(20, 5, 5, 10); // 留出左下边距
    outerLayout->addStretch(); // 将内容推到底部

    // 内层水平布局：图标 + 名字
    QHBoxLayout* bottomLayout = new QHBoxLayout();

    bottomLayout->setSpacing(5); // 图标与文字间距
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 在线状态图标
    statusIcon = new QLabel(chatInfo);

    // 名字 Label
    nameLabel = new QLabel("", chatInfo);
    nameLabel->setStyleSheet("font-size: 17px; color: #000000;");

    // 添加到底部布局
    bottomLayout->addWidget(nameLabel);
    bottomLayout->addWidget(statusIcon);
    bottomLayout->addStretch(); // 保证靠左
    // 添加到底部区域
    outerLayout->addLayout(bottomLayout);
    chatInfo->setLayout(outerLayout);

    // 添加到主布局
    mainLayout->addWidget(chatInfo);
    mainLayout->addWidget(chatView);
    setLayout(mainLayout);

    // 连接信号槽
    connect(chatView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatArea::onScrollValueChanged);
    connect(newMessageNotifier, &NewMessageNotifier::clicked, this, &ChatArea::onNewMessageNotifierClicked);
    connect(inputBar, &FloatingInputBar::sendImage, this, &ChatArea::onSendImage);
    connect(inputBar, &FloatingInputBar::sendText, this, &ChatArea::onSendText);

    // 连接MessageHandler信号
    connect(&MessageHandler::instance(), &MessageHandler::messageReceived, this, &ChatArea::onMessageReceived);

    // 设置样式
    chatView->setStyleSheet("QListView {""    background-color: #F2F2F2;""}");
}

void ChatArea::addMessage(QSharedPointer <ChatMessage> message) {
    bool shouldScroll = message->isFromMe() || isNearBottom();

    // 添加消息
    chatModel->addMessage(message);
    MessageRepository::instance().addMessage(messageId, message);

    // 调整底部空白
    adjustBottomSpace();

    // 根据情况处理滚动和未读计数
    if (!message->isFromMe() && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }

    // 如果需要滚动到底部，使用延时确保布局更新完成
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}

void ChatArea::addImageMessage(QSharedPointer <ImageMessage> message, const QDateTime& timestamp) {
    bool isFromMe = message->isFromMe();
    bool shouldScroll = isFromMe || isNearBottom();

    // 设置消息时间戳为当前时间
    message->setTimestamp(timestamp);

    // 添加消息
    chatModel->addMessage(std::move(message));

    // 根据情况处理滚动和未读计数
    if (!isFromMe && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }

    // 最后处理滚动，确保新消息完全可见
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}

void ChatArea::onScrollValueChanged(int) {
    isAtBottom = isScrollAtBottom();

    if (isAtBottom) {
        unreadMessageCount = 0;
        updateNewMessageNotifier();
    }
}

bool ChatArea::isNearBottom() const {
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    int totalHeight = scrollBar->maximum();
    int currentBottom = scrollBar->value();

    return ((totalHeight - currentBottom) <= 250);
}

bool ChatArea::isScrollAtBottom() const {
    QScrollBar* scrollBar = chatView->verticalScrollBar();

    return (scrollBar->value() >= scrollBar->maximum() - 5);
}

void ChatArea::onNewMessageNotifierClicked() {
    scrollToBottom();
}

void ChatArea::resizeEvent(QResizeEvent*event) {
    QWidget::resizeEvent(event);
    updateNewMessageNotifierPosition();
    updateInputBarPosition();

    // 确保新消息提示器在最上层
    if (newMessageNotifier && newMessageNotifier->isVisible()) {
        newMessageNotifier->raise();
    }
}

void ChatArea::updateNewMessageNotifier() {
    if (unreadMessageCount > 0) {
        newMessageNotifier->setMessageCount(unreadMessageCount);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
    } else {
        newMessageNotifier->hide();
    }
}

void ChatArea::updateNewMessageNotifierPosition() {
    if (newMessageNotifier->isVisible() && inputBar) {
        // 计算新消息提示器的位置
        int x = (width() - newMessageNotifier->width()) / 2;

        // 将提示器放在输入栏上方
        const int BUTTOM_MARGIN = 26;
        int y = height() - inputBar->height() - newMessageNotifier->height() - BUTTOM_MARGIN;

        newMessageNotifier->move(x, y);
        newMessageNotifier->raise();  // 确保显示在最上层
    }
}

void ChatArea::scrollToBottom() {
    chatView->scrollToBottom();
    isAtBottom = true;
    unreadMessageCount = 0;
    updateNewMessageNotifier();
}

void ChatArea::adjustBottomSpace() {
    // 使用固定的底部空白高度
    chatModel->setBottomSpaceHeight(BottomSpace::DEFAULT_HEIGHT);
}

void ChatArea::addTextMessage(QSharedPointer <TextMessage> message, const QDateTime &timestamp) {
    bool isFromMe = message->isFromMe();
    bool shouldScroll = isFromMe || isNearBottom();

    // 设置消息时间戳为当前时间
    message->setTimestamp(timestamp);

    // 添加消息
    chatModel->addMessage(message);

    // 根据情况处理滚动和未读计数
    if (!isFromMe && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }

    // 最后处理滚动，确保新消息完全可见
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}

void ChatArea::setGroupMode(bool mode) {
    isGroupMode = mode;
}

void ChatArea::updateInputBarPosition() {
    if (inputBar) {
        // 设置输入栏的大小和位置
        const int MARGIN = 20; // 距离边缘的边距
        const int INPUT_BAR_WIDTH = width() - 2 * MARGIN;
        const int INPUT_BAR_HEIGHT = 175;  // 输入栏高度

        inputBar->setGeometry(MARGIN, // x
                              height() - INPUT_BAR_HEIGHT - MARGIN, // y
                              INPUT_BAR_WIDTH, // width
                              INPUT_BAR_HEIGHT // height
                              );
    }
}

void ChatArea::onSendImage(const QString &path) {
    QPixmap image(path);

    if (!image.isNull()) {
        auto& cu = CurrentUser::instance();
        GroupRole role = GroupRole::Admin;  // 默认为普通成员

        if (isGroupMode) {
            // 如果是群聊，直接查询数据库获取用户身份
            role = GroupRepository::instance().getMemberRole(messageId.toInt(), cu.getUserId());
        }

        auto ptr = QSharedPointer <ImageMessage>::create(image, true, cu.getUserId(), isGroupMode, cu.getUserName(), role);

        addMessage(ptr);
    }
}

void ChatArea::onSendText(const QString& text) {
    if (text.isEmpty()) {
        return;
    }

    auto& cu = CurrentUser::instance();
    GroupRole role = GroupRole::Admin;  // 默认为普通成员

    if (isGroupMode) {
        // 如果是群聊，直接查询数据库获取用户身份
        role = GroupRepository::instance().getMemberRole(messageId.toInt(), cu.getUserId());
    }

    // 创建文本消息
    QString currentUserId = CurrentUser::instance().getUserId();
    auto message = QSharedPointer <TextMessage>(new TextMessage(text, true, currentUserId, isGroupMode, CurrentUser::instance().getUserName(), role));

    message->setTimestamp(QDateTime::currentDateTime());

    // 1. 添加到UI并存储到本地数据库
    addMessage(message);

    // 2. 通过WebSocket发送消息
    QJsonObject payload;

    payload["to"] = messageId;  // messageId是当前聊天对象的ID（用户ID或群ID）
    payload["content"] = text;
    payload["type"] = "text";
    payload["extra"] = "{}";
    payload["is_group"] = isGroupMode;

    QJsonObject wsMessage;

    wsMessage["type"] = "chat";
    wsMessage["payload"] = payload;

    QString jsonString = QJsonDocument(wsMessage).toJson(QJsonDocument::Compact);

    NetworkManager::instance().sendWsMessage(jsonString);
}

void ChatArea::clearAll() {
    chatModel->clear();
}

void ChatArea::setMessageId(QString id) {
    messageId = id;

    // 通知MessageHandler当前聊天ID
    MessageHandler::instance().setCurrentChatId(id);

    if (isGroupMode) {
        auto group = GroupRepository::instance().getGroup(id);

        statusIcon->hide();

        QString fullName("%1（%2）");

        nameLabel->setText(fullName.arg(group.groupName, QString::number(group.memberNum)));
    } else {
        auto user = UserRepository::instance().getUser(id);

        statusIcon->show();

        QPixmap pixmap(QPixmap(statusIconPath(user.status)));

        statusIcon->setPixmap(pixmap.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        nameLabel->setText(user.nick);
    }
}

void ChatArea::initMessage(QVector <ChatArea::ChatMessagePtr>& messages) {
    clearAll();

    for (auto message : messages) {
        chatModel->addMessage(message);
    }
    adjustBottomSpace();
}

void ChatArea::onMessageReceived(ChatArea::ChatMessagePtr message) {
    bool shouldScroll = message->isFromMe() || isNearBottom();

    // 添加消息
    chatModel->addMessage(message);

    // 调整底部空白
    adjustBottomSpace();

    // 根据情况处理滚动和未读计数
    if (!message->isFromMe() && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }

    // 如果需要滚动到底部，使用延时确保布局更新完成
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}
