
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "AiChatItemDelegate.h"
#include "AiChatWindow.h"

#include "MainWindow.h"

#include "NotificationManager.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

AiChatWindow::AiChatWindow(QWidget*parent) : QWidget(parent), m_model(new AiChatListModel(this)), m_currentConversationId("") {
    setupUI();

    auto* message = new AiChatMessage(AiChatMessage::AI, "您好！我是DeepSeek，很高兴见到您！", "");

    m_model->appendMessage(message);
    m_webSocket = new AiChatWebSocket(this);
    connect(m_webSocket, &AiChatWebSocket::messageContent, this, &AiChatWindow::onMessageContent);
    connect(m_webSocket, &AiChatWebSocket::conversationStarted, this, &AiChatWindow::onConversationStarted);
    connect(m_webSocket, &AiChatWebSocket::messageEnded, this, &AiChatWindow::onMessageEnded);
    connect(m_webSocket, &AiChatWebSocket::connectionEstablished, this, &AiChatWindow::onConnectionEstablished);
    connect(m_webSocket, &AiChatWebSocket::connectionError, this, &AiChatWindow::onConnectionError);
    connect(m_webSocket, &AiChatWebSocket::requestError, this, &AiChatWindow::onRequestError);
    m_webSocket->connectToServer();
}

void AiChatWindow::setupUI() {
    // 1. 创建主布局，和示例保持一致
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 2. 创建聊天信息区 chatInfo（对应示例中的 ChatInfo 部分）
    QWidget* chatInfo = new QWidget(this);

    chatInfo->setFixedHeight(24 + 26 + 12);
    chatInfo->setObjectName("AiChatInfo");
    chatInfo->setStyleSheet("#AiChatInfo {""    background-color: #F2F2F2;""    border-bottom: 1px solid #E9E9E9;""}");

    QVBoxLayout* outerLayout = new QVBoxLayout(chatInfo);

    outerLayout->setContentsMargins(20, 5, 5, 10);
    outerLayout->addStretch();

    // 内层水平布局：图标 + 名字（与示例保持一致）
    QHBoxLayout* bottomLayout = new QHBoxLayout();

    bottomLayout->setSpacing(5);    // 图标与文字间距
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* statusIcon = new QLabel(chatInfo);

    statusIcon->setPixmap(QPixmap(":/icon/online.png").scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel* nameLabel = new QLabel("DeepSeek", chatInfo);

    nameLabel->setStyleSheet("font-size: 17px; color: #000000;");

    // 将 nameLabel 和 statusIcon 添加到底部布局
    bottomLayout->addWidget(nameLabel);
    bottomLayout->addWidget(statusIcon);
    bottomLayout->addStretch(); // 保证靠左
    // 把底部布局添加到外层布局中
    outerLayout->addLayout(bottomLayout);

    // **关键：为 chatInfo 设置 layout**
    chatInfo->setLayout(outerLayout);

    // 3. 创建聊天视图 m_chatView，并设置 model/delegate
    m_chatView = new AiChatListView(this);
    m_chatView->setModel(m_model);
    m_chatView->setItemDelegate(new AiChatItemDelegate(m_chatView));
    m_chatView->setSpacing(2);  // 如果需要和示例对齐，可以加上这一行
    // 如果想给 m_chatView 设置背景色，可以像示例一样：
    m_chatView->setStyleSheet("QListView {""    background-color: #F2F2F2;""}");

    // 4. 创建新消息提示、悬浮输入栏等（根据你现有逻辑保留）
    // 假设这里你已经在类成员里声明了 m_inputBar
    m_inputBar = new FloatingInputBar(this);
    connect(m_inputBar, &FloatingInputBar::sendText, this, &AiChatWindow::onSendMessage);

    // 5. 将 chatInfo 和 m_chatView 依次添加到主布局
    m_mainLayout->addWidget(chatInfo);
    m_mainLayout->addWidget(m_chatView);

    // 6. 最后调用 setLayout，保证整个窗口使用 m_mainLayout
    setLayout(m_mainLayout);
    setMinimumSize(400, 600);
}

void AiChatWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateInputBarPosition();
}

void AiChatWindow::updateInputBarPosition() {
    if (m_inputBar) {
        // 计算输入栏的大小和位置
        int inputBarWidth = width() - 2 * INPUT_BAR_MARGIN;
        int inputBarHeight = 175; // 固定高度
        int x = INPUT_BAR_MARGIN;
        int y = height() - inputBarHeight - INPUT_BAR_MARGIN;

        m_inputBar->setGeometry(x, y, inputBarWidth, inputBarHeight);
        m_inputBar->raise(); // 确保输入栏在最上层
    }
}

void AiChatWindow::onSendMessage(const QString& content) {
    if (content.isEmpty()) {
        return;
    }

    auto* message = new AiChatMessage(AiChatMessage::User, content, "");

    m_model->appendMessage(message);
    m_webSocket->sendMessage(content, m_currentConversationId);

    // 滚动到底部
    m_chatView->scrollToBottom();
}

void AiChatWindow::onMessageContent(const QString& content) {
    qDebug() << "添加AI回复内容:" << content;
    m_model->appendContentToLastMessage(content);
    m_chatView->scrollToBottom();
}

void AiChatWindow::onConversationStarted(const QString& conversationId) {
    qDebug() << "收到会话ID:" << conversationId;
    m_currentConversationId = conversationId;

    // 创建AI的初始空消息
    auto* message = new AiChatMessage(AiChatMessage::AI, "", "");

    m_model->appendMessage(message);
}

void AiChatWindow::onMessageEnded() {
    qDebug() << "AI回复结束";
}

void AiChatWindow::onConnectionEstablished() {
    qDebug() << "WebSocket连接已建立";
}

void AiChatWindow::onConnectionError(const QString& error) {
    qDebug() << "WebSocket连接错误:" << error;
    NotificationManager::instance().showMessage(tr("连接错误: %1").arg(error), NotificationManager::Error, MainWindow::getInstance());
}

void AiChatWindow::onRequestError(const QString& errorMessage) {
    qDebug() << "请求错误:" << errorMessage;
    NotificationManager::instance().showMessage(tr("请求错误: %1").arg(errorMessage), NotificationManager::Error, MainWindow::getInstance());
}
