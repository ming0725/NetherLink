#include "ChatArea.h"
#include "shared/services/AppFonts.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/chat/ui/ConversationInfoPanel.h"
#include "features/chat/ui/ChatSessionController.h"
#include "features/friend/ui/FriendProfilePopup.h"
#include "features/friend/ui/AddContactSearchWindow.h"
#include "features/friend/ui/FriendSessionController.h"
#include "app/state/CurrentUser.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/InWindowPopupOverlay.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/QtFallbackLiquidGlass.h"
#include "shared/ui/StyledActionMenu.h"
#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif
#include <QAction>
#include <QPainter>
#include <QPalette>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDateTime>
#include <QFont>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

namespace {

// Floating input bar metrics. Keep list bottom spacing in sync with these values.
static constexpr int kChatInfoHeight = 62;
static constexpr int kInputBarSideMargin = 20;
static constexpr int kInputBarBottomMargin = 18;
static constexpr int kInputBarHeight = 195;
static constexpr int kChatListBottomSpacePadding = 10;
static constexpr int kNewMessageNotifierInputGap = 10;
static constexpr int kInputBarBottomGradientFadeHeight = 32;
static constexpr int kInputBarBottomGradientSolidAlpha = 192;
static constexpr int kOlderMessagePageSize = 24;
static constexpr int kFetchOlderTopThreshold = 8;
#ifdef Q_OS_WIN
static constexpr int kOlderMessageTriggerCooldownMs = 160;
#endif
static constexpr int kMessageRecallReeditSeconds = 120;
static constexpr int kInfoButtonSize = 28;
static constexpr int kInfoPanelDividerHeight = 1;
static constexpr int kInfoPanelMinWidth = 220;
static constexpr int kInfoPanelPreferredWidth = 280;
static constexpr int kInfoPanelMaxWidth = 340;
static constexpr int kInfoPanelAnimationDuration = 220;
static constexpr int kChatInfoRightMargin = 18;
static constexpr int kHistoryUnreadScrollDelayMs = 16;
static constexpr int kHistoryUnreadNotifierLoadDelayMs = 120;

static constexpr int kMessageLoadingSkeletonFrameMs = 40;
static constexpr int kNewMessageNotifierMinBottomDistance = 220;
static constexpr int kNewMessageNotifierViewportDistanceDivisor = 2;

class SquareDotsButton : public QPushButton
{
public:
    explicit SquareDotsButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFlat(true);
        setFixedSize(kInfoButtonSize, kInfoButtonSize);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        if (isDown()) {
            painter.setBrush(ThemeManager::instance().color(ThemeColor::Divider));
        } else if (underMouse()) {
            painter.setBrush(ThemeManager::instance().color(ThemeColor::ListHover));
        } else {
            painter.setBrush(Qt::transparent);
        }
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

        painter.setBrush(ThemeManager::instance().color(ThemeColor::SecondaryText));
        const int dotSize = 4;
        const int spacing = 4;
        const int totalWidth = dotSize * 3 + spacing * 2;
        const int startX = (width() - totalWidth) / 2;
        const int y = (height() - dotSize) / 2;
        for (int i = 0; i < 3; ++i) {
            painter.drawRect(startX + i * (dotSize + spacing), y, dotSize, dotSize);
        }
    }
};

class BottomGapGradientOverlay : public QWidget
{
public:
    explicit BottomGapGradientOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        hide();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
        const qreal fadeStop = rect().height() > 0
                ? qBound(0.0,
                         static_cast<qreal>(kInputBarBottomGradientFadeHeight) / rect().height(),
                         1.0)
                : 1.0;
        QColor pageBackground = ThemeManager::instance().color(ThemeColor::PageBackground);
        pageBackground.setAlpha(0);
        gradient.setColorAt(0.0, pageBackground);
        pageBackground.setAlpha(kInputBarBottomGradientSolidAlpha);
        gradient.setColorAt(fadeStop, pageBackground);
        gradient.setColorAt(1.0, pageBackground);
        painter.fillRect(rect(), gradient);
    }
};

void applyThemedFill(QWidget* widget, ThemeColor role)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(role));
    widget->setAutoFillBackground(true);
    widget->setPalette(palette);
}

void applyPrimaryText(PaintedLabel* label)
{
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
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

QString groupMemberDisplayName(const Group& group, const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
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
    return userId;
}

QString simulatedPeerIdForGroup(const Group& group)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    if (!currentUser.isCurrentUserId(group.ownerId)) {
        return group.ownerId;
    }
    for (const QString& adminId : group.adminsID) {
        if (!currentUser.isCurrentUserId(adminId)) {
            return adminId;
        }
    }
    for (const QString& memberId : group.membersID) {
        if (!currentUser.isCurrentUserId(memberId)) {
            return memberId;
        }
    }
    return {};
}

QString groupRoleLabel(GroupRole role)
{
    switch (role) {
    case GroupRole::Owner:
        return QStringLiteral("群主");
    case GroupRole::Admin:
        return QStringLiteral("管理员");
    case GroupRole::Member:
    default:
        return QStringLiteral("群成员");
    }
}

QString directRecallText(bool originalFromMe, bool actorIsCurrentUser)
{
    if (originalFromMe && actorIsCurrentUser) {
        return QStringLiteral("你撤回了一条消息");
    }
    return QStringLiteral("对方撤回了一条消息");
}

QString groupRecallText(const QString& actorName,
                        GroupRole actorRole,
                        const QString& senderName,
                        GroupRole senderRole,
                        bool moderatorRecall)
{
    if (moderatorRecall) {
        return QStringLiteral("%1 %2 撤回了一条群成员消息")
                .arg(groupRoleLabel(actorRole), actorName);
    }
    return QStringLiteral("%1 %2 撤回了一条消息")
            .arg(groupRoleLabel(senderRole), senderName);
}

} // namespace

ChatArea::ChatArea(QWidget *parent)
        : QWidget(parent)
{
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
    chatDelegate->setRecallEligibilityCallback([this](const ChatMessage* message) {
        return canRecallMessage(message);
    });
    
    // 设置模型和代理
    chatView->setModel(chatModel);
    chatView->setItemDelegate(chatDelegate);
    
    // 创建未读提示组件
    historyUnreadNotifier = new HistoryUnreadNotifier(this);
    historyUnreadNotifier->hide();
    historyUnreadNotifierLoadTimer = new QTimer(this);
    historyUnreadNotifierLoadTimer->setSingleShot(true);
    historyUnreadNotifierLoadTimer->setInterval(kHistoryUnreadNotifierLoadDelayMs);
    messageLoadingAnimationTimer = new QTimer(this);
    messageLoadingAnimationTimer->setInterval(kMessageLoadingSkeletonFrameMs);

    newMessageNotifier = new NewMessageNotifier(this);
    newMessageNotifier->hide();

    bottomGapGradientOverlay = new BottomGapGradientOverlay(this);
    sessionController = new ChatSessionController(this);
    friendProfileController = new FriendSessionController(this);
    friendProfilePopup = new FriendProfilePopup(this);
    friendProfilePopup->setController(friendProfileController);

    infoPanelAnimation = new QPropertyAnimation(this);
    infoPanelAnimation->setPropertyName("geometry");
    infoPanelAnimation->setDuration(kInfoPanelAnimationDuration);
    infoPanelAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(infoPanelAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (!infoPanelOpen) {
            releaseInfoPanels();
        }
        updateInputBarPosition();
        adjustBottomSpace();
    });
    connect(infoPanelAnimation, &QPropertyAnimation::valueChanged, this, [this]() {
        updateInputBarPosition();
        adjustBottomSpace();
        if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
            panel->raise();
        }
    });

    // 创建悬浮输入栏
    inputBar = new FloatingInputBar(this);
    inputBar->setLiquidGlassSourceWidget(chatView->viewport());
    inputBar->hide();

    QWidget* chatInfo = new QWidget(this);
    chatInfo->setFixedHeight(kChatInfoHeight);
    QWidget* chatInfoDivider = new QWidget(this);
    chatInfoDivider->setFixedHeight(1);

    // 外层垂直布局：用于将内容推到底部
    QVBoxLayout* outerLayout = new QVBoxLayout(chatInfo);
    outerLayout->setContentsMargins(20, 5, kChatInfoRightMargin, 10); // 留出左右下边距
    outerLayout->addStretch(); // 将内容推到底部

    // 内层水平布局：图标 + 名字
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(5); // 图标与文字间距
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 在线状态图标
    statusIcon = new QLabel(chatInfo);

    // 名字 Label
    nameLabel = new PaintedLabel(chatInfo);
    QFont nameFont = nameLabel->font();
    nameFont.setPixelSize(17);
    nameLabel->setFont(nameFont);
    auto applyHeaderTheme = [this, chatInfo, chatInfoDivider]() {
        applyPrimaryText(nameLabel);
        applyThemedFill(chatInfo, ThemeColor::PageBackground);
        applyThemedFill(chatInfoDivider, ThemeColor::Divider);
        if (chatView) {
            chatView->viewport()->update();
        }
    };
    applyHeaderTheme();

    infoButton = new SquareDotsButton(chatInfo);

    // 添加到底部布局
    bottomLayout->addWidget(nameLabel);
    bottomLayout->addWidget(statusIcon);
    bottomLayout->addStretch(); // 保证名称靠左，入口靠右
    bottomLayout->addWidget(infoButton);

    // 添加到底部区域
    outerLayout->addLayout(bottomLayout);

    chatInfo->setLayout(outerLayout);
    
    // 添加到主布局
    mainLayout->addWidget(chatInfo);
    mainLayout->addWidget(chatInfoDivider);
    mainLayout->addWidget(chatView);
    setLayout(mainLayout);

    // 连接信号槽
    connect(chatView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ChatArea::onScrollValueChanged);
    connect(chatView, &ChatListView::userScrollUpIntent, this, [this]() {
        m_state.newMessageNotifierRevealedByDownScroll = false;
        updateNewMessageNotifier();
    });
    connect(chatView, &ChatListView::userScrollDownIntent, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            if (m_state.newUnreadMessageCount == 0 && shouldShowNewMessageNotifier()) {
                m_state.newMessageNotifierRevealedByDownScroll = true;
                updateNewMessageNotifier();
            }
        });
    });
    connect(historyUnreadNotifier, &HistoryUnreadNotifier::clicked,
            this, &ChatArea::scrollToFirstHistoryUnread);
    connect(historyUnreadNotifierLoadTimer, &QTimer::timeout,
            this, &ChatArea::showHistoryUnreadNotifier);
    connect(messageLoadingAnimationTimer, &QTimer::timeout, this, [this]() {
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
    connect(newMessageNotifier, &NewMessageNotifier::clicked,
            this, &ChatArea::onNewMessageNotifierClicked);
    connect(chatView, &ChatListView::avatarClicked,
            this, &ChatArea::showFriendProfilePopup);
    connect(chatView, &ChatListView::avatarContextMenuRequested,
            this, &ChatArea::showAvatarContextMenu);
    connect(friendProfilePopup, &FriendProfilePopup::requestMessage,
            this, &ChatArea::requestOpenConversation);
    connect(friendProfilePopup, &FriendProfilePopup::requestAddFriend,
            this, [this](const QString& userId) {
                AddContactSearchWindow::openUserRequest(userId, this);
            });
    connect(inputBar, &FloatingInputBar::sendImage,
            this, &ChatArea::onSendImage);
    connect(inputBar, &FloatingInputBar::sendText,
            this, &ChatArea::onSendText);
    connect(inputBar, &FloatingInputBar::sendTextAsPeer,
            this, &ChatArea::onSendTextAsPeer);
    connect(inputBar, &FloatingInputBar::recallLatestPeerMessageRequested,
            this, &ChatArea::onRecallLatestPeerMessageRequested);
    connect(inputBar, &FloatingInputBar::inputFocused,
            this, &ChatArea::clearMessageSelection);
    connect(chatDelegate, &ChatItemDelegate::deleteRequested,
            this, &ChatArea::onDeleteMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::recallRequested,
            this, &ChatArea::onRecallMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::reeditRequested,
            this, &ChatArea::onReeditMessageRequested);
    connect(infoButton, &QPushButton::clicked,
            this, &ChatArea::onInfoButtonClicked);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, applyHeaderTheme);
    connect(sessionController, &ChatSessionController::sessionChanged,
            this, [this](const ConversationMeta& meta, const User&, const Group&) {
                onSessionChanged(meta);
            });
    connect(&MessageRepository::instance(), &MessageRepository::lastMessageChanged,
            this, &ChatArea::appendRepositoryMessage);
    connect(sessionController, &ChatSessionController::directPanelDataLoaded,
            this, &ChatArea::onDirectPanelDataLoaded);
    connect(sessionController, &ChatSessionController::groupPanelDataLoaded,
            this, &ChatArea::onGroupPanelDataLoaded);
    connect(sessionController, &ChatSessionController::groupMembersPageLoaded,
            this, &ChatArea::onGroupMembersPageLoaded);
    connect(sessionController, &ChatSessionController::messagesCleared,
            this, &ChatArea::onSessionMessagesCleared);
    connect(sessionController, &ChatSessionController::conversationRemoved,
            this, &ChatArea::onSessionConversationRemoved);

}


