#ifndef CHATAREA_H
#define CHATAREA_H

#include <QWidget>
#include <QSharedPointer>
#include <QDateTime>
#include <QHash>
#include <QPersistentModelIndex>
#include <QPointer>
#include <optional>
#include "ChatListView.h"
#include "features/chat/model/ChatListModel.h"
#include "ChatItemDelegate.h"
#include "shared/types/ChatMessage.h"
#include "HistoryUnreadNotifier.h"
#include "NewMessageNotifier.h"
#include "ReferenceMessageNotifier.h"
#include "shared/ui/FloatingInputBar.h"
#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"

class QPropertyAnimation;
class QPushButton;
class QLabel;
class QTimer;
class ChatSessionController;
class DirectConversationInfoPanel;
class FriendProfilePopup;
class FriendSessionController;
class GroupConversationInfoPanel;
class InWindowPopupOverlay;
class PaintedLabel;

class ChatArea : public QWidget
{
    Q_OBJECT
    using ChatMessagePtr = QSharedPointer<ChatMessage>;
public:
    explicit ChatArea(QWidget *parent = nullptr);
    void showConversationLoading(const ConversationMeta& meta);
    void openConversation(const ConversationThreadData& conversation);
    void addMessage(ChatMessagePtr message);
    void addImageMessage(QSharedPointer<ImageMessage> message,
                         const QDateTime& timestamp = QDateTime::currentDateTime());
    void addTextMessage(QSharedPointer<TextMessage> message,
                        const QDateTime& timestamp = QDateTime::currentDateTime());
    void closeConversation();
    void clearMessageSelection();
    void handleGlobalMousePress(const QPoint& globalPos);
    void setSystemFloatingBarsSuppressed(bool suppressed);

signals:
    void currentConversationRemoved();
    void requestOpenConversation(const QString& conversationId);

protected:
    void resizeEvent(QResizeEvent *event) override;
private slots:
    void onScrollValueChanged(int value);
    void onNewMessageNotifierClicked();
    void onSendImage(const QString &path);
    void onSendText(const QString &text);
    void onDeleteMessageRequested(int row);
    void onRecallMessageRequested(int row);
    void onReferenceMessageRequested(int row);
    void onReferenceMessageCloseRequested();
    void onReferencedMessageClicked(const QString& messageId);
    void onReeditMessageRequested(int row);
    void onRetryMessageRequested(int row);
    void onInfoButtonClicked();
    void confirmClearChatHistory();
    void confirmDeleteFriend();
    void confirmExitGroup();
    void showCurrentUserEditProfilePopup();

private:
    struct ConversationState {
        ConversationMeta meta;
        int historyUnreadMessageCount = 0;
        int historyUnloadedUnreadMessageCount = 0;
        int newUnreadMessageCount = 0;
        int loadedMessageCount = 0;
        bool isAtBottom = true;
        bool hasMoreBefore = false;
        bool loadingOlderMessages = false;
        bool loadingInitialMessages = false;
        bool allowOlderMessageFetch = false;
#ifdef Q_OS_WIN
        qint64 olderMessageTriggerCooldownUntilMs = 0;
#endif
        bool visibleUnreadCheckScheduled = false;
        bool pendingHistoryUnreadScroll = false;
        bool newMessageNotifierRevealedByDownScroll = false;
        bool hasHistoryUnreadOrdinalRange = false;
        int historyUnreadFirstOrdinal = 0;
        int historyUnreadLastOrdinal = -1;
        int historyReadMinOrdinal = 0;
        bool hasNewUnreadOrdinalRange = false;
        int newUnreadFirstOrdinal = 0;
        int newUnreadLastOrdinal = -1;
        int newReadMaxOrdinal = -1;
        QHash<const ChatMessage*, int> peerMessageOrdinals;
        int minPeerMessageOrdinal = 0;
        int maxPeerMessageOrdinal = -1;
        bool directRelationshipDeleted = false;
    };

    struct PendingLocalSend {
        QString conversationId;
        QString clientMessageId;
        QString text;
        QString imagePath;
        QString referencedMessageId;
        bool isImage = false;
        int attempt = 0;
    };

