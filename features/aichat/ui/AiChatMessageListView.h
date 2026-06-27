#pragma once

#include "shared/ui/OverlayScrollListView.h"

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPointer>
#include <QHash>
#include <QString>
#include <QStyleOptionViewItem>

class AiChatMessageDelegate;
class QKeyEvent;
class QMouseEvent;
class QPropertyAnimation;
class QTimer;
class QVariantAnimation;
class QWheelEvent;

class AiChatMessageListView : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit AiChatMessageListView(QWidget* parent = nullptr);
    AiChatMessageDelegate* messageDelegate() const;
    bool isBottomLocked() const;
    void scrollToBottom(bool accelerateFarDistance = false);
    void scrollToBottomIfLocked(bool accelerateFarDistance = false);
    void jumpToBottom();
    void clearTextSelection();
    void setBottomViewportMargin(int margin);
    void refreshMessageLayout(bool keepBottomWhenLocked = true);

signals:
    void regenerateAiReplyRequested(const QString& conversationId, const QString& messageId);
    void traceToggleRequested(const QString& messageId);
    void userScrollUpIntent();
    void userScrollDownIntent();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    int characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const;
    void copySelectionToClipboard();
    void copyBubbleToClipboard(const QModelIndex& index);
    void setCopiedCodeBlock(const QModelIndex& index, int blockRow);
    void clearCopiedCodeBlock();
    void selectAllTextInActiveBubble();
    void showSelectionMenu(const QPoint& globalPos);
    void showBubbleMenu(const QPoint& globalPos, const QModelIndex& index);
    void showUrlMenu(const QPoint& globalPos, const QString& url);
    void openUrl(const QString& url);
    void updateHoveredUserCopyIndex(const QModelIndex& index);
    void animateUserCopyButton(const QPersistentModelIndex& index, qreal targetOpacity);
    void toggleUserMessageExpansion(const QModelIndex& index);
    void refreshAnimatedMessageLayout(const QModelIndex& index);
    void scheduleScrollToBottom(bool force, bool accelerateFarDistance);
    void animateScrollToBottom(bool accelerateFarDistance);
    void setScrollBarToBottom();
    void onScrollValueChanged(int value);
    void unlockBottomLockForUserScrollUp();
    void updateCodeBlockLoadingAnimation();
    void syncCodeBlockLoadingAnimationTimer();
    bool isAtBottom() const;
    bool hasUpwardScrollIntent(const QWheelEvent* event) const;
    bool hasDownwardScrollIntent(const QWheelEvent* event) const;

    AiChatMessageDelegate* m_delegate = nullptr;
    QPropertyAnimation* m_scrollAnimation = nullptr;
    QPersistentModelIndex m_activeBubbleIndex;
    QPersistentModelIndex m_copiedCodeIndex;
    QPersistentModelIndex m_codeBlockLoadingIndex;
    QTimer* m_copyResetTimer = nullptr;
    QTimer* m_codeBlockLoadingTimer = nullptr;
    QPersistentModelIndex m_dragIndex;
    int m_dragAnchor = -1;
    bool m_dragging = false;
    QPersistentModelIndex m_pressedUrlIndex;
    QPersistentModelIndex m_hoveredUserCopyIndex;
    QPoint m_pressedUrlPos;
    QString m_pressedUrl;
    QHash<QString, QPointer<QVariantAnimation>> m_userCopyFadeAnimations;
    QHash<QString, QPointer<QVariantAnimation>> m_userExpansionAnimations;
    bool m_scrollToBottomPending = false;
    bool m_forcePendingScrollToBottom = false;
    bool m_pendingScrollAccelerateFarDistance = false;
    bool m_stickToBottom = true;
    bool m_programmaticScrollChange = false;
    int m_lastScrollValue = 0;

    static constexpr int kBottomReachedThreshold = 2;
};