void ChatArea::addMessage(QSharedPointer<ChatMessage> message)
{
    if (!message || conversationId().isEmpty()) {
        return;
    }

    const bool isOwnMessage = message->isFromMe();
    const bool shouldFollowIncomingMessage = chatView->isBottomLocked();
    // 添加消息
    chatModel->addMessage(message);
    assignAppendedPeerMessageOrdinal(message);
    MessageRepository::instance().addMessage(conversationId(), message);
    ++m_state.loadedMessageCount;

    // 调整底部空白
    adjustBottomSpace();

    // 根据情况处理滚动和未读计数
    if (!isOwnMessage && !shouldFollowIncomingMessage) {
        registerNewUnreadCandidate(message);
        updateNewMessageNotifier();
    }

    if (isOwnMessage) {
        QTimer::singleShot(0, this, [this]() {
            scrollToBottom(true);
        });
        return;
    }

    // 如果需要滚动到底部，使用延时确保布局更新完成
    if (shouldFollowIncomingMessage) {
        QTimer::singleShot(0, this, [this, isOwnMessage, message]() {
            if (chatView->scrollToBottomIfLocked(isOwnMessage)) {
                scheduleVisibleUnreadCheck();
            } else if (!isOwnMessage) {
                registerNewUnreadCandidate(message);
                updateNewMessageNotifier();
            }
        });
    }
}

void ChatArea::appendRepositoryMessage(const QString& changedConversationId,
                                       const ChatMessagePtr& message)
{
    if (changedConversationId.isEmpty() ||
            changedConversationId != conversationId() ||
            m_state.loadingInitialMessages ||
            !message ||
            !chatModel ||
            chatModel->indexForMessage(message.get()).isValid()) {
        return;
    }

    const bool isOwnMessage = message->isFromMe();
    const bool shouldFollowIncomingMessage = chatView->isBottomLocked();
    chatModel->addMessage(message);
    assignAppendedPeerMessageOrdinal(message);
    ++m_state.loadedMessageCount;
    adjustBottomSpace();

    if (!isOwnMessage && !shouldFollowIncomingMessage) {
        registerNewUnreadCandidate(message);
        updateNewMessageNotifier();
    }

    if (isOwnMessage) {
        QTimer::singleShot(0, this, [this]() {
            scrollToBottom(true);
        });
        return;
    }

    if (shouldFollowIncomingMessage) {
        QTimer::singleShot(0, this, [this, isOwnMessage, message]() {
            if (chatView->scrollToBottomIfLocked(isOwnMessage)) {
                scheduleVisibleUnreadCheck();
            } else if (!isOwnMessage) {
                registerNewUnreadCandidate(message);
                updateNewMessageNotifier();
            }
        });
    }
}


void ChatArea::addImageMessage(QSharedPointer<ImageMessage> message,
                               const QDateTime& timestamp)
{
    if (!message) {
        return;
    }
    message->setTimestamp(timestamp);
    addMessage(std::move(message));
}

void ChatArea::onScrollValueChanged(int)
{
    if (inputBar) {
        inputBar->scheduleLiquidGlassInteractiveUpdate();
    }
    m_state.isAtBottom = isScrollAtBottom();

#ifdef Q_OS_WIN
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool olderMessageTriggerInCooldown =
            nowMs < m_state.olderMessageTriggerCooldownUntilMs;
#endif
    if (m_state.allowOlderMessageFetch &&
        m_state.hasMoreBefore &&
        !m_state.loadingOlderMessages &&
#ifdef Q_OS_WIN
        !olderMessageTriggerInCooldown &&
#endif
        chatView->verticalScrollBar()->value() <= kFetchOlderTopThreshold) {
#ifdef Q_OS_WIN
        chatView->stopAnimatedWheelScroll();
        m_state.olderMessageTriggerCooldownUntilMs =
                nowMs + kOlderMessageTriggerCooldownMs;
#endif
        loadOlderMessages();
    }

    scheduleVisibleUnreadCheck();
    updateNewMessageNotifier();
}



bool ChatArea::isScrollAtBottom() const
{
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    return scrollBar->value() >= scrollBar->maximum() - 5;
}

int ChatArea::registerHistoryUnreadCandidates(const ChatMessageList& messages, int maxCount)
{
    if (maxCount <= 0) {
        return 0;
    }

    int registeredCount = 0;
    int firstOrdinal = 0;
    int lastOrdinal = -1;
    for (auto it = messages.crbegin(); it != messages.crend() && registeredCount < maxCount; ++it) {
        const ChatMessagePtr& message = *it;
        if (!message || message->isFromMe()) {
            continue;
        }

        const auto ordinalIt = m_state.peerMessageOrdinals.constFind(message.get());
        if (ordinalIt == m_state.peerMessageOrdinals.cend()) {
            continue;
        }

        const int ordinal = ordinalIt.value();
        if (lastOrdinal < firstOrdinal) {
            firstOrdinal = ordinal;
            lastOrdinal = ordinal;
        } else {
            firstOrdinal = qMin(firstOrdinal, ordinal);
            lastOrdinal = qMax(lastOrdinal, ordinal);
        }
        ++registeredCount;
    }

    if (registeredCount > 0) {
        extendHistoryUnreadRange(firstOrdinal, lastOrdinal);
    }
    return registeredCount;
}

bool ChatArea::registerNewUnreadCandidate(const ChatMessagePtr& message)
{
    if (!message || message->isFromMe()) {
        return false;
    }

    const auto ordinalIt = m_state.peerMessageOrdinals.constFind(message.get());
    if (ordinalIt == m_state.peerMessageOrdinals.cend()) {
        return false;
    }

    const int ordinal = ordinalIt.value();
    if (!m_state.hasNewUnreadOrdinalRange) {
        m_state.hasNewUnreadOrdinalRange = true;
        m_state.newUnreadFirstOrdinal = ordinal;
        m_state.newUnreadLastOrdinal = ordinal;
        m_state.newReadMaxOrdinal = ordinal - 1;
    } else {
        m_state.newUnreadFirstOrdinal = qMin(m_state.newUnreadFirstOrdinal, ordinal);
        m_state.newUnreadLastOrdinal = qMax(m_state.newUnreadLastOrdinal, ordinal);
    }
    recalculateNewUnreadCount();
    return true;
}

