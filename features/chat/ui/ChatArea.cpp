#include "ChatArea.h"
#include "shared/services/AppFonts.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/ChatRemoteDataSource.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/chat/ui/ConversationInfoPanel.h"
#include "features/chat/ui/ChatSessionController.h"
#include "features/friend/ui/FriendProfilePopup.h"
#include "features/friend/ui/AddContactSearchWindow.h"
#include "features/friend/ui/FriendSessionController.h"
#include "app/state/CurrentUserProfileEditContent.h"
#include "app/state/CurrentUser.h"
#include "shared/network/NetworkTypes.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/BottomFadeOverlay.h"
#include "shared/ui/popup/InWindowPopupDialogs.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/QtFallbackLiquidGlass.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/StyledActionMenu.h"
#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif
#include <QAction>
#include <QPainter>
#include <QPalette>
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
#include <QSet>
#include <QVBoxLayout>
#include <QVariant>
#include <QUuid>
#include <algorithm>
#include <utility>

namespace {

// Floating input bar metrics. Keep list bottom spacing in sync with these values.
static constexpr int kChatInfoHeight = 62;
static constexpr int kInputBarSideMargin = 20;
static constexpr int kInputBarBottomMargin = 18;
static constexpr int kInputBarHeight = 195;
static constexpr int kChatListBottomSpacePadding = 10;
static constexpr int kFloatingNotifierInputGap = 4;
static constexpr int kFloatingNotifierHorizontalGap = 4;
static constexpr int kInputBarBottomGradientFadeHeight = 32;
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
static constexpr int kMessageSendAckTimeoutMs = 12000;
static constexpr auto kDirectRelationshipDeletedNoticeText = "你们已不是好友，无法发送消息";

static constexpr int kMessageLoadingSkeletonFrameMs = 40;
static constexpr int kNewMessageNotifierMinBottomDistance = 220;
static constexpr int kNewMessageNotifierViewportDistanceDivisor = 2;

bool isGroupSystemEventMessage(const ChatMessage* message)
{
    if (!message) {
        return false;
    }
    return message->getType() == MessageType::GroupMemberJoined ||
           message->getType() == MessageType::GroupSystemEvent;
}

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
    const GroupMemberProfile member = GroupRepository::instance().requestGroupMember(group.groupId, userId);
    if (member.role == GroupMemberRoleValue::Ai) {
        return GroupRole::Ai;
    }
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
    const QString cachedNickname = GroupRepository::instance().requestGroupMemberNickname(group.groupId, userId).trimmed();
    if (!cachedNickname.isEmpty()) {
        return cachedNickname;
    }
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

QString currentUserSenderId()
{
    const CurrentUserProfile profile = CurrentUser::instance().identity();
    return profile.userUuid.isEmpty() ? CurrentUser::instance().getUserId() : profile.userUuid;
}

QString mentionTargetUserUuid(const User& user, const QString& fallbackUserId)
{
    if (CurrentUser::instance().isCurrentUserId(fallbackUserId) ||
        CurrentUser::instance().isCurrentUserId(user.id)) {
        const CurrentUserProfile profile = CurrentUser::instance().identity();
        if (!profile.userUuid.isEmpty()) {
            return profile.userUuid;
        }
    }
    if (!user.userUuid.isEmpty()) {
        return user.userUuid;
    }
    if (!user.id.isEmpty()) {
        return user.id;
    }
    return fallbackUserId;
}

QString mentionTargetId(const User& user, const QString& targetUserUuid)
{
    if (!user.userId.trimmed().isEmpty()) {
        return user.userId.trimmed();
    }
    return targetUserUuid;
}

QString mentionDisplayName(const Group& group,
                           const User& user,
                           const QString& targetUserUuid)
{
    const QString groupName = groupMemberDisplayName(group, targetUserUuid).trimmed();
    if (!groupName.isEmpty()) {
        return groupName;
    }
    if (!user.nick.trimmed().isEmpty()) {
        return user.nick.trimmed();
    }
    if (!user.userId.trimmed().isEmpty()) {
        return user.userId.trimmed();
    }
    return targetUserUuid;
}

QString groupRoleLabel(GroupRole role)
{
    switch (role) {
    case GroupRole::Owner:
        return QStringLiteral("群主");
    case GroupRole::Admin:
        return QStringLiteral("管理员");
    case GroupRole::Ai:
        return QStringLiteral("AI");
    case GroupRole::Member:
    default:
        return QStringLiteral("群成员");
    }
}

bool canEditGroupMemberNickname(const Group& group, const QString& userId)
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (userId == currentUserId) {
        return true;
    }

    const GroupRole currentRole = groupRoleForUser(group, currentUserId);
    const GroupRole targetRole = groupRoleForUser(group, userId);
    if (targetRole == GroupRole::Ai) {
        return false;
    }
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
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
    chatDelegate->setReferenceResolver([this](const QString& messageId) -> const ChatMessage* {
        if (chatModel) {
            if (const ChatMessage* message = chatModel->messageById(messageId)) {
                return message;
            }
        }
        return MessageRepository::instance().requestMessageById(conversationId(), messageId).get();
    });
    
    // 设置模型和代理
    chatView->setModel(chatModel);
    chatView->setItemDelegate(chatDelegate);
    
    // 创建未读提示组件
    historyUnreadNotifier = new HistoryUnreadNotifier(this);
    historyUnreadNotifier->hide();
    historyMentionNotifier = new HistoryUnreadNotifier(this);
    historyMentionNotifier->setText(QStringLiteral("有人@我"));
    historyMentionNotifier->hide();
    historyUnreadNotifierLoadTimer = new QTimer(this);
    historyUnreadNotifierLoadTimer->setSingleShot(true);
    historyUnreadNotifierLoadTimer->setInterval(kHistoryUnreadNotifierLoadDelayMs);
    messageLoadingAnimationTimer = new QTimer(this);
    messageLoadingAnimationTimer->setInterval(kMessageLoadingSkeletonFrameMs);

    newMessageNotifier = new NewMessageNotifier(this);
    newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
    newMessageNotifier->hide();
    referenceMessageNotifier = new ReferenceMessageNotifier(this);
    referenceMessageNotifier->hide();

