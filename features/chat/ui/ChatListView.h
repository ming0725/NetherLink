#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPropertyAnimation>
#include <QStyleOptionViewItem>

#include "shared/ui/OverlayScrollListView.h"
#include "features/chat/model/ChatListModel.h"

class ChatItemDelegate;
class QKeyEvent;
class QWheelEvent;

class ChatListView : public OverlayScrollListView
{
    Q_OBJECT
public:
    explicit ChatListView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    void scrollToBottom(bool accelerateFarDistance = false);
    void scrollToIndexAtTopAnimated(const QModelIndex& index,
                                    bool accelerateFarDistance = false);
    bool scrollToBottomIfLocked(bool accelerateFarDistance = false);
    void jumpToBottom();
    bool isBottomLocked() const { return m_stickToBottom; }
    void preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum);
    void clearTextSelection();

signals:
    void userScrollUpIntent();
    void userScrollDownIntent();
    void avatarClicked(const QString& userId, const QPoint& globalPos);
    void avatarContextMenuRequested(const QString& userId, const QPoint& globalPos);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onModelRowsChanged();
    void onScrollValueChanged(int value);

private:
    ChatItemDelegate* chatDelegate() const;
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    int characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const;
    void copySelectionToClipboard();
    void selectAllTextInActiveBubble();
    void showSelectionMenu(const QPoint& globalPos);
    void showUrlMenu(const QPoint& globalPos, const QString& url);
    void openUrl(const QString& url);
    void openImageViewer(const QString& imageSource);
    void animateScrollToValue(int targetValue,
                              bool accelerateFarDistance,
                              bool programmaticUpwardScroll);
    void setScrollBarToBottom();
    void unlockBottomLockForUserScrollUp();
    bool isAtBottom() const;
    bool hasUpwardScrollIntent(const QWheelEvent* event) const;
    bool hasDownwardScrollIntent(const QWheelEvent* event) const;

    QPropertyAnimation* m_scrollAnimation;
    QPersistentModelIndex m_activeBubbleIndex;
    QPersistentModelIndex m_dragIndex;
    int m_dragAnchor = -1;
    bool m_dragging = false;
    QPersistentModelIndex m_pressedUrlIndex;
    QPoint m_pressedUrlPos;
    QString m_pressedUrl;
    bool m_stickToBottom = true;
    bool m_programmaticScrollChange = false;
    int m_lastScrollValue = 0;

    static constexpr int kBottomReachedThreshold = 5;
};

#endif // CHATLISTVIEW_H 