void ChatArea::scheduleVisibleUnreadCheck()
{
    if (m_state.visibleUnreadCheckScheduled) {
        return;
    }

    m_state.visibleUnreadCheckScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_state.visibleUnreadCheckScheduled = false;
        updateVisibleUnreadMessages();
    });
}

void ChatArea::updateVisibleUnreadMessages()
{
    if (conversationId().isEmpty() ||
            (m_state.historyUnreadMessageCount <= 0 && m_state.newUnreadMessageCount <= 0) ||
            m_state.loadingInitialMessages ||
            m_state.loadingOlderMessages) {
        return;
    }

    const std::optional<int> firstVisibleOrdinal = firstVisiblePeerOrdinal();
    if (m_state.hasHistoryUnreadOrdinalRange &&
            firstVisibleOrdinal.has_value() &&
            firstVisibleOrdinal.value() <= m_state.historyUnreadLastOrdinal &&
            firstVisibleOrdinal.value() < m_state.historyReadMinOrdinal) {
        m_state.historyReadMinOrdinal = qMax(m_state.historyUnreadFirstOrdinal,
                                             firstVisibleOrdinal.value());
        recalculateHistoryUnreadCount();
    }

    const std::optional<int> lastVisibleOrdinal = lastVisiblePeerOrdinal();
    if (m_state.hasNewUnreadOrdinalRange &&
            lastVisibleOrdinal.has_value() &&
            lastVisibleOrdinal.value() >= m_state.newUnreadFirstOrdinal &&
            lastVisibleOrdinal.value() > m_state.newReadMaxOrdinal) {
        m_state.newReadMaxOrdinal = qMin(m_state.newUnreadLastOrdinal,
                                         lastVisibleOrdinal.value());
        recalculateNewUnreadCount();
    }

    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
}

QRect ChatArea::effectiveMessageViewportRect() const
{
    if (!chatView || !chatView->viewport()) {
        return {};
    }

    QRect effectiveViewport = chatView->viewport()->rect();
    if (inputBar && inputBar->isVisible()) {
        const int inputTopInViewport = chatView->viewport()->mapFrom(this, inputBar->pos()).y();
        if (inputTopInViewport > 0) {
            effectiveViewport.setBottom(qMin(effectiveViewport.bottom(), inputTopInViewport - 6));
        }
    }
    return effectiveViewport;
}

std::optional<int> ChatArea::firstVisiblePeerOrdinal() const
{
    if (!chatModel || !chatView || !chatView->viewport()) {
        return std::nullopt;
    }

    const QRect viewportRect = effectiveMessageViewportRect();
    if (viewportRect.isEmpty()) {
        return std::nullopt;
    }

    QModelIndex index = chatView->indexAt(QPoint(viewportRect.center().x(), viewportRect.top() + 1));
    if (!index.isValid()) {
        index = chatView->indexAt(QPoint(viewportRect.left() + 1, viewportRect.top() + 1));
    }
    if (!index.isValid()) {
        return std::nullopt;
    }

    return peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrAfter(index.row()));
}

std::optional<int> ChatArea::lastVisiblePeerOrdinal() const
{
    if (!chatModel || !chatView || !chatView->viewport()) {
        return std::nullopt;
    }

    const QRect viewportRect = effectiveMessageViewportRect();
    if (viewportRect.isEmpty()) {
        return std::nullopt;
    }

    QModelIndex index = chatView->indexAt(QPoint(viewportRect.center().x(), viewportRect.bottom() - 1));
    if (!index.isValid()) {
        index = chatView->indexAt(QPoint(viewportRect.left() + 1, viewportRect.bottom() - 1));
    }
    if (!index.isValid()) {
        return std::nullopt;
    }

    return peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrBefore(index.row()));
}

std::optional<int> ChatArea::peerOrdinalForRow(int row) const
{
    if (!chatModel || row < 0) {
        return std::nullopt;
    }

    const ChatMessage* message = chatModel->messageAt(row);
    const auto it = m_state.peerMessageOrdinals.constFind(message);
    return it == m_state.peerMessageOrdinals.cend()
            ? std::nullopt
            : std::optional<int>(it.value());
}

int ChatArea::historyUnreadJumpRow(int unreadCount) const
{
    if (!chatModel || unreadCount <= 0) {
        return -1;
    }

    if (m_state.hasHistoryUnreadOrdinalRange) {
        for (int row = 0; row < chatModel->rowCount(); ++row) {
            const std::optional<int> ordinal = peerOrdinalForRow(row);
            if (ordinal.has_value() &&
                    ordinal.value() >= m_state.historyUnreadFirstOrdinal &&
                    ordinal.value() <= m_state.historyUnreadLastOrdinal &&
                    ordinal.value() < m_state.historyReadMinOrdinal) {
                return row;
            }
        }
    }

    if (m_state.historyUnloadedUnreadMessageCount > 0) {
        return -1;
    }

    int remainingUnread = unreadCount;
    int oldestAvailablePeerRow = -1;
    for (int row = chatModel->rowCount() - 1; row >= 0; --row) {
        const ChatMessage* message = chatModel->messageAt(row);
        if (!message || message->isFromMe()) {
            continue;
        }

        oldestAvailablePeerRow = row;
        --remainingUnread;
        if (remainingUnread <= 0) {
            return row;
        }
    }

    return oldestAvailablePeerRow;
}

void ChatArea::assignInitialPeerMessageOrdinals(const ChatMessageList& messages)
{
    m_state.peerMessageOrdinals.clear();
    int ordinal = 0;
    for (const ChatMessagePtr& message : messages) {
        if (!message || message->isFromMe()) {
            continue;
        }
        m_state.peerMessageOrdinals.insert(message.get(), ordinal++);
    }
    m_state.minPeerMessageOrdinal = 0;
    m_state.maxPeerMessageOrdinal = ordinal - 1;
}

void ChatArea::assignPrependedPeerMessageOrdinals(const ChatMessageList& messages)
{
    int peerCount = 0;
    for (const ChatMessagePtr& message : messages) {
        if (message && !message->isFromMe()) {
            ++peerCount;
        }
    }
    if (peerCount <= 0) {
        return;
    }

    int ordinal = m_state.peerMessageOrdinals.isEmpty()
            ? 0
            : m_state.minPeerMessageOrdinal - peerCount;
    for (const ChatMessagePtr& message : messages) {
        if (!message || message->isFromMe()) {
            continue;
        }
        m_state.peerMessageOrdinals.insert(message.get(), ordinal++);
    }
    m_state.minPeerMessageOrdinal = m_state.peerMessageOrdinals.isEmpty()
            ? 0
            : ordinal - peerCount;
    if (m_state.maxPeerMessageOrdinal < m_state.minPeerMessageOrdinal) {
        m_state.maxPeerMessageOrdinal = ordinal - 1;
    }
}

void ChatArea::assignAppendedPeerMessageOrdinal(const ChatMessagePtr& message)
{
    if (!message || message->isFromMe()) {
        return;
    }

    const int ordinal = m_state.peerMessageOrdinals.isEmpty()
            ? 0
            : m_state.maxPeerMessageOrdinal + 1;
    m_state.peerMessageOrdinals.insert(message.get(), ordinal);
    if (m_state.peerMessageOrdinals.size() == 1) {
        m_state.minPeerMessageOrdinal = ordinal;
    }
    m_state.maxPeerMessageOrdinal = ordinal;
}

void ChatArea::extendHistoryUnreadRange(int firstOrdinal, int lastOrdinal)
{
    if (lastOrdinal < firstOrdinal) {
        return;
    }

    if (!m_state.hasHistoryUnreadOrdinalRange) {
        m_state.hasHistoryUnreadOrdinalRange = true;
        m_state.historyUnreadFirstOrdinal = firstOrdinal;
        m_state.historyUnreadLastOrdinal = lastOrdinal;
        m_state.historyReadMinOrdinal = lastOrdinal + 1;
    } else {
        m_state.historyUnreadFirstOrdinal = qMin(m_state.historyUnreadFirstOrdinal, firstOrdinal);
        m_state.historyUnreadLastOrdinal = qMax(m_state.historyUnreadLastOrdinal, lastOrdinal);
        m_state.historyReadMinOrdinal = qMax(m_state.historyReadMinOrdinal,
                                             m_state.historyUnreadFirstOrdinal);
    }
    recalculateHistoryUnreadCount();
}

void ChatArea::recalculateHistoryUnreadCount()
{
    int loadedUnreadCount = 0;
    if (m_state.hasHistoryUnreadOrdinalRange) {
        const int unreadLastOrdinal = qMin(m_state.historyUnreadLastOrdinal,
                                           m_state.historyReadMinOrdinal - 1);
        loadedUnreadCount = qMax(0, unreadLastOrdinal - m_state.historyUnreadFirstOrdinal + 1);
    }
    m_state.historyUnreadMessageCount = loadedUnreadCount +
            qMax(0, m_state.historyUnloadedUnreadMessageCount);
    if (m_state.historyUnreadMessageCount == 0) {
        m_state.hasHistoryUnreadOrdinalRange = false;
        m_state.historyUnloadedUnreadMessageCount = 0;
    }
}

void ChatArea::recalculateNewUnreadCount()
{
    if (!m_state.hasNewUnreadOrdinalRange) {
        m_state.newUnreadMessageCount = 0;
        return;
    }

    const int firstUnreadOrdinal = qMax(m_state.newUnreadFirstOrdinal,
                                        m_state.newReadMaxOrdinal + 1);
    m_state.newUnreadMessageCount = qMax(0, m_state.newUnreadLastOrdinal - firstUnreadOrdinal + 1);
    if (m_state.newUnreadMessageCount == 0) {
        m_state.hasNewUnreadOrdinalRange = false;
    }
}