    ChatListView* chatView;
    ChatListModel* chatModel;
    ChatItemDelegate* chatDelegate;
    HistoryUnreadNotifier* historyUnreadNotifier;
    QTimer* historyUnreadNotifierLoadTimer = nullptr;
    QTimer* messageLoadingAnimationTimer = nullptr;
    NewMessageNotifier* newMessageNotifier;
    ReferenceMessageNotifier* referenceMessageNotifier;
    QWidget* bottomGapGradientOverlay;
    FloatingInputBar* inputBar;
    QLabel* statusIcon;
    PaintedLabel* nameLabel;
    QPushButton* infoButton;
    GroupConversationInfoPanel* groupInfoPanel = nullptr;
    DirectConversationInfoPanel* directInfoPanel = nullptr;
    ChatSessionController* sessionController;
    FriendSessionController* friendProfileController = nullptr;
    FriendProfilePopup* friendProfilePopup = nullptr;
    QPointer<InWindowPopupOverlay> currentUserEditProfilePopup;
    QPropertyAnimation* infoPanelAnimation;
    bool infoPanelOpen = false;
    bool m_systemFloatingBarsSuppressed = false;
    bool m_inputBarVisibleBeforeSystemSuppression = false;
    ConversationState m_state;
    
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    bool shouldShowNewMessageNotifier() const;
    void updateReferenceMessageNotifierPosition();
    void updateReferenceMessageNotifier();
    void updateHistoryUnreadNotifier();
    void showHistoryUnreadNotifier();
    void hideHistoryUnreadNotifier();
    void updateHistoryUnreadNotifierPosition();
    void scrollToBottom(bool accelerateFarDistance = false);
    void scrollToFirstHistoryUnread();
    bool isScrollAtBottom() const;
    int registerHistoryUnreadCandidates(const ChatMessageList& messages, int maxCount);
    bool registerNewUnreadCandidate(const ChatMessagePtr& message);
    void scheduleVisibleUnreadCheck();
    void updateVisibleUnreadMessages();
    QRect effectiveMessageViewportRect() const;
    std::optional<int> firstVisiblePeerOrdinal() const;
    std::optional<int> lastVisiblePeerOrdinal() const;
    std::optional<int> peerOrdinalForRow(int row) const;
    int historyUnreadJumpRow(int unreadCount) const;
    void assignInitialPeerMessageOrdinals(const ChatMessageList& messages);
    void assignPrependedPeerMessageOrdinals(const ChatMessageList& messages);
    void assignAppendedPeerMessageOrdinal(const ChatMessagePtr& message);
    void extendHistoryUnreadRange(int firstOrdinal, int lastOrdinal);
    void recalculateHistoryUnreadCount();
    void recalculateNewUnreadCount();
    void appendRepositoryMessage(const QString& changedConversationId,
                                 const ChatMessagePtr& message);
    void replaceRepositoryMessage(const QString& changedConversationId,
                                  const ChatMessagePtr& message);
    void refreshCurrentGroupMessageDisplayNames();
    void reconcileHistoryUnreadAfterHistoryExhausted();
    void adjustBottomSpace();
    void updateInputBarPosition();
    void applyConversationMeta();
    void clearConversation(bool closeInfoPanel = true);
    void loadOlderMessages();
    bool loadHistoryUnreadMessages(int requestedMessageCount);
    QString conversationId() const;
    QString groupId() const;
    QString directPeerUserId() const;
    bool isGroupMode() const;
    bool ensureMessageLoaded(const QString& messageId);
    void scrollToMessageAndHighlight(const QString& messageId);
    void applyPendingReference(const ChatMessagePtr& message);
    void registerPendingLocalSend(const PendingLocalSend& pending,
                                  const ChatMessagePtr& message);
    void schedulePendingLocalSendTimeout(const QString& clientMessageId);
    void setLocalSendState(const QString& clientMessageId,
                           MessageSendState state);
    void markLocalSendSucceeded(const QString& clientMessageId);
    void markLocalSendFailed(const QString& clientMessageId);
    void failPendingLocalSendsForCurrentConversation();
    bool isDirectRelationshipUnavailable() const;
    QString directRelationshipDeletedNoticeMessageId() const;
    void updateDirectRelationshipState(bool scrollToNotice = false);
    void appendDirectRelationshipDeletedNotice(bool scrollToNotice);
    void removeDirectRelationshipDeletedNotice();
    void updateMessageAnimationTimer();
    QWidget* activeInfoPanel() const;
    QWidget* inactiveInfoPanel() const;
    QWidget* ensureActiveInfoPanel();
    void connectGroupInfoPanel(GroupConversationInfoPanel* panel);
    void connectDirectInfoPanel(DirectConversationInfoPanel* panel);
    void releaseInfoPanels();
    void showFriendProfilePopup(const QString& userId, const QPoint& globalPos);
    void showAvatarContextMenu(const QString& userId, const QPoint& globalPos);
    void requestInfoPanelData(bool resetTransientState);
    int visibleInfoPanelWidth() const;
    void showInfoPanel(bool animated);
    void hideInfoPanel(bool animated);
    void updateInfoPanelGeometry();
    QRect infoPanelOpenGeometry() const;
    QRect infoPanelClosedGeometry() const;
    bool containsGlobalPoint(QWidget* widget, const QPoint& globalPos) const;
    void updateGroupInfoPanelState();
    void updateDirectInfoPanelState(bool animated);
    void onSessionChanged(const ConversationMeta& meta);
    void onDirectPanelDataLoaded(const ConversationMeta& meta, const User& directUser);
    void onGroupPanelDataLoaded(const ConversationMeta& meta,
                                const Group& group,
                                const QVector<GroupMemberProfile>& previewMembers,
                                int totalMembers,
                                bool canEditGroupInfo,
                                bool canExitGroup);
    void onGroupMembersPageLoaded(const ConversationMeta& meta, const GroupMembersPage& page);
    void onSessionMessagesCleared();
    void onSessionConversationRemoved();
    bool canRecallMessage(const ChatMessage* message) const;
    void recallMessageAtRow(int row,
                            const QString& actorId = QString(),
                            const QString& actorName = QString(),
                            GroupRole actorRole = GroupRole::Member,
                            bool force = false);
    QSharedPointer<RecallMessage> createRecallMessage(const QSharedPointer<ChatMessage>& message,
                                                      const QString& actorId,
                                                      const QString& actorName,
                                                      GroupRole actorRole,
                                                      bool moderatorRecall) const;
    void scheduleReeditExpiry(const QSharedPointer<RecallMessage>& message);
    void removeUnreadCandidate(const ChatMessage* message);
    QString m_pendingReferenceMessageId;
    QHash<QString, PendingLocalSend> m_pendingLocalSends;
};

#endif // CHATAREA_H 