    bottomGapGradientOverlay = new BottomFadeOverlay(this);
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
    connect(historyMentionNotifier, &HistoryUnreadNotifier::clicked,
            this, &ChatArea::scrollToNextHistoryMention);
    connect(historyUnreadNotifierLoadTimer, &QTimer::timeout,
            this, &ChatArea::showHistoryUnreadNotifier);
    connect(messageLoadingAnimationTimer, &QTimer::timeout, this, [this]() {
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
    connect(&ImageService::instance(), &ImageService::previewReady, this, [this]() {
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
    connect(&ImageService::instance(), &ImageService::resourceChanged, this, [this](const QString&) {
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
    connect(newMessageNotifier, &NewMessageNotifier::clicked,
            this, &ChatArea::onNewMessageNotifierClicked);
    connect(referenceMessageNotifier, &ReferenceMessageNotifier::closeRequested,
            this, &ChatArea::onReferenceMessageCloseRequested);
    connect(chatView, &ChatListView::avatarClicked,
            this, &ChatArea::showFriendProfilePopup);
    connect(chatView, &ChatListView::avatarContextMenuRequested,
            this, &ChatArea::showAvatarContextMenu);
    connect(chatView, &ChatListView::groupSystemEventProfileRequested,
            this, &ChatArea::showFriendProfilePopup);
    connect(chatView, &ChatListView::referencedMessageClicked,
            this, &ChatArea::onReferencedMessageClicked);
    connect(friendProfilePopup, &FriendProfilePopup::requestMessage,
            this, &ChatArea::requestOpenConversation);
    connect(friendProfilePopup, &FriendProfilePopup::requestAddFriend,
            this, [this](const QString& userId) {
                AddContactSearchWindow::openUserRequest(userId, this);
            });
    connect(friendProfilePopup, &FriendProfilePopup::requestEditProfile,
            this, &ChatArea::showCurrentUserEditProfilePopup);
    connect(friendProfilePopup, &FriendProfilePopup::requestGroupNicknameChange,
            sessionController, &ChatSessionController::saveGroupMemberNickname);
    connect(inputBar, &FloatingInputBar::sendImage,
            this, &ChatArea::onSendImage);
    connect(inputBar, &FloatingInputBar::sendText,
            this, &ChatArea::onSendText);
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::messageSendFailed,
            this,
            [this](const QString& clientMessageId, const NetworkError& error) {
                markLocalSendFailed(clientMessageId);
                if (error.code == QStringLiteral("NOT_FRIEND")) {
                    const QString peerUserId = directPeerUserId();
                    if (!peerUserId.isEmpty()) {
                        UserRepository::instance().removeUser(peerUserId);
                    }
                    updateDirectRelationshipState(true);
                    GlobalNotification::showFailure(this, QStringLiteral("你们已不是好友，无法发送消息"));
                } else {
                    GlobalNotification::showFailure(this, QStringLiteral("消息发送失败"));
                }
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::messageSendBlocked,
            this,
            [this](const NetworkError& error) {
                if (error.code != QStringLiteral("NOT_FRIEND")) {
                    return;
                }
                const QString peerUserId = directPeerUserId();
                if (!peerUserId.isEmpty()) {
                    UserRepository::instance().removeUser(peerUserId);
                }
                failPendingLocalSendsForCurrentConversation();
                updateDirectRelationshipState(true);
                GlobalNotification::showFailure(this, QStringLiteral("你们已不是好友，无法发送消息"));
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::messageSendSucceeded,
            this,
            [this](const QString& clientMessageId) {
                markLocalSendSucceeded(clientMessageId);
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::imageUploadSucceeded,
            this,
            [this](const QString& clientMessageId) {
                setLocalSendState(clientMessageId, MessageSendState::Sending);
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::imageUploadFailed,
            this,
            [this](const QString& clientMessageId, const NetworkError&) {
                markLocalSendFailed(clientMessageId);
                GlobalNotification::showFailure(this, QStringLiteral("图片上传失败"));
            });
    connect(&ChatRemoteDataSource::instance(),
            &ChatRemoteDataSource::messageRecallFailed,
            this,
            [this](const QString&, const QString& changedConversationId, const QString&, const NetworkError&) {
                if (changedConversationId == conversationId()) {
                    GlobalNotification::showFailure(this, QStringLiteral("消息撤回失败"));
                }
            });
    connect(inputBar, &FloatingInputBar::inputFocused,
            this, &ChatArea::clearMessageSelection);
    connect(chatDelegate, &ChatItemDelegate::deleteRequested,
            this, &ChatArea::onDeleteMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::recallRequested,
            this, &ChatArea::onRecallMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::referenceRequested,
            this, &ChatArea::onReferenceMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::reeditRequested,
            this, &ChatArea::onReeditMessageRequested);
    connect(chatDelegate, &ChatItemDelegate::retrySendRequested,
            this, &ChatArea::onRetryMessageRequested);
    connect(infoButton, &QPushButton::clicked,
            this, &ChatArea::onInfoButtonClicked);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, applyHeaderTheme);
    connect(sessionController, &ChatSessionController::sessionChanged,
            this, [this](const ConversationMeta& meta, const User&, const Group&) {
                onSessionChanged(meta);
            });
    connect(&MessageRepository::instance(), &MessageRepository::lastMessageChanged,
            this, &ChatArea::appendRepositoryMessage);
    connect(&MessageRepository::instance(), &MessageRepository::messageUpdated,
            this, &ChatArea::replaceRepositoryMessage);
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, &ChatArea::refreshCurrentGroupMessageDisplayNames);
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, [this]() {
                refreshCurrentGroupMessageDisplayNames();
                updateDirectRelationshipState(true);
                if (chatView && chatView->viewport()) {
                    chatView->viewport()->update();
                }
            });
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
    connect(sessionController, &ChatSessionController::friendUpdateFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("好友资料保存失败"));
            });
    connect(sessionController, &ChatSessionController::friendDeleteFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("删除好友失败"));
            });
    connect(sessionController, &ChatSessionController::groupUpdateFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("群资料保存失败"));
            });
    connect(sessionController, &ChatSessionController::groupMySettingsUpdateFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("群设置保存失败"));
            });
    connect(sessionController, &ChatSessionController::groupBotCreateFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("机器人创建失败"));
            });
    connect(sessionController, &ChatSessionController::groupLeaveFailed,
            this, [this](const QString&, const NetworkError&) {
                GlobalNotification::showFailure(this, QStringLiteral("退出群聊失败"));
            });

}