void ChatArea::reconcileHistoryUnreadAfterHistoryExhausted()
{
    if (m_state.hasMoreBefore || m_state.historyUnloadedUnreadMessageCount <= 0) {
        return;
    }

    m_state.historyUnloadedUnreadMessageCount = 0;
    recalculateHistoryUnreadCount();
}

void ChatArea::onNewMessageNotifierClicked()
{
    m_state.newMessageNotifierRevealedByDownScroll = false;
    scrollToBottom(true);
}

void ChatArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateInfoPanelGeometry();
    updateHistoryUnreadNotifierPosition();
    updateNewMessageNotifierPosition();
    updateInputBarPosition();
    adjustBottomSpace();
    scheduleVisibleUnreadCheck();

    // 确保新消息提示器在最上层
    if (historyUnreadNotifier && historyUnreadNotifier->isVisible()) {
        historyUnreadNotifier->raise();
    }
    if (newMessageNotifier && newMessageNotifier->isVisible()) {
        newMessageNotifier->raise();
    }
    if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
        panel->raise();
    }
}

void ChatArea::updateNewMessageNotifier()
{
    if (!newMessageNotifier) {
        return;
    }

    if (isScrollAtBottom()) {
        m_state.newMessageNotifierRevealedByDownScroll = false;
        if (m_state.newUnreadMessageCount <= 0) {
            newMessageNotifier->hide();
            return;
        }
    }

    if (m_state.newUnreadMessageCount > 0) {
        newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::Count);
        newMessageNotifier->setMessageCount(m_state.newUnreadMessageCount);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        return;
    }

    if (m_state.newMessageNotifierRevealedByDownScroll && shouldShowNewMessageNotifier()) {
        newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        return;
    }

    newMessageNotifier->hide();
}

bool ChatArea::shouldShowNewMessageNotifier() const
{
    if (!chatView ||
            !inputBar ||
            conversationId().isEmpty() ||
            chatView->isHidden() ||
            inputBar->isHidden()) {
        return false;
    }

    const QScrollBar* scrollBar = chatView->verticalScrollBar();
    if (!scrollBar) {
        return false;
    }

    const int bottomDistance = scrollBar->maximum() - scrollBar->value();
    const int threshold = qMax(kNewMessageNotifierMinBottomDistance,
                               chatView->viewport()->height() /
                                       kNewMessageNotifierViewportDistanceDivisor);
    return bottomDistance > threshold;
}

void ChatArea::updateHistoryUnreadNotifier()
{
    if (!historyUnreadNotifier) {
        return;
    }

    if (m_state.historyUnreadMessageCount > 0) {
        if (historyUnreadNotifier->isVisible()) {
            showHistoryUnreadNotifier();
        } else if (!historyUnreadNotifierLoadTimer ||
                   !historyUnreadNotifierLoadTimer->isActive()) {
            if (historyUnreadNotifierLoadTimer) {
                historyUnreadNotifierLoadTimer->start();
            } else {
                showHistoryUnreadNotifier();
            }
        }
        return;
    }

    hideHistoryUnreadNotifier();
}

void ChatArea::showHistoryUnreadNotifier()
{
    if (historyUnreadNotifierLoadTimer) {
        historyUnreadNotifierLoadTimer->stop();
    }
    if (!historyUnreadNotifier) {
        return;
    }
    if (conversationId().isEmpty() || m_state.historyUnreadMessageCount <= 0) {
        historyUnreadNotifier->hide();
        return;
    }

    historyUnreadNotifier->setUnreadCount(m_state.historyUnreadMessageCount);
    historyUnreadNotifier->show();
    historyUnreadNotifier->raise();
    updateHistoryUnreadNotifierPosition();
}

void ChatArea::hideHistoryUnreadNotifier()
{
    if (historyUnreadNotifierLoadTimer) {
        historyUnreadNotifierLoadTimer->stop();
    }
    if (historyUnreadNotifier) {
        historyUnreadNotifier->hide();
    }
}

void ChatArea::updateHistoryUnreadNotifierPosition()
{
    if (!historyUnreadNotifier || !historyUnreadNotifier->isVisible() || !chatView) {
        return;
    }

    const QRect chatViewRect = chatView->geometry();
    const int rightEdge = chatViewRect.right() + 1 - visibleInfoPanelWidth();
    const int x = qMax(chatViewRect.left() + 12, rightEdge - historyUnreadNotifier->width());
    const int y = chatViewRect.top() + 12;
    historyUnreadNotifier->move(x, y);
    historyUnreadNotifier->raise();
}

void ChatArea::updateNewMessageNotifierPosition()
{
    if (newMessageNotifier->isVisible() && inputBar) {
        const QRect inputBarRect = inputBar->geometry();
        const int x = inputBarRect.x() + inputBarRect.width() - newMessageNotifier->width();
        const int y = inputBarRect.y() - newMessageNotifier->height() - kNewMessageNotifierInputGap;
        newMessageNotifier->move(x, y);
        newMessageNotifier->raise();
    }
}

void ChatArea::scrollToBottom(bool accelerateFarDistance)
{
    chatView->scrollToBottom(accelerateFarDistance);
    updateNewMessageNotifier();
    scheduleVisibleUnreadCheck();
}

void ChatArea::scrollToFirstHistoryUnread()
{
    if (m_state.historyUnreadMessageCount <= 0) {
        updateHistoryUnreadNotifier();
        return;
    }

    const int clickedUnreadCount = m_state.historyUnreadMessageCount;
    if (m_state.loadingOlderMessages) {
        m_state.pendingHistoryUnreadScroll = true;
        return;
    }

    while (m_state.historyUnloadedUnreadMessageCount > 0) {
        if (!loadHistoryUnreadMessages(clickedUnreadCount)) {
            break;
        }
    }

    const int unreadJumpRow = historyUnreadJumpRow(clickedUnreadCount);
    const ChatMessage* unreadJumpMessage = chatModel && unreadJumpRow >= 0
            ? chatModel->messageAt(unreadJumpRow)
            : nullptr;
    if (!unreadJumpMessage) {
        updateHistoryUnreadNotifier();
        return;
    }

    chatModel->setNewMessageDividerBefore(unreadJumpMessage);
    const QModelIndex dividerIndex = chatModel->newMessageDividerIndex();
    const QModelIndex messageIndex = chatModel->indexForMessage(unreadJumpMessage);
    const QPersistentModelIndex targetIndex(dividerIndex.isValid() ? dividerIndex : messageIndex);

    QTimer::singleShot(kHistoryUnreadScrollDelayMs, this, [this, targetIndex]() {
        if (!targetIndex.isValid()) {
            return;
        }

        chatView->scrollToIndexAtTopAnimated(targetIndex, true);
        scheduleVisibleUnreadCheck();
    });
}

void ChatArea::adjustBottomSpace()
{
    if (!chatModel || !chatView || !inputBar) {
        return;
    }
    if (conversationId().isEmpty() || inputBar->isHidden()) {
        chatModel->setBottomSpaceHeight(0);
        return;
    }

    const QRect chatViewRect = chatView->geometry();
    const QRect inputBarRect = inputBar->geometry();
    const int inputBarOverlapHeight = qMax(0, chatViewRect.bottom() - inputBarRect.top() + 1);
    const int safeBottomSpace = inputBarOverlapHeight + kChatListBottomSpacePadding;
    const bool shouldPreserveBottom = chatView->isBottomLocked() && isScrollAtBottom();

    const bool bottomSpaceChanged = chatModel->setBottomSpaceHeight(safeBottomSpace);
    if (bottomSpaceChanged && shouldPreserveBottom && !m_state.loadingOlderMessages) {
        chatView->scrollToBottomIfLocked();
    }
}

void ChatArea::addTextMessage(QSharedPointer<TextMessage> message,
                              const QDateTime &timestamp)
{
    if (!message) {
        return;
    }
    message->setTimestamp(timestamp);
    addMessage(std::move(message));
}

void ChatArea::applyConversationMeta()
{
    if (infoPanelOpen) {
        requestInfoPanelData(false);
    }
    if (m_state.meta.isGroup) {
        statusIcon->hide();
        nameLabel->setText(QString("%1（%2）").arg(m_state.meta.title, QString::number(m_state.meta.memberCount)));
        return;
    }

    statusIcon->show();
    statusIcon->setPixmap(ImageService::instance().scaled(statusIconPath(m_state.meta.status),
                                                          QSize(12, 12)));
    nameLabel->setText(m_state.meta.title);
}

void ChatArea::onSessionChanged(const ConversationMeta& meta)
{
    m_state.meta = meta;
    applyConversationMeta();
}

QWidget* ChatArea::activeInfoPanel() const
{
    return isGroupMode()
            ? static_cast<QWidget*>(groupInfoPanel)
            : static_cast<QWidget*>(directInfoPanel);
}

QWidget* ChatArea::inactiveInfoPanel() const
{
    return isGroupMode()
            ? static_cast<QWidget*>(directInfoPanel)
            : static_cast<QWidget*>(groupInfoPanel);
}

QWidget* ChatArea::ensureActiveInfoPanel()
{
    if (isGroupMode()) {
        if (!groupInfoPanel) {
            groupInfoPanel = new GroupConversationInfoPanel(this);
            groupInfoPanel->setObjectName("groupConversationInfoPanel");
            groupInfoPanel->setProperty("conversationType", "group");
            connectGroupInfoPanel(groupInfoPanel);
        }
        return groupInfoPanel;
    }

    if (!directInfoPanel) {
        directInfoPanel = new DirectConversationInfoPanel(this);
        directInfoPanel->setObjectName("directConversationInfoPanel");
        directInfoPanel->setProperty("conversationType", "direct");
        connectDirectInfoPanel(directInfoPanel);
    }
    return directInfoPanel;
}

void ChatArea::connectGroupInfoPanel(GroupConversationInfoPanel* panel)
{
    if (!panel || !sessionController) {
        return;
    }

    connect(panel, &GroupConversationInfoPanel::groupNameChanged,
            sessionController, &ChatSessionController::saveGroupName);
    connect(panel, &GroupConversationInfoPanel::groupIntroductionChanged,
            sessionController, &ChatSessionController::saveGroupIntroduction);
    connect(panel, &GroupConversationInfoPanel::groupAnnouncementChanged,
            sessionController, &ChatSessionController::saveGroupAnnouncement);
    connect(panel, &GroupConversationInfoPanel::currentUserNicknameChanged,
            sessionController, &ChatSessionController::saveCurrentUserGroupNickname);
    connect(panel, &GroupConversationInfoPanel::groupMemberNicknameChanged,
            sessionController, &ChatSessionController::saveGroupMemberNickname);
    connect(panel, &GroupConversationInfoPanel::groupMemberAdminPromotionRequested,
            sessionController, &ChatSessionController::promoteGroupMemberToAdmin);
    connect(panel, &GroupConversationInfoPanel::groupMemberAdminCancellationRequested,
            sessionController, &ChatSessionController::cancelGroupMemberAdmin);
    connect(panel, &GroupConversationInfoPanel::groupMemberInvitationRequested,
            sessionController, &ChatSessionController::inviteGroupMembers);
    connect(panel, &GroupConversationInfoPanel::groupMemberRemovalRequested,
            sessionController, &ChatSessionController::removeGroupMember);
    connect(panel, &GroupConversationInfoPanel::groupMembersBatchRemovalRequested,
            sessionController, &ChatSessionController::removeGroupMembers);
    connect(panel, &GroupConversationInfoPanel::groupRemarkChanged,
            sessionController, &ChatSessionController::saveGroupRemark);
    connect(panel, &GroupConversationInfoPanel::pinChanged,
            sessionController, &ChatSessionController::setPinned);
    connect(panel, &GroupConversationInfoPanel::doNotDisturbChanged,
            sessionController, &ChatSessionController::setDoNotDisturb);
    connect(panel, &GroupConversationInfoPanel::clearChatHistoryRequested,
            this, &ChatArea::confirmClearChatHistory);
    connect(panel, &GroupConversationInfoPanel::exitGroupRequested,
            this, &ChatArea::confirmExitGroup);
    connect(panel, &GroupConversationInfoPanel::groupMembersPageRequested,
            sessionController, &ChatSessionController::loadGroupMembersPage);
    connect(panel, &GroupConversationInfoPanel::memberProfileRequested,
            this, &ChatArea::showFriendProfilePopup);
    connect(panel, &GroupConversationInfoPanel::memberMessageRequested,
            this, &ChatArea::requestOpenConversation);
    connect(panel, &GroupConversationInfoPanel::memberAddFriendRequested,
            this, [this](const QString& userId) {
                AddContactSearchWindow::openUserRequest(userId, this);
            });
}

void ChatArea::connectDirectInfoPanel(DirectConversationInfoPanel* panel)
{
    if (!panel || !sessionController) {
        return;
    }

    connect(panel, &DirectConversationInfoPanel::remarkChanged,
            sessionController, &ChatSessionController::saveDirectRemark);
    connect(panel, &DirectConversationInfoPanel::pinChanged,
            sessionController, &ChatSessionController::setPinned);
    connect(panel, &DirectConversationInfoPanel::doNotDisturbChanged,
            sessionController, &ChatSessionController::setDoNotDisturb);
    connect(panel, &DirectConversationInfoPanel::clearChatHistoryRequested,
            this, &ChatArea::confirmClearChatHistory);
    connect(panel, &DirectConversationInfoPanel::deleteFriendRequested,
            this, &ChatArea::confirmDeleteFriend);
}

void ChatArea::releaseInfoPanels()
{
    if (sessionController) {
        sessionController->cancelPanelLoads();
    }
    if (infoPanelAnimation) {
        infoPanelAnimation->setTargetObject(nullptr);
    }
    if (groupInfoPanel) {
        groupInfoPanel->releaseTransientResources();
        groupInfoPanel->deleteLater();
        groupInfoPanel = nullptr;
    }
    if (directInfoPanel) {
        directInfoPanel->deleteLater();
        directInfoPanel = nullptr;
    }
}

void ChatArea::showFriendProfilePopup(const QString& userId, const QPoint& globalPos)
{
    if (userId.isEmpty() || !friendProfilePopup) {
        return;
    }

    friendProfilePopup->popupAt(globalPos, userId);
}

void ChatArea::showAvatarContextMenu(const QString& userId, const QPoint& globalPos)
{
    if (userId.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(ThemeManager::instance().color(ThemeColor::ContextMenuHover));

    const bool isCurrentUser = CurrentUser::instance().isCurrentUserId(userId);
    const bool isFriend = !isCurrentUser && UserRepository::instance().isFriend(userId);
    if (!isCurrentUser) {
        QAction* primaryAction = menu->addAction(isFriend
                                                 ? QStringLiteral("发消息")
                                                 : QStringLiteral("添加好友"));
        if (isFriend) {
            connect(primaryAction, &QAction::triggered, this, [this, userId]() {
                emit requestOpenConversation(userId);
            });
        } else {
            connect(primaryAction, &QAction::triggered, this, [this, userId]() {
                AddContactSearchWindow::openUserRequest(userId, this);
            });
        }
    }

    QAction* profileAction = menu->addAction(QStringLiteral("查看资料"));
    connect(profileAction, &QAction::triggered, this, [this, userId, globalPos]() {
        showFriendProfilePopup(userId, globalPos);
    });

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });
    menu->popupWhenMouseReleased(globalPos);
}

void ChatArea::requestInfoPanelData(bool resetTransientState)
{
    if (!sessionController || conversationId().isEmpty()) {
        return;
    }

    if (resetTransientState && isGroupMode()) {
        if (groupInfoPanel) {
            groupInfoPanel->resetTransientState();
            groupInfoPanel->setGroupSummary(Group{},
                                            m_state.meta,
                                            {},
                                            m_state.meta.memberCount,
                                            false,
                                            false);
        }
    } else if (resetTransientState && directInfoPanel) {
        directInfoPanel->setConversationMeta(m_state.meta, QString(), false);
    }
    sessionController->loadPanelData();
}

QRect ChatArea::infoPanelOpenGeometry() const
{
    const int maxPanelWidth = qMax(0, width() - 48);
    const int boundedWidth = qBound(kInfoPanelMinWidth,
                                    qMin(kInfoPanelPreferredWidth, width() * 2 / 5),
                                    kInfoPanelMaxWidth);
    const int panelWidth = qMin(boundedWidth, maxPanelWidth);
    const int panelTop = kChatInfoHeight + kInfoPanelDividerHeight;
    return QRect(width() - panelWidth,
                 panelTop,
                 panelWidth,
                 qMax(0, height() - panelTop));
}

QRect ChatArea::infoPanelClosedGeometry() const
{
    QRect openRect = infoPanelOpenGeometry();
    openRect.moveLeft(width());
    return openRect;
}

int ChatArea::visibleInfoPanelWidth() const
{
    QWidget* panel = activeInfoPanel();
    if (!panel || !panel->isVisible()) {
        return 0;
    }

    return qBound(0, width() - panel->geometry().left(), infoPanelOpenGeometry().width());
}

void ChatArea::showInfoPanel(bool animated)
{
    QWidget* panel = ensureActiveInfoPanel();
    QWidget* hiddenPanel = inactiveInfoPanel();
    if (!panel) {
        return;
    }

    requestInfoPanelData(true);

    if (infoPanelAnimation) {
        infoPanelAnimation->stop();
    }
    if (hiddenPanel) {
        hiddenPanel->hide();
    }

    infoPanelOpen = true;
    const QRect openRect = infoPanelOpenGeometry();
    panel->setGeometry(animated ? infoPanelClosedGeometry() : openRect);
    panel->show();
    panel->raise();
    if (infoButton) {
        infoButton->raise();
    }

    if (!animated || !infoPanelAnimation) {
        panel->setGeometry(openRect);
        return;
    }

    AudioService::instance().play(AudioService::SoundEffect::ChestOpen);
    infoPanelAnimation->setTargetObject(panel);
    infoPanelAnimation->setStartValue(panel->geometry());
    infoPanelAnimation->setEndValue(openRect);
    infoPanelAnimation->start();
}

void ChatArea::hideInfoPanel(bool animated)
{
    QWidget* panel = activeInfoPanel();
    if (!panel || (!panel->isVisible() && !infoPanelOpen)) {
        infoPanelOpen = false;
        releaseInfoPanels();
        return;
    }

    if (infoPanelAnimation) {
        infoPanelAnimation->stop();
    }

    infoPanelOpen = false;
    const QRect closedRect = infoPanelClosedGeometry();

    if (!animated || !infoPanelAnimation) {
        releaseInfoPanels();
        return;
    }

    panel->show();
    panel->raise();
    AudioService::instance().play(AudioService::SoundEffect::ChestClosed);
    infoPanelAnimation->setTargetObject(panel);
    infoPanelAnimation->setStartValue(panel->geometry());
    infoPanelAnimation->setEndValue(closedRect);
    infoPanelAnimation->start();
}