void ChatArea::addMessage(QSharedPointer<ChatMessage> message)
{
    if (!message || conversationId().isEmpty()) {
        return;
    }

    const bool isOwnMessage = message->isFromMe();
    const bool isSystemEvent = isGroupSystemEventMessage(message.get());
    const bool shouldFollowIncomingMessage = !isSystemEvent && chatView->isBottomLocked();
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

    if (isOwnMessage && !isSystemEvent) {
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
            !chatModel) {
        return;
    }

    if (chatModel->indexForMessage(message.get()).isValid() ||
            (!message->getMessageId().isEmpty() &&
             chatModel->messageById(message->getMessageId())) ||
            (!message->getClientMessageId().isEmpty() &&
             chatModel->messageByClientMessageId(message->getClientMessageId()))) {
        return;
    }

    const bool isOwnMessage = message->isFromMe();
    const bool isSystemEvent = isGroupSystemEventMessage(message.get());
    const bool shouldFollowIncomingMessage = !isSystemEvent && chatView->isBottomLocked();
    chatModel->addMessage(message);
    assignAppendedPeerMessageOrdinal(message);
    ++m_state.loadedMessageCount;
    adjustBottomSpace();

    if (!isOwnMessage && !shouldFollowIncomingMessage) {
        registerNewUnreadCandidate(message);
        updateNewMessageNotifier();
    }

    if (isOwnMessage && !isSystemEvent) {
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

void ChatArea::replaceRepositoryMessage(const QString& changedConversationId,
                                        const ChatMessagePtr& message)
{
    if (changedConversationId.isEmpty() ||
            changedConversationId != conversationId() ||
            m_state.loadingInitialMessages ||
            message.isNull() ||
            !chatModel) {
        return;
    }

    QModelIndex messageIndex = chatModel->indexForMessageId(message->getMessageId());
    if (!messageIndex.isValid() && !message->getClientMessageId().isEmpty()) {
        messageIndex = chatModel->indexForClientMessageId(message->getClientMessageId());
    }
    if (!messageIndex.isValid()) {
        return;
    }

    const QSharedPointer<ChatMessage> previousMessage = chatModel->sharedMessageAt(messageIndex.row());
    if (previousMessage.isNull()) {
        return;
    }
    if (message->getType() == MessageType::Image &&
        previousMessage->getType() == MessageType::Image) {
        auto* imageMessage = static_cast<ImageMessage*>(message.data());
        const auto* previousImageMessage = static_cast<const ImageMessage*>(previousMessage.data());
        if (!imageMessage->getImageSize().isValid() &&
            previousImageMessage->getImageSize().isValid()) {
            imageMessage->setImageSize(previousImageMessage->getImageSize());
        }
    }

    const auto ordinalIt = m_state.peerMessageOrdinals.constFind(previousMessage.get());
    if (ordinalIt != m_state.peerMessageOrdinals.cend() && !message->isFromMe()) {
        m_state.peerMessageOrdinals.insert(message.get(), ordinalIt.value());
    }
    removeUnreadCandidate(previousMessage.get());
    chatModel->replaceMessage(messageIndex.row(), message);
    if (!message->getClientMessageId().isEmpty()) {
        m_pendingLocalSends.remove(message->getClientMessageId());
        updateMessageAnimationTimer();
    }
    if (message->getType() == MessageType::Recall) {
        const QSharedPointer<RecallMessage> recallMessage = message.dynamicCast<RecallMessage>();
        scheduleReeditExpiry(recallMessage);
    }
    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
    adjustBottomSpace();
}

void ChatArea::refreshCurrentGroupMessageDisplayNames()
{
    if (!chatModel || conversationId().isEmpty() || !isGroupMode()) {
        return;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
    if (group.groupId.isEmpty()) {
        return;
    }

    for (int row = 0; row < chatModel->rowCount(); ++row) {
        const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
        if (message.isNull() ||
            !message->isInGroupChat() ||
            (message->getSenderId().isEmpty() && message->getType() != MessageType::GroupMemberJoined &&
             message->getType() != MessageType::GroupSystemEvent)) {
            continue;
        }

        bool changed = false;
        if (!message->getSenderId().isEmpty()) {
            const QString nextName = groupMemberDisplayName(group, message->getSenderId());
            const GroupRole nextRole = groupRoleForUser(group, message->getSenderId());
            if (message->getSenderName() != nextName || message->getRole() != nextRole) {
                message->setSenderName(nextName);
                message->setRole(nextRole);
                changed = true;
            }
        }

        if (message->getType() == MessageType::Recall) {
            auto* recallMessage = static_cast<RecallMessage*>(message.data());
            if (!recallMessage->getActorId().isEmpty()) {
                const QString actorName = groupMemberDisplayName(group, recallMessage->getActorId());
                const GroupRole actorRole = groupRoleForUser(group, recallMessage->getActorId());
                if (recallMessage->getActorName() != actorName ||
                    recallMessage->getActorRole() != actorRole) {
                    recallMessage->setActorName(actorName);
                    recallMessage->setActorRole(actorRole);
                    changed = true;
                }
            }
        } else if (message->getType() == MessageType::GroupMemberJoined) {
            auto* joinedMessage = static_cast<GroupMemberJoinedMessage*>(message.data());
            if (!joinedMessage->getMemberId().isEmpty()) {
                const QString memberName = groupMemberDisplayName(group, joinedMessage->getMemberId());
                if (joinedMessage->getMemberName() != memberName) {
                    joinedMessage->setMemberName(memberName);
                    changed = true;
                }
            }
            if (!joinedMessage->getInviterId().isEmpty()) {
                const QString inviterName = groupMemberDisplayName(group, joinedMessage->getInviterId());
                if (joinedMessage->getInviterName() != inviterName) {
                    joinedMessage->setInviterName(inviterName);
                    changed = true;
                }
            }
        } else if (message->getType() == MessageType::GroupSystemEvent) {
            auto* eventMessage = static_cast<GroupSystemEventMessage*>(message.data());
            if (!eventMessage->getHighlightedUserId().isEmpty()) {
                const QString highlightedName = groupMemberDisplayName(group, eventMessage->getHighlightedUserId());
                if (eventMessage->getHighlightedName() != highlightedName) {
                    eventMessage->setHighlightedName(highlightedName);
                    changed = true;
                }
            }
        }

        if (changed) {
            chatModel->notifyMessageChanged(message.get());
        }
    }
    if (chatView && chatView->viewport()) {
        chatView->viewport()->update();
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
        if (!message || message->isFromMe() || isGroupSystemEventMessage(message.get())) {
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
    if (!message || message->isFromMe() || isGroupSystemEventMessage(message.get())) {
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
            (m_state.historyUnreadMessageCount <= 0 &&
             m_state.newUnreadMessageCount <= 0 &&
             m_state.pendingHistoryMentions.isEmpty()) ||
            m_state.loadingInitialMessages ||
            m_state.loadingOlderMessages) {
        return;
    }

    pruneVisibleHistoryMentions();

    if (!m_state.suppressHistoryUnreadVisibilityForMentionScroll) {
        const std::optional<int> firstVisibleOrdinal = firstVisiblePeerOrdinal();
        if (m_state.hasHistoryUnreadOrdinalRange &&
                firstVisibleOrdinal.has_value() &&
                firstVisibleOrdinal.value() <= m_state.historyUnreadLastOrdinal &&
                firstVisibleOrdinal.value() < m_state.historyReadMinOrdinal) {
            m_state.historyReadMinOrdinal = qMax(m_state.historyUnreadFirstOrdinal,
                                                 firstVisibleOrdinal.value());
            recalculateHistoryUnreadCount();
        }
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
    if (index.isValid()) {
        const std::optional<int> ordinal =
                peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrAfter(index.row()));
        if (ordinal.has_value()) {
            return ordinal;
        }
    }

    for (int row = 0; row < chatModel->rowCount(); ++row) {
        if (!chatView->visualRect(chatModel->index(row, 0)).intersects(viewportRect)) {
            continue;
        }

        const std::optional<int> ordinal =
                peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrAfter(row));
        if (ordinal.has_value()) {
            return ordinal;
        }
    }
    return std::nullopt;
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
    if (index.isValid()) {
        const std::optional<int> ordinal =
                peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrBefore(index.row()));
        if (ordinal.has_value()) {
            return ordinal;
        }
    }

    for (int row = chatModel->rowCount() - 1; row >= 0; --row) {
        if (!chatView->visualRect(chatModel->index(row, 0)).intersects(viewportRect)) {
            continue;
        }

        const std::optional<int> ordinal =
                peerOrdinalForRow(chatModel->nearestPeerMessageRowAtOrBefore(row));
        if (ordinal.has_value()) {
            return ordinal;
        }
    }
    return std::nullopt;
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
        if (!message || message->isFromMe() || isGroupSystemEventMessage(message)) {
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
        if (!message || message->isFromMe() || isGroupSystemEventMessage(message.get())) {
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
        if (message && !message->isFromMe() && !isGroupSystemEventMessage(message.get())) {
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
        if (!message || message->isFromMe() || isGroupSystemEventMessage(message.get())) {
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
    if (!message || message->isFromMe() || isGroupSystemEventMessage(message.get())) {
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
    updateHistoryMentionNotifierPosition();
    updateReferenceMessageNotifierPosition();
    updateNewMessageNotifierPosition();
    updateInputBarPosition();
    adjustBottomSpace();
    scheduleVisibleUnreadCheck();

    // 确保新消息提示器在最上层
    if (historyUnreadNotifier && historyUnreadNotifier->isVisible()) {
        historyUnreadNotifier->raise();
    }
    if (historyMentionNotifier && historyMentionNotifier->isVisible()) {
        historyMentionNotifier->raise();
    }
    if (newMessageNotifier && newMessageNotifier->isVisible()) {
        newMessageNotifier->raise();
    }
    if (referenceMessageNotifier && referenceMessageNotifier->isVisible()) {
        referenceMessageNotifier->raise();
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
            newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
            newMessageNotifier->hide();
            updateReferenceMessageNotifierPosition();
            return;
        }
    }

    if (m_state.newUnreadMessageCount > 0) {
        newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::Count);
        newMessageNotifier->setMessageCount(m_state.newUnreadMessageCount);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        updateReferenceMessageNotifierPosition();
        newMessageNotifier->raise();
        return;
    }

    if (m_state.newMessageNotifierRevealedByDownScroll && shouldShowNewMessageNotifier()) {
        newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        updateReferenceMessageNotifierPosition();
        newMessageNotifier->raise();
        return;
    }

    newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
    newMessageNotifier->hide();
    updateReferenceMessageNotifierPosition();
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
    updateHistoryMentionNotifierPosition();
}

void ChatArea::hideHistoryUnreadNotifier()
{
    if (historyUnreadNotifierLoadTimer) {
        historyUnreadNotifierLoadTimer->stop();
    }
    if (historyUnreadNotifier) {
        historyUnreadNotifier->hide();
    }
    updateHistoryMentionNotifierPosition();
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

void ChatArea::updateHistoryMentionNotifier()
{
    if (!historyMentionNotifier) {
        return;
    }

    if (!m_state.pendingHistoryMentions.isEmpty()) {
        showHistoryMentionNotifier();
        return;
    }

    hideHistoryMentionNotifier();
}

void ChatArea::showHistoryMentionNotifier()
{
    if (!historyMentionNotifier) {
        return;
    }
    if (conversationId().isEmpty() || m_state.pendingHistoryMentions.isEmpty()) {
        historyMentionNotifier->hide();
        return;
    }

    historyMentionNotifier->setText(QStringLiteral("有人@我"));
    historyMentionNotifier->show();
    historyMentionNotifier->raise();
    updateHistoryMentionNotifierPosition();
}

void ChatArea::hideHistoryMentionNotifier()
{
    if (historyMentionNotifier) {
        historyMentionNotifier->hide();
    }
}

void ChatArea::updateHistoryMentionNotifierPosition()
{
    if (!historyMentionNotifier || !historyMentionNotifier->isVisible() || !chatView) {
        return;
    }

    const QRect chatViewRect = chatView->geometry();
    const int rightEdge = chatViewRect.right() + 1 - visibleInfoPanelWidth();
    const int x = qMax(chatViewRect.left() + 12, rightEdge - historyMentionNotifier->width());
    int y = chatViewRect.top() + 12;
    if (historyUnreadNotifier && historyUnreadNotifier->isVisible()) {
        y = historyUnreadNotifier->geometry().bottom() + 6;
    }
    historyMentionNotifier->move(x, y);
    historyMentionNotifier->raise();
}

QModelIndex ChatArea::indexForMentionRef(const ConversationMentionRef& mention) const
{
    if (!chatModel || mention.messageId.isEmpty()) {
        return {};
    }
    return chatModel->indexForMessageId(mention.messageId);
}

void ChatArea::setPendingHistoryMentions(QVector<ConversationMentionRef> mentions)
{
    QSet<QString> seenMessageIds;
    QVector<ConversationMentionRef> uniqueMentions;
    uniqueMentions.reserve(mentions.size());
    for (const ConversationMentionRef& mention : std::as_const(mentions)) {
        if (mention.messageId.isEmpty() || seenMessageIds.contains(mention.messageId)) {
            continue;
        }
        uniqueMentions.push_back(mention);
        seenMessageIds.insert(mention.messageId);
    }

    std::sort(uniqueMentions.begin(),
              uniqueMentions.end(),
              [](const ConversationMentionRef& lhs, const ConversationMentionRef& rhs) {
                  if (lhs.messageSeq > 0 && rhs.messageSeq > 0 && lhs.messageSeq != rhs.messageSeq) {
                      return lhs.messageSeq > rhs.messageSeq;
                  }
                  if (lhs.timestamp != rhs.timestamp) {
                      return lhs.timestamp > rhs.timestamp;
                  }
                  return lhs.messageId > rhs.messageId;
              });
    m_state.pendingHistoryMentions = std::move(uniqueMentions);
    pruneVisibleHistoryMentions();
    updateHistoryMentionNotifier();
}

void ChatArea::pruneVisibleHistoryMentions()
{
    if (m_state.pendingHistoryMentions.isEmpty() || !chatView || !chatView->viewport()) {
        return;
    }

    const QRect viewportRect = effectiveMessageViewportRect();
    if (viewportRect.isEmpty()) {
        return;
    }

    bool changed = false;
    for (int index = m_state.pendingHistoryMentions.size() - 1; index >= 0; --index) {
        const QModelIndex messageIndex = indexForMentionRef(m_state.pendingHistoryMentions.at(index));
        if (!messageIndex.isValid()) {
            continue;
        }
        if (!chatView->visualRect(messageIndex).intersects(viewportRect)) {
            continue;
        }

        m_state.pendingHistoryMentions.removeAt(index);
        changed = true;
    }

    if (changed) {
        updateHistoryMentionNotifier();
    }
}

void ChatArea::scrollToNextHistoryMention()
{
    while (!m_state.pendingHistoryMentions.isEmpty()) {
        const ConversationMentionRef mention = m_state.pendingHistoryMentions.first();
        if (mention.messageId.isEmpty()) {
            m_state.pendingHistoryMentions.removeFirst();
            continue;
        }

        if (m_state.loadingOlderMessages) {
            m_state.pendingHistoryMentionScroll = true;
            return;
        }

        if (!ensureMessageLoaded(mention.messageId)) {
            m_state.pendingHistoryMentions.removeFirst();
            updateHistoryMentionNotifier();
            continue;
        }

        m_state.suppressHistoryUnreadVisibilityForMentionScroll = true;
        const int mentionScrollGeneration = ++m_historyMentionScrollGeneration;
        scrollToMessageAndHighlight(mention.messageId);
        QTimer::singleShot(220, this, [this]() {
            pruneVisibleHistoryMentions();
        });
        QTimer::singleShot(900, this, [this, mentionScrollGeneration]() {
            pruneVisibleHistoryMentions();
            if (m_historyMentionScrollGeneration == mentionScrollGeneration) {
                m_state.suppressHistoryUnreadVisibilityForMentionScroll = false;
            }
        });
        return;
    }

    updateHistoryMentionNotifier();
}

void ChatArea::updateNewMessageNotifierPosition()
{
    if (newMessageNotifier->isVisible() && inputBar) {
        const QRect inputBarRect = inputBar->geometry();
        const int x = inputBarRect.x() + inputBarRect.width() - newMessageNotifier->width();
        const int y = inputBarRect.y() - newMessageNotifier->height() - kFloatingNotifierInputGap;
        newMessageNotifier->setGeometry(x, y, newMessageNotifier->width(), newMessageNotifier->height());
        newMessageNotifier->raise();
    }
}

void ChatArea::updateReferenceMessageNotifierPosition()
{
    if (!referenceMessageNotifier || !referenceMessageNotifier->isVisible() || !inputBar) {
        return;
    }

    const QRect inputBarRect = inputBar->geometry();
    const int newNotifierWidth = newMessageNotifier
            ? newMessageNotifier->sizeHint().width()
            : 0;
    const int referenceRight = inputBarRect.right() + 1 - newNotifierWidth - kFloatingNotifierHorizontalGap;
    const int availableWidth = qMax(0, referenceRight - inputBarRect.x());
    if (availableWidth <= 0) {
        referenceMessageNotifier->hide();
        return;
    }
    const int notifierHeight = newMessageNotifier
            ? newMessageNotifier->height()
            : referenceMessageNotifier->sizeHint().height();
    const int y = inputBarRect.y() - notifierHeight - kFloatingNotifierInputGap;
    referenceMessageNotifier->setGeometry(inputBarRect.x(), y, availableWidth, notifierHeight);
    referenceMessageNotifier->raise();
}

void ChatArea::updateReferenceMessageNotifier()
{
    if (!referenceMessageNotifier) {
        return;
    }

    if (m_pendingReferenceMessageId.isEmpty() ||
            conversationId().isEmpty() ||
            m_systemFloatingBarsSuppressed ||
            !inputBar ||
            inputBar->isHidden()) {
        referenceMessageNotifier->hide();
        updateNewMessageNotifierPosition();
        return;
    }

    const ChatMessage* referencedMessage = chatModel
            ? chatModel->messageById(m_pendingReferenceMessageId)
            : nullptr;
    if (!referencedMessage) {
        referencedMessage = MessageRepository::instance()
                .requestMessageById(conversationId(), m_pendingReferenceMessageId)
                .get();
    }

    referenceMessageNotifier->setMessage(referencedMessage);
    referenceMessageNotifier->show();
    updateReferenceMessageNotifierPosition();
    updateNewMessageNotifierPosition();
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

void ChatArea::registerPendingLocalSend(const PendingLocalSend& pending,
                                        const ChatMessagePtr& message)
{
    if (pending.clientMessageId.isEmpty() || message.isNull()) {
        return;
    }

    m_pendingLocalSends.insert(pending.clientMessageId, pending);
    schedulePendingLocalSendTimeout(pending.clientMessageId);
    updateMessageAnimationTimer();
}

void ChatArea::schedulePendingLocalSendTimeout(const QString& clientMessageId)
{
    if (clientMessageId.isEmpty()) {
        return;
    }

    const int attempt = m_pendingLocalSends.value(clientMessageId).attempt;
    QTimer::singleShot(kMessageSendAckTimeoutMs, this, [this, clientMessageId, attempt]() {
        const PendingLocalSend pending = m_pendingLocalSends.value(clientMessageId);
        if (pending.clientMessageId.isEmpty() ||
            pending.conversationId != conversationId() ||
            pending.attempt != attempt ||
            !chatModel) {
            return;
        }

        const QModelIndex messageIndex = chatModel->indexForClientMessageId(clientMessageId);
        const QSharedPointer<ChatMessage> message = messageIndex.isValid()
                ? chatModel->sharedMessageAt(messageIndex.row())
                : QSharedPointer<ChatMessage>();
        if (message.isNull()) {
            return;
        }
        const MessageSendState state = message->getSendState();
        if (state == MessageSendState::Uploading || state == MessageSendState::Sending) {
            markLocalSendFailed(clientMessageId);
        }
    });
}

void ChatArea::setLocalSendState(const QString& clientMessageId,
                                 MessageSendState state)
{
    if (clientMessageId.isEmpty() || !chatModel) {
        return;
    }

    const QModelIndex messageIndex = chatModel->indexForClientMessageId(clientMessageId);
    if (!messageIndex.isValid()) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(messageIndex.row());
    if (message.isNull() || message->getSendState() == state) {
        return;
    }

    message->setSendState(state);
    if (state == MessageSendState::Failed || state == MessageSendState::Sent) {
        MessageRepository::instance().persistMessage(conversationId(), message);
    }
    chatModel->notifyMessageChanged(message.get());
    if (chatView && chatView->viewport()) {
        chatView->viewport()->update();
    }
    updateMessageAnimationTimer();
}

void ChatArea::markLocalSendSucceeded(const QString& clientMessageId)
{
    if (clientMessageId.isEmpty()) {
        return;
    }

    m_pendingLocalSends.remove(clientMessageId);
    setLocalSendState(clientMessageId, MessageSendState::Sent);
    updateMessageAnimationTimer();
}

void ChatArea::markLocalSendFailed(const QString& clientMessageId)
{
    if (clientMessageId.isEmpty()) {
        return;
    }

    if (!m_pendingLocalSends.contains(clientMessageId)) {
        return;
    }
    setLocalSendState(clientMessageId, MessageSendState::Failed);
    updateMessageAnimationTimer();
}

void ChatArea::failPendingLocalSendsForCurrentConversation()
{
    if (conversationId().isEmpty()) {
        return;
    }

    const QList<QString> clientMessageIds = m_pendingLocalSends.keys();
    for (const QString& clientMessageId : clientMessageIds) {
        const PendingLocalSend pending = m_pendingLocalSends.value(clientMessageId);
        if (pending.conversationId == conversationId()) {
            markLocalSendFailed(clientMessageId);
        }
    }
}

bool ChatArea::isDirectRelationshipUnavailable() const
{
    const QString peerUserId = directPeerUserId();
    return !peerUserId.isEmpty() &&
           !isGroupMode() &&
           !UserRepository::instance().isFriend(peerUserId);
}

QString ChatArea::directRelationshipDeletedNoticeMessageId() const
{
    return QStringLiteral("__direct_relationship_deleted_notice_%1").arg(conversationId());
}

void ChatArea::updateDirectRelationshipState(bool scrollToNotice)
{
    const bool unavailable = isDirectRelationshipUnavailable();
    if (m_state.directRelationshipDeleted == unavailable) {
        if (unavailable) {
            appendDirectRelationshipDeletedNotice(scrollToNotice);
        }
        return;
    }

    m_state.directRelationshipDeleted = unavailable;
    if (unavailable) {
        appendDirectRelationshipDeletedNotice(scrollToNotice);
    } else {
        removeDirectRelationshipDeletedNotice();
    }
}

void ChatArea::appendDirectRelationshipDeletedNotice(bool scrollToNotice)
{
    if (!chatModel || conversationId().isEmpty() || isGroupMode()) {
        return;
    }

    const QString noticeId = directRelationshipDeletedNoticeMessageId();
    if (chatModel->messageById(noticeId)) {
        return;
    }

    auto notice = QSharedPointer<GroupSystemEventMessage>::create(
            QString(),
            QString::fromUtf8(kDirectRelationshipDeletedNoticeText));
    notice->setMessageId(noticeId);
    chatModel->addMessage(notice);
    adjustBottomSpace();

    if (scrollToNotice && chatView && chatView->isBottomLocked()) {
        QTimer::singleShot(0, this, [this]() {
            scrollToBottom(true);
        });
    }
}

void ChatArea::removeDirectRelationshipDeletedNotice()
{
    if (!chatModel || conversationId().isEmpty()) {
        return;
    }

    const QModelIndex noticeIndex =
            chatModel->indexForMessageId(directRelationshipDeletedNoticeMessageId());
    if (!noticeIndex.isValid()) {
        return;
    }
    chatModel->removeMessage(noticeIndex.row());
    adjustBottomSpace();
}

void ChatArea::updateMessageAnimationTimer()
{
    if (!messageLoadingAnimationTimer) {
        return;
    }

    bool hasAnimatingSend = false;
    if (chatModel) {
        for (const PendingLocalSend& pending : std::as_const(m_pendingLocalSends)) {
            const QModelIndex index = chatModel->indexForClientMessageId(pending.clientMessageId);
            const QSharedPointer<ChatMessage> message = index.isValid()
                    ? chatModel->sharedMessageAt(index.row())
                    : QSharedPointer<ChatMessage>();
            if (message &&
                (message->getSendState() == MessageSendState::Uploading ||
                 message->getSendState() == MessageSendState::Sending)) {
                hasAnimatingSend = true;
                break;
            }
        }
    }

    if (hasAnimatingSend || m_state.loadingInitialMessages) {
        if (!messageLoadingAnimationTimer->isActive()) {
            messageLoadingAnimationTimer->start();
        }
    } else if (messageLoadingAnimationTimer->isActive()) {
        messageLoadingAnimationTimer->stop();
    }
}

void ChatArea::applyConversationMeta()
{
    if (m_state.meta.isGroup) {
        statusIcon->hide();
        nameLabel->setText(QString("%1（%2）").arg(m_state.meta.title, QString::number(m_state.meta.memberCount)));
        updateGroupInfoPanelState();
        return;
    }

    statusIcon->show();
    statusIcon->setPixmap(ImageService::instance().scaled(statusIconPath(m_state.meta.status),
                                                          QSize(12, 12)));
    nameLabel->setText(m_state.meta.title);
    updateDirectInfoPanelState(false);
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
    connect(panel, &GroupConversationInfoPanel::groupBotCreateRequested,
            sessionController, &ChatSessionController::createGroupBot);
    connect(panel, &GroupConversationInfoPanel::groupMemberRemovalRequested,
            sessionController, &ChatSessionController::removeGroupMember);
    connect(panel, &GroupConversationInfoPanel::groupMembersBatchRemovalRequested,
            sessionController, &ChatSessionController::removeGroupMembers);
    connect(panel, &GroupConversationInfoPanel::groupOwnerTransferRequested,
            sessionController, &ChatSessionController::transferGroupOwner);
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
    connect(panel, &GroupConversationInfoPanel::memberMentionRequested,
            this, &ChatArea::mentionUser);
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

    if (isGroupMode() && !conversationId().isEmpty()) {
        const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
        friendProfilePopup->setGroupContext(group, canEditGroupMemberNickname(group, userId));
    } else {
        friendProfilePopup->clearGroupContext();
    }
    friendProfilePopup->popupAt(globalPos, userId);
}

void ChatArea::mentionUser(const QString& userId)
{
    if (!isGroupMode() || userId.isEmpty() || !inputBar) {
        return;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
    if (group.groupId.isEmpty()) {
        return;
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    const QString targetUserUuid = mentionTargetUserUuid(user, userId).trimmed();
    if (targetUserUuid.isEmpty()) {
        return;
    }

    const QString displayName = mentionDisplayName(group, user, targetUserUuid).trimmed();
    if (displayName.isEmpty()) {
        return;
    }

    ChatMessageMention mention;
    const GroupMemberProfile member = GroupRepository::instance().requestGroupMember(group.groupId,
                                                                                     targetUserUuid);
    const bool isAiMention = user.isAi || member.role == GroupMemberRoleValue::Ai;
    mention.targetType = isAiMention ? QStringLiteral("ai") : QStringLiteral("user");
    mention.targetUserUuid = targetUserUuid;
    if (isAiMention) {
        mention.targetId = !user.aiAgentId.trimmed().isEmpty()
                ? user.aiAgentId.trimmed()
                : mentionTargetId(user, targetUserUuid);
    } else {
        mention.targetId = mentionTargetId(user, targetUserUuid);
    }

    bool found = false;
    for (PendingMention& pending : m_pendingMentions) {
        if (pending.mention.targetType == mention.targetType &&
            pending.mention.targetUserUuid == targetUserUuid) {
            pending.mention = mention;
            pending.displayText = displayName;
            found = true;
            break;
        }
    }
    if (!found) {
        m_pendingMentions.push_back(PendingMention{mention, displayName});
    }

    inputBar->appendText(QStringLiteral("@%1 ").arg(displayName));
    inputBar->focusInput();
}

QVector<ChatMessageMention> ChatArea::mentionsForText(const QString& text) const
{
    QVector<ChatMessageMention> mentions;
    if (!isGroupMode() || text.trimmed().isEmpty()) {
        return mentions;
    }

    QSet<QString> seenTargets;
    for (const PendingMention& pending : m_pendingMentions) {
        const QString targetType = pending.mention.targetType.trimmed().toLower();
        if ((targetType != QStringLiteral("user") && targetType != QStringLiteral("ai")) ||
            pending.displayText.trimmed().isEmpty() ||
            (pending.mention.targetUserUuid.isEmpty() && pending.mention.targetId.isEmpty())) {
            continue;
        }

        const QString dedupeKey = QStringLiteral("%1:%2")
                .arg(targetType,
                     pending.mention.targetUserUuid.isEmpty()
                             ? pending.mention.targetId
                             : pending.mention.targetUserUuid);
        if (seenTargets.contains(dedupeKey)) {
            continue;
        }

        if (!text.contains(QStringLiteral("@%1").arg(pending.displayText))) {
            continue;
        }

        ChatMessageMention mention = pending.mention;
        mention.position = mentions.size() + 1;
        mentions.push_back(mention);
        seenTargets.insert(dedupeKey);
    }
    return mentions;
}

void ChatArea::clearPendingMentions()
{
    m_pendingMentions.clear();
}

void ChatArea::showCurrentUserEditProfilePopup()
{
    if (currentUserEditProfilePopup) {
        currentUserEditProfilePopup->raise();
        return;
    }

    const CurrentUserProfile profile = CurrentUser::instance().profile();
    if (!profile.isValid()) {
        return;
    }

    auto* content = new CurrentUserProfileEditContent(profile);
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(520, 520);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    currentUserEditProfilePopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!currentUserEditProfilePopup) {
        return;
    }

    content->saveRequested = [this](const CurrentUserProfile& editedProfile) {
        const QString requestId = CurrentUser::instance().saveProfile(editedProfile);
        if (requestId.isEmpty()) {
            GlobalNotification::showFailure(this, QStringLiteral("资料保存失败"));
            return;
        }
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    content->cancelRequested = [this]() {
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    };

    connect(currentUserEditProfilePopup,
            &InWindowPopupOverlay::dismissed,
            this,
            [this]() {
                currentUserEditProfilePopup = nullptr;
            });
}

void ChatArea::showAvatarContextMenu(const QString& userId, const QPoint& globalPos)
{
    if (userId.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(ThemeManager::instance().color(ThemeColor::ContextMenuHover));

    const bool isCurrentUser = CurrentUser::instance().isCurrentUserId(userId);
    const User contextUser = UserRepository::instance().requestUserDetail({userId});
    const GroupMemberProfile contextMember = isGroupMode()
            ? GroupRepository::instance().requestGroupMember(groupId(), userId)
            : GroupMemberProfile{};
    const bool isAiUser = contextUser.isAi || contextMember.role == GroupMemberRoleValue::Ai;
    const bool isFriend = !isCurrentUser && UserRepository::instance().isFriend(userId);
    if (isGroupMode()) {
        QAction* mentionAction = menu->addAction(QStringLiteral("@TA"));
        connect(mentionAction, &QAction::triggered, this, [this, userId]() {
            mentionUser(userId);
        });
    }

    if (!isCurrentUser && !isAiUser) {
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

    connect(menu, &StyledActionMenu::aboutToHide, menu, [menu]() {
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
        const QString requestedConversationId = conversationId();
        QTimer::singleShot(0, this, [this, requestedConversationId]() {
            if (infoPanelOpen && conversationId() == requestedConversationId) {
                requestInfoPanelData(true);
            }
        });
        return;
    }

    AudioService::instance().play(AudioService::SoundEffect::ChestOpen);
    infoPanelAnimation->setTargetObject(panel);
    infoPanelAnimation->setStartValue(panel->geometry());
    infoPanelAnimation->setEndValue(openRect);
    infoPanelAnimation->start();

    const QString requestedConversationId = conversationId();
    QTimer::singleShot(0, this, [this, requestedConversationId]() {
        if (infoPanelOpen && conversationId() == requestedConversationId) {
            requestInfoPanelData(true);
        }
    });
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
    updateHistoryUnreadNotifierPosition();
    updateHistoryMentionNotifierPosition();
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

    const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
    if (!group.groupId.isEmpty()) {
        const bool canEditGroupInfo =
                GroupRepository::instance().isCurrentUserGroupOwner(group) ||
                GroupRepository::instance().isCurrentUserGroupAdmin(group);
        groupInfoPanel->setGroupState(group, canEditGroupInfo, !groupId().isEmpty());
    }
    groupInfoPanel->setConversationMeta(m_state.meta);
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
                                      const QVector<GroupMemberProfile>& previewMembers,
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
    m_state.pendingHistoryMentionScroll = false;
    m_state.suppressHistoryUnreadVisibilityForMentionScroll = false;
    ++m_historyMentionScrollGeneration;
    m_state.pendingHistoryMentions.clear();
    m_state.hasHistoryUnreadOrdinalRange = false;
    m_state.hasNewUnreadOrdinalRange = false;
    m_state.peerMessageOrdinals.clear();
    m_state.minPeerMessageOrdinal = 0;
    m_state.maxPeerMessageOrdinal = -1;
    m_state.hasMoreBefore = false;
    m_state.loadingOlderMessages = false;
    m_state.loadingInitialMessages = false;
    m_pendingLocalSends.clear();
    if (messageLoadingAnimationTimer) {
        messageLoadingAnimationTimer->stop();
    }
    hideHistoryUnreadNotifier();
    hideHistoryMentionNotifier();
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
            if (referenceMessageNotifier) {
                referenceMessageNotifier->hide();
            }
            if (historyUnreadNotifier) {
                hideHistoryUnreadNotifier();
            }
            if (historyMentionNotifier) {
                hideHistoryMentionNotifier();
            }
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

        if (m_systemFloatingBarsSuppressed) {
            inputBar->setProperty("systemFloatingBarsSuppressed", true);
            inputBar->setProperty("systemFloatingBarsSuppressedOpacity", 0.0);
            inputBar->refreshPlatformAppearance();
            if (referenceMessageNotifier) {
                referenceMessageNotifier->hide();
            }
            if (bottomGapGradientOverlay) {
                bottomGapGradientOverlay->hide();
            }
            updateHistoryUnreadNotifierPosition();
            updateHistoryMentionNotifierPosition();
            updateNewMessageNotifierPosition();
            return;
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
        updateReferenceMessageNotifier();
        updateHistoryUnreadNotifierPosition();
        updateHistoryMentionNotifierPosition();
        updateNewMessageNotifierPosition();
    }
}

void ChatArea::onSendImage(const QString &path)
{
    const QSize imageSize = ImageService::instance().sourceSize(path);
    if (imageSize.isValid()) {
        const QString referencedMessageId = m_pendingReferenceMessageId;
        if (isDirectRelationshipUnavailable()) {
            const QString clientMessageId = QStringLiteral("msg_%1").arg(
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
            const QString senderId = currentUserSenderId();
            auto ptr = QSharedPointer<ImageMessage>::create(path,
                                                            true,
                                                            senderId,
                                                            false,
                                                            CurrentUser::instance().getUserName(),
                                                            GroupRole::Member,
                                                            imageSize);
            ptr->setClientMessageId(clientMessageId);
            ptr->setSendState(MessageSendState::Failed);
            applyPendingReference(ptr);
            addMessage(ptr);
            registerPendingLocalSend(PendingLocalSend{
                                             conversationId(),
                                             clientMessageId,
                                             {},
                                             path,
                                             referencedMessageId,
                                             true
                                     },
                                     ptr);
            updateDirectRelationshipState(true);
            GlobalNotification::showFailure(this, QStringLiteral("你们已不是好友，无法发送消息"));
            return;
        }
        const QString clientMessageId = ChatRemoteDataSource::instance().sendImageMessage(
                conversationId(),
                path,
                referencedMessageId);
        if (clientMessageId.isEmpty()) {
            GlobalNotification::showFailure(this, QStringLiteral("图片发送失败"));
            return;
        }
        GroupRole role = GroupRole::Member;
        QString senderName = CurrentUser::instance().getUserName();
        const QString senderId = currentUserSenderId();
        if (isGroupMode()) {
            const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
            role = groupRoleForUser(group, senderId);
            senderName = groupMemberDisplayName(group, senderId);
        }
        auto ptr =
                QSharedPointer<ImageMessage>::create(path,
                                               true,
                                               senderId,
                                               isGroupMode(),
                                               senderName,
                                               role,
                                               imageSize);
        ptr->setClientMessageId(clientMessageId);
        ptr->setSendState(MessageSendState::Uploading);
        applyPendingReference(ptr);
        addMessage(ptr);
        registerPendingLocalSend(PendingLocalSend{
                                         conversationId(),
                                         clientMessageId,
                                         {},
                                         path,
                                         referencedMessageId,
                                         true
                                 },
                                 ptr);
    }
}

void ChatArea::onSendText(const QString &text)
{
    if (!text.trimmed().isEmpty()) {
        const QString referencedMessageId = m_pendingReferenceMessageId;
        const QVector<ChatMessageMention> mentions = mentionsForText(text);
        clearPendingMentions();
        if (isDirectRelationshipUnavailable()) {
            const QString clientMessageId = QStringLiteral("msg_%1").arg(
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
            auto ptr = QSharedPointer<TextMessage>::create(text,
                                                           true,
                                                           currentUserSenderId(),
                                                           false,
                                                           CurrentUser::instance().getUserName(),
                                                           GroupRole::Member);
            ptr->setClientMessageId(clientMessageId);
            ptr->setMentions(mentions);
            ptr->setSendState(MessageSendState::Failed);
            applyPendingReference(ptr);
            addMessage(ptr);
            PendingLocalSend pending{
                    conversationId(),
                    clientMessageId,
                    text,
                    {},
                    referencedMessageId,
                    false
            };
            pending.mentions = mentions;
            registerPendingLocalSend(pending, ptr);
            updateDirectRelationshipState(true);
            GlobalNotification::showFailure(this, QStringLiteral("你们已不是好友，无法发送消息"));
            return;
        }
        const QString clientMessageId = ChatRemoteDataSource::instance().sendTextMessage(
                conversationId(),
                text,
                referencedMessageId,
                {},
                mentions);
        if (clientMessageId.isEmpty()) {
            GlobalNotification::showFailure(this, QStringLiteral("消息发送失败"));
            return;
        }
        GroupRole role = GroupRole::Member;
        QString senderName = CurrentUser::instance().getUserName();
        const QString senderId = currentUserSenderId();
        if (isGroupMode()) {
            const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
            role = groupRoleForUser(group, senderId);
            senderName = groupMemberDisplayName(group, senderId);
        }
        auto ptr =
                QSharedPointer<TextMessage>::create(text,
                                               true,
                                               senderId,
                                               isGroupMode(),
                                               senderName,
                                               role);
        ptr->setClientMessageId(clientMessageId);
        ptr->setMentions(mentions);
        ptr->setSendState(MessageSendState::Sending);
        applyPendingReference(ptr);
        addMessage(ptr);
        PendingLocalSend pending{
                conversationId(),
                clientMessageId,
                text,
                {},
                referencedMessageId,
                false
        };
        pending.mentions = mentions;
        registerPendingLocalSend(pending, ptr);
    }
}

void ChatArea::onRetryMessageRequested(int row)
{
    if (!chatModel || conversationId().isEmpty()) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull() ||
        !message->isFromMe() ||
        message->getSendState() != MessageSendState::Failed ||
        message->getClientMessageId().isEmpty()) {
        return;
    }

    const QString clientMessageId = message->getClientMessageId();
    const PendingLocalSend pending = m_pendingLocalSends.value(clientMessageId);
    if (pending.clientMessageId.isEmpty() || pending.conversationId != conversationId()) {
        return;
    }

    if (isDirectRelationshipUnavailable()) {
        markLocalSendFailed(clientMessageId);
        updateDirectRelationshipState(true);
        GlobalNotification::showFailure(this, QStringLiteral("你们已不是好友，无法发送消息"));
        return;
    }

    const MessageSendState retryState = pending.isImage
            ? MessageSendState::Uploading
            : MessageSendState::Sending;
    PendingLocalSend nextPending = pending;
    ++nextPending.attempt;
    m_pendingLocalSends.insert(clientMessageId, nextPending);
    setLocalSendState(clientMessageId, retryState);

    QString returnedClientMessageId;
    if (nextPending.isImage) {
        returnedClientMessageId = ChatRemoteDataSource::instance().sendImageMessage(
                nextPending.conversationId,
                nextPending.imagePath,
                nextPending.referencedMessageId,
                nextPending.clientMessageId);
    } else {
        returnedClientMessageId = ChatRemoteDataSource::instance().sendTextMessage(
                nextPending.conversationId,
                nextPending.text,
                nextPending.referencedMessageId,
                nextPending.clientMessageId,
                nextPending.mentions);
    }

    if (returnedClientMessageId.isEmpty()) {
        markLocalSendFailed(clientMessageId);
        GlobalNotification::showFailure(this, nextPending.isImage
                                        ? QStringLiteral("图片发送失败")
                                        : QStringLiteral("消息发送失败"));
        return;
    }

    schedulePendingLocalSendTimeout(clientMessageId);
    updateMessageAnimationTimer();
}

bool ChatArea::canRecallMessage(const ChatMessage* message) const
{
    if (!message ||
        message->getType() == MessageType::Recall ||
        message->getType() == MessageType::GroupMemberJoined ||
        message->getType() == MessageType::GroupSystemEvent ||
        conversationId().isEmpty()) {
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

    const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
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
    recallMessage->setMessageId(message->getMessageId());
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
            const Group group = GroupRepository::instance().requestGroupDetail({groupId()});
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
    if (conversationId().isEmpty() || !chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull() || !canRecallMessage(message.get())) {
        return;
    }

    const QString requestId = ChatRemoteDataSource::instance().recallMessage(conversationId(),
                                                                             message->getMessageId());
    if (requestId.isEmpty()) {
        GlobalNotification::showFailure(this, QStringLiteral("消息撤回失败"));
    }
}

void ChatArea::onReferenceMessageRequested(int row)
{
    if (!chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull() ||
            message->getType() == MessageType::GroupMemberJoined ||
            message->getType() == MessageType::GroupSystemEvent) {
        return;
    }

    m_pendingReferenceMessageId = message->getMessageId();
    if (referenceMessageNotifier) {
        referenceMessageNotifier->setMessage(message.data());
        referenceMessageNotifier->show();
        updateReferenceMessageNotifierPosition();
        updateNewMessageNotifierPosition();
    }
    if (isGroupMode() &&
        message->getRole() == GroupRole::Ai &&
        !message->getSenderId().isEmpty()) {
        mentionUser(message->getSenderId());
        return;
    }
    if (inputBar) {
        inputBar->focusInput();
    }
}

void ChatArea::onReferenceMessageCloseRequested()
{
    m_pendingReferenceMessageId.clear();
    if (referenceMessageNotifier) {
        referenceMessageNotifier->hide();
    }
    updateNewMessageNotifierPosition();
}

void ChatArea::onReferencedMessageClicked(const QString& messageId)
{
    scrollToMessageAndHighlight(messageId);
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

    if (m_pendingReferenceMessageId == message->getMessageId()) {
        onReferenceMessageCloseRequested();
    }

    removeUnreadCandidate(message.get());
    for (int index = m_state.pendingHistoryMentions.size() - 1; index >= 0; --index) {
        if (m_state.pendingHistoryMentions.at(index).messageId == message->getMessageId()) {
            m_state.pendingHistoryMentions.removeAt(index);
        }
    }
    if (chatModel->removeMessage(row) && m_state.loadedMessageCount > 0) {
        --m_state.loadedMessageCount;
    }
    updateHistoryUnreadNotifier();
    updateHistoryMentionNotifier();
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
        friendProfilePopup->close();
        friendProfilePopup->clear();
    }
    chatModel->clear();
    chatModel->clearSelection();
    chatModel->clearRowHighlight();
    chatView->clearTextSelection();
    m_pendingReferenceMessageId.clear();
    m_pendingLocalSends.clear();
    clearPendingMentions();
    ++m_historyMentionScrollGeneration;
    if (referenceMessageNotifier) {
        referenceMessageNotifier->hide();
    }
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
    hideHistoryMentionNotifier();
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
        updateHistoryMentionNotifier();
        m_state.loadingOlderMessages = false;
        if (m_state.pendingHistoryUnreadScroll) {
            m_state.pendingHistoryUnreadScroll = false;
            scrollToFirstHistoryUnread();
        } else if (m_state.pendingHistoryMentionScroll) {
            m_state.pendingHistoryMentionScroll = false;
            scrollToNextHistoryMention();
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
    updateHistoryMentionNotifier();
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
        if (m_state.pendingHistoryMentionScroll) {
            m_state.pendingHistoryMentionScroll = false;
            scrollToNextHistoryMention();
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
        updateHistoryMentionNotifier();
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
    updateHistoryMentionNotifier();
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

QString ChatArea::groupId() const
{
    if (!m_state.meta.isGroup) {
        return {};
    }
    return m_state.meta.groupId.isEmpty()
            ? m_state.meta.conversationId
            : m_state.meta.groupId;
}

QString ChatArea::directPeerUserId() const
{
    if (m_state.meta.isGroup) {
        return {};
    }
    return m_state.meta.peerUserId.isEmpty()
            ? m_state.meta.conversationId
            : m_state.meta.peerUserId;
}

bool ChatArea::isGroupMode() const
{
    return m_state.meta.isGroup;
}

void ChatArea::applyPendingReference(const ChatMessagePtr& message)
{
    if (message.isNull() || m_pendingReferenceMessageId.isEmpty()) {
        return;
    }

    if (message->getMessageId() != m_pendingReferenceMessageId) {
        message->setReferencedMessageId(m_pendingReferenceMessageId);
    }
    m_pendingReferenceMessageId.clear();
    if (referenceMessageNotifier) {
        referenceMessageNotifier->hide();
    }
    updateNewMessageNotifierPosition();
}

bool ChatArea::ensureMessageLoaded(const QString& messageId)
{
    if (messageId.isEmpty() || !chatModel || conversationId().isEmpty()) {
        return false;
    }
    if (chatModel->indexForMessageId(messageId).isValid()) {
        return true;
    }

    if (m_state.hasMoreBefore && !m_state.loadingOlderMessages) {
        m_state.loadingOlderMessages = true;
        const ConversationThreadData olderPage =
                MessageRepository::instance().requestConversationThreadUntilMessage({
                        conversationId(),
                        messageId,
                        m_state.loadedMessageCount
                });
        m_state.loadingOlderMessages = false;

        if (!olderPage.messages.isEmpty()) {
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
            updateHistoryMentionNotifier();
            adjustBottomSpace();

            if (chatModel->indexForMessageId(messageId).isValid()) {
                return true;
            }
        }
    }

    while (m_state.hasMoreBefore && !m_state.loadingOlderMessages) {
        m_state.loadingOlderMessages = true;
        const ConversationThreadData olderPage = MessageRepository::instance().requestConversationThread({
                conversationId(),
                m_state.loadedMessageCount,
                kOlderMessagePageSize
        });
        m_state.loadingOlderMessages = false;
        if (olderPage.messages.isEmpty()) {
            m_state.hasMoreBefore = false;
            break;
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
        updateHistoryMentionNotifier();
        adjustBottomSpace();

        if (chatModel->indexForMessageId(messageId).isValid()) {
            return true;
        }
    }
    return chatModel->indexForMessageId(messageId).isValid();
}

void ChatArea::scrollToMessageAndHighlight(const QString& messageId)
{
    if (!ensureMessageLoaded(messageId) || !chatModel || !chatView) {
        return;
    }

    const QModelIndex messageIndex = chatModel->indexForMessageId(messageId);
    if (!messageIndex.isValid()) {
        return;
    }

    chatModel->clearSelection();
    chatModel->clearRowHighlight();
    chatView->clearTextSelection();
    chatView->scrollToIndexAtTopAnimated(messageIndex, true);

    QPersistentModelIndex persistentIndex(messageIndex);
    QTimer::singleShot(180, this, [this, persistentIndex]() {
        if (!persistentIndex.isValid() || !chatModel) {
            return;
        }
        chatModel->setData(persistentIndex, true, Qt::UserRole + 2);
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
    QTimer::singleShot(1100, this, [this, persistentIndex]() {
        if (!persistentIndex.isValid() || !chatModel) {
            return;
        }
        chatModel->setData(persistentIndex, false, Qt::UserRole + 2);
        if (chatView && chatView->viewport()) {
            chatView->viewport()->update();
        }
    });
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

    if (inputBar && !m_systemFloatingBarsSuppressed) {
        inputBar->show();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
    }
    updateInputBarPosition();
    adjustBottomSpace();

    const int targetLoadingHeight = chatView && chatView->viewport()
            ? chatView->viewport()->height() / 2
            : height() / 2;
    if (chatModel) {
        chatModel->showInitialLoadingPlaceholders(targetLoadingHeight);
    }
    if (messageLoadingAnimationTimer) {
        messageLoadingAnimationTimer->start();
    }

    hideHistoryUnreadNotifier();
    hideHistoryMentionNotifier();
    if (newMessageNotifier) {
        newMessageNotifier->hide();
    }
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
    updateMessageAnimationTimer();
    m_state.hasMoreBefore = conversation.hasMoreBefore;
    m_state.allowOlderMessageFetch = false;
    assignInitialPeerMessageOrdinals(conversation.messages);
    const int loadedUnread = registerHistoryUnreadCandidates(
            conversation.messages,
            m_state.historyUnloadedUnreadMessageCount);
    m_state.historyUnloadedUnreadMessageCount = qMax(
            0,
            m_state.historyUnloadedUnreadMessageCount - loadedUnread);
    if (inputBar && !m_systemFloatingBarsSuppressed) {
        inputBar->show();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
    }
    updateInputBarPosition();
    adjustBottomSpace();
    recalculateHistoryUnreadCount();

    const bool listUpdatesWereEnabled = chatView->updatesEnabled();
    chatView->setUpdatesEnabled(false);
    chatModel->setMessages(conversation.messages);
    updateDirectRelationshipState(false);
    chatView->jumpToBottom();
    chatView->setUpdatesEnabled(listUpdatesWereEnabled);
    if (chatView->viewport()) {
        chatView->viewport()->update();
    }

    setPendingHistoryMentions(conversation.currentUserMentions);
    reconcileHistoryUnreadAfterHistoryExhausted();
    updateHistoryUnreadNotifier();
    updateNewMessageNotifier();
    updateHistoryMentionNotifier();
    m_state.allowOlderMessageFetch = true;
    scheduleVisibleUnreadCheck();
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
    inputBar->setProperty("systemFloatingBarsSuppressedOpacity", suppressed ? 0.0 : QVariant());

    if (m_systemFloatingBarsSuppressed == suppressed) {
        if (suppressed && !inputBar->isHidden()) {
            m_inputBarVisibleBeforeSystemSuppression = true;
            inputBar->refreshPlatformAppearance();
            if (bottomGapGradientOverlay) {
                bottomGapGradientOverlay->hide();
            }
        }
        return;
    }

    m_systemFloatingBarsSuppressed = suppressed;
    if (suppressed) {
        m_inputBarVisibleBeforeSystemSuppression = !inputBar->isHidden();
        inputBar->refreshPlatformAppearance();
        if (bottomGapGradientOverlay) {
            bottomGapGradientOverlay->hide();
        }
        updateReferenceMessageNotifier();
        updateHistoryUnreadNotifierPosition();
        updateNewMessageNotifierPosition();
        return;
    }

    if (m_inputBarVisibleBeforeSystemSuppression && !conversationId().isEmpty()) {
        updateInputBarPosition();
        inputBar->refreshPlatformAppearance();
        inputBar->show();
        inputBar->raise();
        updateHistoryUnreadNotifierPosition();
        updateNewMessageNotifierPosition();
    } else if (!inputBar->isHidden()) {
        updateInputBarPosition();
        inputBar->refreshPlatformAppearance();
        inputBar->raise();
        updateHistoryUnreadNotifierPosition();
    }
    m_inputBarVisibleBeforeSystemSuppression = false;
}