void ChatArea::updateInfoPanelGeometry()
{
    if (infoPanelAnimation && infoPanelAnimation->state() == QPropertyAnimation::Running) {
        infoPanelAnimation->stop();
    }

    const QRect targetRect = infoPanelOpen ? infoPanelOpenGeometry() : infoPanelClosedGeometry();
    if (groupInfoPanel) {
        groupInfoPanel->setGeometry(targetRect);
        groupInfoPanel->setVisible(infoPanelOpen && isGroupMode());
    }
    if (directInfoPanel) {
        directInfoPanel->setGeometry(targetRect);
        directInfoPanel->setVisible(infoPanelOpen && !isGroupMode());
    }

    if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
        panel->raise();
    }
    if (infoButton) {
        infoButton->raise();
    }
}

bool ChatArea::containsGlobalPoint(QWidget* widget, const QPoint& globalPos) const
{
    return widget && widget->isVisible() && widget->rect().contains(widget->mapFromGlobal(globalPos));
}

void ChatArea::updateGroupInfoPanelState()
{
    if (!groupInfoPanel || !infoPanelOpen) {
        return;
    }

    requestInfoPanelData(false);
}

void ChatArea::updateDirectInfoPanelState(bool animated)
{
    if (directInfoPanel && infoPanelOpen) {
        const QString remark = sessionController ? sessionController->directUser().remark : QString{};
        directInfoPanel->setConversationMeta(m_state.meta, remark, animated);
    }
}

void ChatArea::onDirectPanelDataLoaded(const ConversationMeta& meta, const User& directUser)
{
    if (!directInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || meta.isGroup) {
        return;
    }

    m_state.meta = meta;
    directInfoPanel->setConversationMeta(meta, directUser.remark, true);
}

void ChatArea::onGroupPanelDataLoaded(const ConversationMeta& meta,
                                      const Group& group,
                                      const QVector<User>& previewMembers,
                                      int totalMembers,
                                      bool canEditGroupInfo,
                                      bool canExitGroup)
{
    if (!groupInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || !meta.isGroup) {
        return;
    }

    m_state.meta = meta;
    groupInfoPanel->setGroupSummary(group,
                                    meta,
                                    previewMembers,
                                    totalMembers,
                                    canEditGroupInfo,
                                    canExitGroup);
}

void ChatArea::onGroupMembersPageLoaded(const ConversationMeta& meta, const GroupMembersPage& page)
{
    if (!groupInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || !meta.isGroup) {
        return;
    }

    groupInfoPanel->appendGroupMembersPage(page);
}

void ChatArea::onInfoButtonClicked()
{
    if (infoPanelOpen) {
        hideInfoPanel(true);
    } else {
        showInfoPanel(true);
    }
}

void ChatArea::confirmClearChatHistory()
{
    if (conversationId().isEmpty()) {
        return;
    }

    const InWindowPopup::Button result = InWindowPopup::question(this,
                                                                 QStringLiteral("删除聊天记录"),
                                                                 QStringLiteral("确认删除当前聊天记录吗？"));
    if (result != InWindowPopup::Button::Yes) {
        return;
    }

    if (sessionController) {
        sessionController->clearMessages();
    }
}

void ChatArea::onSessionMessagesCleared()
{
    chatModel->clear();
    m_state.loadedMessageCount = 0;
    m_state.historyUnreadMessageCount = 0;
    m_state.historyUnloadedUnreadMessageCount = 0;
    m_state.newUnreadMessageCount = 0;
    m_state.newMessageNotifierRevealedByDownScroll = false;
    m_state.pendingHistoryUnreadScroll = false;
    m_state.hasHistoryUnreadOrdinalRange = false;
    m_state.hasNewUnreadOrdinalRange = false;
    m_state.peerMessageOrdinals.clear();
    m_state.minPeerMessageOrdinal = 0;
    m_state.maxPeerMessageOrdinal = -1;
    m_state.hasMoreBefore = false;
    m_state.loadingOlderMessages = false;
    m_state.loadingInitialMessages = false;
    if (messageLoadingAnimationTimer) {
        messageLoadingAnimationTimer->stop();
    }
    hideHistoryUnreadNotifier();
    newMessageNotifier->hide();
    adjustBottomSpace();
}

void ChatArea::confirmExitGroup()
{
    if (conversationId().isEmpty() || !isGroupMode()) {
        return;
    }

    if (!sessionController || !sessionController->canExitGroup()) {
        return;
    }

    const InWindowPopup::Button result = InWindowPopup::question(this,
                                                                 QStringLiteral("退出群聊"),
                                                                 QStringLiteral("确认退出该群聊吗？"));
    if (result != InWindowPopup::Button::Yes) {
        return;
    }

    sessionController->exitGroup();
}

void ChatArea::confirmDeleteFriend()
{
    if (conversationId().isEmpty() || isGroupMode()) {
        return;
    }

    const InWindowPopup::Button result = InWindowPopup::question(this,
                                                                 QStringLiteral("删除好友"),
                                                                 QStringLiteral("确认删除该好友吗？"));
    if (result != InWindowPopup::Button::Yes) {
        return;
    }

    if (sessionController) {
        sessionController->deleteFriend();
    }
}

void ChatArea::onSessionConversationRemoved()
{
    clearConversation();
    emit currentConversationRemoved();
}

void ChatArea::updateInputBarPosition() {
    if (inputBar) {
        if (conversationId().isEmpty()) {
            inputBar->hide();
            if (historyUnreadNotifier) {
                hideHistoryUnreadNotifier();
            }
            if (bottomGapGradientOverlay) {
                bottomGapGradientOverlay->hide();
            }
            adjustBottomSpace();
            return;
        }

        if (m_systemFloatingBarsSuppressed) {
            inputBar->hide();
            if (bottomGapGradientOverlay) {
                bottomGapGradientOverlay->hide();
            }
            adjustBottomSpace();
            return;
        }

        const int infoPanelWidth = visibleInfoPanelWidth();
        const QRect inputBarRect(
                kInputBarSideMargin,
                height() - kInputBarHeight - kInputBarBottomMargin,
                qMax(0, width() - 2 * kInputBarSideMargin - infoPanelWidth),
                kInputBarHeight);
        inputBar->setGeometry(inputBarRect);
        if (inputBar->isHidden()) {
            inputBar->show();
        }
        inputBar->raise();
        inputBar->scheduleLiquidGlassUpdate(0);

#ifdef Q_OS_MACOS
        const bool useBottomGradient =
                MacFloatingInputBarBridge::appearance() == MacFloatingInputBarBridge::Appearance::LiquidGlass;
#else
        const bool useBottomGradient = false;
#endif
        if (bottomGapGradientOverlay) {
            const int gradientTop = inputBarRect.bottom() + 1;
            const int gradientHeight = height() - gradientTop;
            if (useBottomGradient && gradientHeight > 0) {
                const int overlayTop = qMax(0, gradientTop - kInputBarBottomGradientFadeHeight);
                bottomGapGradientOverlay->setGeometry(
                        0,
                        overlayTop,
                        width(),
                        height() - overlayTop);
                bottomGapGradientOverlay->show();
                bottomGapGradientOverlay->raise();
                inputBar->raise();
            } else {
                bottomGapGradientOverlay->hide();
            }
        }
        if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
            panel->raise();
        }
        updateHistoryUnreadNotifierPosition();
        updateNewMessageNotifierPosition();
    }
}

void ChatArea::onSendImage(const QString &path)
{
    if (ImageService::instance().sourceSize(path).isValid()) {
        GroupRole role = GroupRole::Member;
        if (isGroupMode()) {
            const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
            role = groupRoleForUser(group, CurrentUser::instance().getUserId());
        }
        auto ptr =
                QSharedPointer<ImageMessage>::create(path,
                                               true,
                                               CurrentUser::instance().getUserId(),
                                               isGroupMode(),
                                               CurrentUser::instance().getUserName(),
                                               role);
        addMessage(ptr);
    }
}

void ChatArea::onSendText(const QString &text)
{
    if (!text.trimmed().isEmpty()) {
        GroupRole role = GroupRole::Member;
        if (isGroupMode()) {
            const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
            role = groupRoleForUser(group, CurrentUser::instance().getUserId());
        }
        auto ptr =
                QSharedPointer<TextMessage>::create(text,
                                               true,
                                               CurrentUser::instance().getUserId(),
                                               isGroupMode(),
                                               CurrentUser::instance().getUserName(),
                                               role);
        addMessage(ptr);
    }
}

void ChatArea::onSendTextAsPeer(const QString& text)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return;
    }

    QString senderId = m_state.meta.conversationId;
    QString senderName = m_state.meta.title;
    GroupRole role = GroupRole::Member;

    if (isGroupMode()) {
        const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
        senderId = simulatedPeerIdForGroup(group);
        if (senderId.isEmpty()) {
            return;
        }
        senderName = groupMemberDisplayName(group, senderId);
        role = groupRoleForUser(group, senderId);
    }

    auto ptr = QSharedPointer<TextMessage>::create(trimmedText,
                                                   false,
                                                   senderId,
                                                   isGroupMode(),
                                                   senderName,
                                                   role);
    addMessage(ptr);
}

bool ChatArea::canRecallMessage(const ChatMessage* message) const
{
    if (!message || message->getType() == MessageType::Recall || conversationId().isEmpty()) {
        return false;
    }

    const qint64 ageSeconds = message->getTimestamp().isValid()
            ? message->getTimestamp().secsTo(QDateTime::currentDateTime())
            : kMessageRecallReeditSeconds + 1;
    const bool withinOwnRecallWindow = ageSeconds >= 0 &&
            ageSeconds <= kMessageRecallReeditSeconds;
    if (!isGroupMode()) {
        return message->isFromMe() && withinOwnRecallWindow;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
    const GroupRole currentRole = groupRoleForUser(group, CurrentUser::instance().getUserId());
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    if (currentRole == GroupRole::Admin) {
        return message->isFromMe() || message->getRole() == GroupRole::Member;
    }
    return message->isFromMe() && withinOwnRecallWindow;
}

QSharedPointer<RecallMessage> ChatArea::createRecallMessage(const QSharedPointer<ChatMessage>& message,
                                                            const QString& actorId,
                                                            const QString& actorName,
                                                            GroupRole actorRole,
                                                            bool moderatorRecall) const
{
    if (message.isNull()) {
        return {};
    }

    const bool actorIsCurrentUser = CurrentUser::instance().isCurrentUserId(actorId);
    const bool allowReedit = actorIsCurrentUser &&
            message->isFromMe() &&
            message->getType() == MessageType::Text;
    const QString originalText = allowReedit
            ? static_cast<const TextMessage*>(message.data())->getText()
            : QString();
    const QString senderName = message->getSenderName().isEmpty()
            ? message->getSenderId()
            : message->getSenderName();
    const QString safeActorName = actorName.isEmpty() ? actorId : actorName;
    const QString displayText = isGroupMode()
            ? groupRecallText(safeActorName,
                              actorRole,
                              senderName,
                              message->getRole(),
                              moderatorRecall)
            : directRecallText(message->isFromMe(), actorIsCurrentUser);

    auto recallMessage = QSharedPointer<RecallMessage>::create(displayText,
                                                               originalText,
                                                               allowReedit,
                                                               message->isFromMe(),
                                                               message->getSenderId(),
                                                               message->isInGroupChat(),
                                                               senderName,
                                                               message->getRole(),
                                                               actorId,
                                                               safeActorName,
                                                               actorRole,
                                                               moderatorRecall);
    recallMessage->setTimestamp(message->getTimestamp());
    return recallMessage;
}

void ChatArea::removeUnreadCandidate(const ChatMessage* message)
{
    if (!message) {
        return;
    }

    const auto ordinalIt = m_state.peerMessageOrdinals.constFind(message);
    if (ordinalIt == m_state.peerMessageOrdinals.cend()) {
        return;
    }

    const int ordinal = ordinalIt.value();
    if (m_state.hasHistoryUnreadOrdinalRange &&
            ordinal >= m_state.historyUnreadFirstOrdinal &&
            ordinal <= m_state.historyUnreadLastOrdinal &&
            ordinal < m_state.historyReadMinOrdinal) {
        m_state.historyReadMinOrdinal = qMax(m_state.historyUnreadFirstOrdinal, ordinal);
        recalculateHistoryUnreadCount();
    }
    if (m_state.hasNewUnreadOrdinalRange &&
            ordinal >= m_state.newUnreadFirstOrdinal &&
            ordinal <= m_state.newUnreadLastOrdinal &&
            ordinal > m_state.newReadMaxOrdinal) {
        m_state.newReadMaxOrdinal = ordinal;
        recalculateNewUnreadCount();
    }
}

void ChatArea::scheduleReeditExpiry(const QSharedPointer<RecallMessage>& message)
{
    if (message.isNull() || !message->canReedit()) {
        return;
    }

    QTimer::singleShot(kMessageRecallReeditSeconds * 1000, this, [this, message]() {
        if (message.isNull() || !message->canReedit()) {
            return;
        }

        message->clearReeditText();
        if (chatModel) {
            chatModel->notifyMessageChanged(message.get());
        }
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
}

void ChatArea::recallMessageAtRow(int row,
                                  const QString& actorId,
                                  const QString& actorName,
                                  GroupRole actorRole,
                                  bool force)
{
    if (conversationId().isEmpty() || !chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull() || message->getType() == MessageType::Recall) {
        return;
    }
    if (!force && !canRecallMessage(message.get())) {
        return;
    }

    QString effectiveActorId = actorId;
    QString effectiveActorName = actorName;
    GroupRole effectiveActorRole = actorRole;
    if (effectiveActorId.isEmpty()) {
        effectiveActorId = CurrentUser::instance().getUserId();
        effectiveActorName = CurrentUser::instance().getUserName();
        if (isGroupMode()) {
            const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
            effectiveActorName = groupMemberDisplayName(group, effectiveActorId);
            effectiveActorRole = groupRoleForUser(group, effectiveActorId);
        }
    }

    const bool moderatorRecall = isGroupMode() &&
            !effectiveActorId.isEmpty() &&
            effectiveActorId != message->getSenderId();
    const QSharedPointer<RecallMessage> recallMessage =
            createRecallMessage(message,
                                effectiveActorId,
                                effectiveActorName,
                                effectiveActorRole,
                                moderatorRecall);
    if (recallMessage.isNull()) {
        return;
    }

    if (!MessageRepository::instance().replaceMessage(conversationId(), message, recallMessage)) {
        return;
    }

    const auto ordinalIt = m_state.peerMessageOrdinals.constFind(message.get());
    if (ordinalIt != m_state.peerMessageOrdinals.cend() && !recallMessage->isFromMe()) {
        m_state.peerMessageOrdinals.insert(recallMessage.get(), ordinalIt.value());
    }
    removeUnreadCandidate(message.get());
    chatModel->replaceMessage(row, recallMessage);
    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
    scheduleReeditExpiry(recallMessage);
    adjustBottomSpace();
}

void ChatArea::onRecallMessageRequested(int row)
{
    recallMessageAtRow(row);
}

void ChatArea::onReeditMessageRequested(int row)
{
    if (!inputBar || !chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull() || message->getType() != MessageType::Recall) {
        return;
    }

    const RecallMessage* recallMessage = static_cast<const RecallMessage*>(message.data());
    if (!recallMessage->canReedit()) {
        return;
    }

    inputBar->appendText(recallMessage->getOriginalText());
    inputBar->focusInput();
}

void ChatArea::onRecallLatestPeerMessageRequested()
{
    if (conversationId().isEmpty() || !chatModel) {
        return;
    }

    for (int row = chatModel->rowCount() - 1; row >= 0; --row) {
        const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
        if (message.isNull() ||
                message->isFromMe() ||
                message->getType() == MessageType::Recall) {
            continue;
        }

        recallMessageAtRow(row,
                           message->getSenderId(),
                           message->getSenderName(),
                           message->getRole(),
                           true);
        return;
    }
}

void ChatArea::onDeleteMessageRequested(int row)
{
    if (conversationId().isEmpty() || !chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull()) {
        return;
    }

    if (!MessageRepository::instance().removeMessage(conversationId(), message)) {
        return;
    }

    removeUnreadCandidate(message.get());
    if (chatModel->removeMessage(row) && m_state.loadedMessageCount > 0) {
        --m_state.loadedMessageCount;
    }
    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
    adjustBottomSpace();
}

void ChatArea::clearConversation(bool closeInfoPanel)
{
    if (closeInfoPanel) {
        hideInfoPanel(false);
    }
    if (sessionController) {
        sessionController->close();
    }
    if (friendProfilePopup) {
        friendProfilePopup->hide();
        friendProfilePopup->clear();
    }
    chatModel->clear();
    chatModel->clearSelection();
    chatView->clearTextSelection();
    if (messageLoadingAnimationTimer) {
        messageLoadingAnimationTimer->stop();
    }
    m_state = {};
    nameLabel->clear();
    statusIcon->hide();
    if (inputBar) {
        inputBar->hide();
    }
    if (bottomGapGradientOverlay) {
        bottomGapGradientOverlay->hide();
    }
    hideHistoryUnreadNotifier();
    newMessageNotifier->hide();
    updateGroupInfoPanelState();
    updateDirectInfoPanelState(false);
    adjustBottomSpace();
}

void ChatArea::closeConversation()
{
    clearConversation();
}

void ChatArea::loadOlderMessages()
{
    if (conversationId().isEmpty() ||
            !m_state.hasMoreBefore ||
            m_state.loadingOlderMessages ||
            !chatModel ||
            !chatView) {
        return;
    }

    m_state.loadingOlderMessages = true;
#ifdef Q_OS_WIN
    m_state.olderMessageTriggerCooldownUntilMs =
            QDateTime::currentMSecsSinceEpoch() + kOlderMessageTriggerCooldownMs;
    chatView->stopAnimatedWheelScroll();
#endif
    const QString loadingConversationId = conversationId();
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    const int previousValue = scrollBar->value();
    const int previousMaximum = scrollBar->maximum();

    const ConversationThreadData olderPage = MessageRepository::instance().requestConversationThread({
            loadingConversationId,
            m_state.loadedMessageCount,
            kOlderMessagePageSize
    });

    if (!chatModel || !chatView || conversationId() != loadingConversationId) {
        m_state.loadingOlderMessages = false;
        return;
    }

    if (olderPage.messages.isEmpty()) {
        m_state.hasMoreBefore = false;
        reconcileHistoryUnreadAfterHistoryExhausted();
        updateHistoryUnreadNotifier();
        m_state.loadingOlderMessages = false;
        if (m_state.pendingHistoryUnreadScroll) {
            m_state.pendingHistoryUnreadScroll = false;
            scrollToFirstHistoryUnread();
        }
        return;
    }

    assignPrependedPeerMessageOrdinals(olderPage.messages);
#ifdef Q_OS_WIN
    chatView->setOverlayScrollBarUpdatesPaused(true);
#endif
    chatModel->prependMessages(olderPage.messages);
    const int loadedUnread = registerHistoryUnreadCandidates(
            olderPage.messages,
            m_state.historyUnloadedUnreadMessageCount);
    m_state.historyUnloadedUnreadMessageCount = qMax(
            0,
            m_state.historyUnloadedUnreadMessageCount - loadedUnread);
    recalculateHistoryUnreadCount();
    m_state.loadedMessageCount = olderPage.loadedMessageCount;
    m_state.hasMoreBefore = olderPage.hasMoreBefore;
    reconcileHistoryUnreadAfterHistoryExhausted();
    updateHistoryUnreadNotifier();
    adjustBottomSpace();
    chatView->preserveScrollPositionAfterPrepend(previousValue, previousMaximum);
    QTimer::singleShot(0, this, [this]() {
#ifdef Q_OS_WIN
        if (chatView) {
            chatView->stopAnimatedWheelScroll();
            chatView->setOverlayScrollBarUpdatesPaused(false);
        }
        m_state.olderMessageTriggerCooldownUntilMs =
                QDateTime::currentMSecsSinceEpoch() + kOlderMessageTriggerCooldownMs;
#endif
        m_state.loadingOlderMessages = false;
        if (m_state.pendingHistoryUnreadScroll) {
            m_state.pendingHistoryUnreadScroll = false;
            scrollToFirstHistoryUnread();
            return;
        }
        scheduleVisibleUnreadCheck();
    });
}

bool ChatArea::loadHistoryUnreadMessages(int requestedMessageCount)
{
    if (conversationId().isEmpty() ||
            m_state.historyUnreadMessageCount <= 0 ||
            m_state.historyUnloadedUnreadMessageCount <= 0 ||
            m_state.loadingOlderMessages) {
        return false;
    }

    m_state.loadingOlderMessages = true;
    const int previousLoadedMessageCount = m_state.loadedMessageCount;
    const int requestLimit = qMax(kOlderMessagePageSize,
                                  qMax(requestedMessageCount,
                                       m_state.historyUnloadedUnreadMessageCount));
    QScrollBar* scrollBar = chatView ? chatView->verticalScrollBar() : nullptr;
    const int previousValue = scrollBar ? scrollBar->value() : 0;
    const int previousMaximum = scrollBar ? scrollBar->maximum() : 0;
    const ConversationThreadData olderPage = MessageRepository::instance().requestConversationThread({
            conversationId(),
            m_state.loadedMessageCount,
            requestLimit
    });

    if (olderPage.messages.isEmpty()) {
        m_state.hasMoreBefore = false;
        reconcileHistoryUnreadAfterHistoryExhausted();
        updateHistoryUnreadNotifier();
        adjustBottomSpace();
        m_state.loadingOlderMessages = false;
        return false;
    }

    assignPrependedPeerMessageOrdinals(olderPage.messages);
    chatModel->prependMessages(olderPage.messages);
    const int loadedUnread = registerHistoryUnreadCandidates(
            olderPage.messages,
            m_state.historyUnloadedUnreadMessageCount);
    m_state.historyUnloadedUnreadMessageCount = qMax(
            0,
            m_state.historyUnloadedUnreadMessageCount - loadedUnread);
    recalculateHistoryUnreadCount();
    m_state.loadedMessageCount = olderPage.loadedMessageCount;
    m_state.hasMoreBefore = olderPage.hasMoreBefore;
    reconcileHistoryUnreadAfterHistoryExhausted();
    updateHistoryUnreadNotifier();
    adjustBottomSpace();
    if (chatView) {
        chatView->preserveScrollPositionAfterPrepend(previousValue, previousMaximum);
    }
    m_state.loadingOlderMessages = false;
    return m_state.loadedMessageCount > previousLoadedMessageCount;
}

QString ChatArea::conversationId() const
{
    return m_state.meta.conversationId;
}

bool ChatArea::isGroupMode() const
{
    return m_state.meta.isGroup;
}

void ChatArea::showConversationLoading(const ConversationMeta& meta)
{
    if (infoPanelOpen) {
        hideInfoPanel(true);
    }
    clearConversation(false);

    if (sessionController) {
        sessionController->open(meta);
    }
    m_state.meta = meta;
    m_state.loadingInitialMessages = true;
    m_state.allowOlderMessageFetch = false;
    applyConversationMeta();

    const int targetLoadingHeight = chatView && chatView->viewport()
            ? chatView->viewport()->height() / 2
            : height() / 2;
    if (chatModel) {
        chatModel->showInitialLoadingPlaceholders(targetLoadingHeight);
    }
    if (messageLoadingAnimationTimer) {
        messageLoadingAnimationTimer->start();
    }

    if (inputBar && !m_systemFloatingBarsSuppressed) {
        inputBar->show();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
    }
    hideHistoryUnreadNotifier();
    if (newMessageNotifier) {
        newMessageNotifier->hide();
    }
    updateInputBarPosition();
    adjustBottomSpace();
}

void ChatArea::openConversation(const ConversationThreadData& conversation)
{
    if (infoPanelOpen) {
        hideInfoPanel(true);
    }
    clearConversation(false);
    if (sessionController) {
        sessionController->open(conversation.meta);
    } else {
        m_state.meta = conversation.meta;
    }
    m_state.loadedMessageCount = conversation.loadedMessageCount;
    m_state.historyUnreadMessageCount = qMax(0, conversation.unreadCount);
    m_state.historyUnloadedUnreadMessageCount = m_state.historyUnreadMessageCount;
    m_state.newUnreadMessageCount = 0;
    m_state.newMessageNotifierRevealedByDownScroll = false;
    m_state.loadingInitialMessages = false;
    m_state.hasMoreBefore = conversation.hasMoreBefore;
    m_state.allowOlderMessageFetch = false;
    assignInitialPeerMessageOrdinals(conversation.messages);
    const int loadedUnread = registerHistoryUnreadCandidates(
            conversation.messages,
            m_state.historyUnloadedUnreadMessageCount);
    m_state.historyUnloadedUnreadMessageCount = qMax(
            0,
            m_state.historyUnloadedUnreadMessageCount - loadedUnread);
    recalculateHistoryUnreadCount();
    chatModel->setMessages(conversation.messages);
    reconcileHistoryUnreadAfterHistoryExhausted();
    if (inputBar && !m_systemFloatingBarsSuppressed) {
        inputBar->show();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
    }
    updateInputBarPosition();
    adjustBottomSpace();
    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
    QTimer::singleShot(0, this, [this]() {
        chatView->jumpToBottom();
        m_state.allowOlderMessageFetch = true;
        scheduleVisibleUnreadCheck();
    });
    QTimer::singleShot(0, inputBar, [this]() {
        if (inputBar && inputBar->isVisible()) {
            inputBar->focusInput();
        }
    });
}

void ChatArea::clearMessageSelection()
{
    if (chatModel) {
        chatModel->clearSelection();
    }
    if (chatView) {
        chatView->clearTextSelection();
    }
}

void ChatArea::handleGlobalMousePress(const QPoint& globalPos)
{
    if (!chatModel || !chatView) {
        return;
    }

    if (containsGlobalPoint(infoButton, globalPos)) {
        return;
    }

    if (infoPanelOpen) {
        QWidget* panel = activeInfoPanel();
        if (containsGlobalPoint(panel, globalPos)) {
            clearMessageSelection();
            return;
        }
        hideInfoPanel(true);
    }

    if (inputBar && inputBar->rect().contains(inputBar->mapFromGlobal(globalPos))) {
        clearMessageSelection();
        return;
    }

    QWidget* viewport = chatView->viewport();
    if (!viewport || !viewport->rect().contains(viewport->mapFromGlobal(globalPos))) {
        clearMessageSelection();
        return;
    }

    const QPoint viewportPos = viewport->mapFromGlobal(globalPos);
    const QModelIndex index = chatView->indexAt(viewportPos);
    if (!index.isValid()) {
        clearMessageSelection();
        return;
    }

    if (chatModel->isBottomSpace(index.row()) || chatModel->isTimeHeader(index.row())) {
        clearMessageSelection();
        return;
    }

    QStyleOptionViewItem option;
    option.initFrom(chatView->viewport());
    option.rect = chatView->visualRect(index);
    if (!chatDelegate || !chatDelegate->bubbleHitTest(option, index, viewportPos)) {
        clearMessageSelection();
    }
}

void ChatArea::setSystemFloatingBarsSuppressed(bool suppressed)
{
    if (!inputBar) {
        return;
    }

    inputBar->setProperty("systemFloatingBarsSuppressed", suppressed);

    if (m_systemFloatingBarsSuppressed == suppressed) {
        if (suppressed && !inputBar->isHidden()) {
            m_inputBarVisibleBeforeSystemSuppression = true;
            inputBar->hide();
            if (bottomGapGradientOverlay) {
                bottomGapGradientOverlay->hide();
            }
            adjustBottomSpace();
        }
        return;
    }

    m_systemFloatingBarsSuppressed = suppressed;
    if (suppressed) {
        m_inputBarVisibleBeforeSystemSuppression = !inputBar->isHidden();
        inputBar->hide();
        if (bottomGapGradientOverlay) {
            bottomGapGradientOverlay->hide();
        }
        adjustBottomSpace();
        return;
    }

    if (m_inputBarVisibleBeforeSystemSuppression && !conversationId().isEmpty()) {
        updateInputBarPosition();
        inputBar->refreshPlatformAppearance();
        inputBar->show();
        inputBar->raise();
        updateHistoryUnreadNotifierPosition();
        updateNewMessageNotifierPosition();
        adjustBottomSpace();
    } else if (!inputBar->isHidden()) {
        updateInputBarPosition();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
        updateHistoryUnreadNotifierPosition();
        adjustBottomSpace();
    }
    m_inputBarVisibleBeforeSystemSuppression = false;
}
