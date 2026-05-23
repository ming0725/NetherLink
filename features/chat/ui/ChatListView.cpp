#include "ChatListView.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include "features/chat/ui/ChatItemDelegate.h"
#include "shared/services/ImageService.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/ImageViewer.h"

namespace {

QPoint mouseGlobalPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

constexpr int kAvatarProfilePopupGap = 10;

int scrollAnimationDuration(int distance, int viewportHeight, bool accelerateFarDistance)
{
    if (distance <= 0) {
        return 0;
    }

    constexpr double kMinDurationMs = 200.0;
    constexpr double kMaxDurationMs = 800.0;
    constexpr double kDistanceFactor = 0.5;

    const double normalized = std::min(distance / 1000.0, 1.0);
    const double curved = std::pow(normalized, kDistanceFactor);
    const double duration = kMaxDurationMs * curved;
    const int normalDuration = static_cast<int>(std::clamp(duration, kMinDurationMs, kMaxDurationMs));

    if (!accelerateFarDistance) {
        return normalDuration;
    }

    const int farDistanceThreshold = std::max(viewportHeight * 2, 1000);
    if (distance <= farDistanceThreshold) {
        return normalDuration;
    }

    constexpr double kFastestDurationScale = 0.35;
    const double farProgress = std::clamp(
        static_cast<double>(distance - farDistanceThreshold) /
            static_cast<double>(farDistanceThreshold * 2),
        0.0,
        1.0);
    const double scale = 1.0 - (1.0 - kFastestDurationScale) * farProgress;
    return static_cast<int>(std::clamp(std::round(normalDuration * scale),
                                       kMinDurationMs,
                                       static_cast<double>(normalDuration)));
}

} // namespace

ChatListView::ChatListView(QWidget *parent)
    : OverlayScrollListView(parent)
    , m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this))
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setThemeBackgroundRole(ThemeColor::PageBackground);
    setScrollBarInsets(8, 4);

    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ChatListView::onScrollValueChanged);
    connect(m_scrollAnimation, &QPropertyAnimation::stateChanged,
            this, [this](QAbstractAnimation::State newState,
                         QAbstractAnimation::State oldState) {
                if (oldState == QAbstractAnimation::Running &&
                        newState == QAbstractAnimation::Stopped &&
                        m_programmaticScrollChange) {
                    m_programmaticScrollChange = false;
                    m_lastScrollValue = verticalScrollBar()->value();
                    updateOverlayScrollBar();
                }
            });
}

void ChatListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset,
                   this, &ChatListView::onModelRowsChanged);
    }

    OverlayScrollListView::setModel(model);

    if (!model) {
        return;
    }

    connect(model, &QAbstractItemModel::rowsInserted,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::modelReset,
            this, &ChatListView::onModelRowsChanged);

    QTimer::singleShot(0, this, [this]() { updateOverlayScrollBar(); });
}

void ChatListView::scrollToBottom(bool accelerateFarDistance)
{
    m_stickToBottom = true;
    doItemsLayout();
    updateGeometries();
    if (verticalScrollBar()->value() >= verticalScrollBar()->maximum()) {
        jumpToBottom();
        return;
    }

    animateScrollToValue(verticalScrollBar()->maximum(), accelerateFarDistance, false);
}

void ChatListView::scrollToIndexAtTopAnimated(const QModelIndex& index,
                                              bool accelerateFarDistance)
{
    if (!index.isValid()) {
        return;
    }

    m_stickToBottom = false;
    doItemsLayout();
    updateGeometries();

    QScrollBar* scrollBar = verticalScrollBar();
    const int startValue = scrollBar->value();

    m_programmaticScrollChange = true;
    QListView::scrollTo(index, QAbstractItemView::PositionAtTop);
    const int targetValue = scrollBar->value();
    scrollBar->setValue(startValue);
    m_programmaticScrollChange = false;
    m_lastScrollValue = startValue;

    animateScrollToValue(targetValue, accelerateFarDistance, true);
}

bool ChatListView::scrollToBottomIfLocked(bool accelerateFarDistance)
{
    if (!m_stickToBottom) {
        return false;
    }

    scrollToBottom(accelerateFarDistance);
    return true;
}

void ChatListView::jumpToBottom()
{
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    setScrollBarToBottom();
}

void ChatListView::preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum)
{
    m_scrollAnimation->stop();
#ifdef Q_OS_WIN
    stopAnimatedWheelScroll();
#endif
    QTimer::singleShot(0, this, [this, previousValue, previousMaximum]() {
        doItemsLayout();
        updateGeometries();
        QScrollBar* scrollBar = verticalScrollBar();
        const int addedHeight = qMax(0, scrollBar->maximum() - previousMaximum);
        m_programmaticScrollChange = true;
        scrollBar->setValue(qBound(scrollBar->minimum(),
                                   previousValue + addedHeight,
                                   scrollBar->maximum()));
        m_programmaticScrollChange = false;
        m_lastScrollValue = scrollBar->value();
#ifdef Q_OS_WIN
        stopAnimatedWheelScroll();
#endif
        updateOverlayScrollBar();
    });
}

void ChatListView::clearTextSelection()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || (!delegate->hasSelection() && !delegate->selectionIndex().isValid())) {
        return;
    }

    delegate->clearSelection();
    viewport()->update();
}

void ChatListView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_PageUp:
    case Qt::Key_Home:
        emit userScrollUpIntent();
        unlockBottomLockForUserScrollUp();
        break;
    case Qt::Key_Down:
    case Qt::Key_PageDown:
    case Qt::Key_End:
        emit userScrollDownIntent();
        break;
    default:
        break;
    }

    if (event->matches(QKeySequence::SelectAll)) {
        selectAllTextInActiveBubble();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        if (ChatItemDelegate* delegate = chatDelegate();
                delegate && delegate->hasSelection()) {
            copySelectionToClipboard();
            event->accept();
            return;
        }
    }

    OverlayScrollListView::keyPressEvent(event);
}

void ChatListView::wheelEvent(QWheelEvent* event)
{
    if (hasUpwardScrollIntent(event)) {
        emit userScrollUpIntent();
        unlockBottomLockForUserScrollUp();
    } else if (hasDownwardScrollIntent(event)) {
        emit userScrollDownIntent();
    }

    OverlayScrollListView::wheelEvent(event);
}

void ChatListView::resizeEvent(QResizeEvent* event)
{
    const bool shouldKeepBottom = m_stickToBottom;

    OverlayScrollListView::resizeEvent(event);
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    if (shouldKeepBottom) {
        setScrollBarToBottom();
    }
    updateOverlayScrollBar();
}

void ChatListView::mousePressEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();

    if (delegate && event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            if (delegate->avatarHitTest(option, index, event->pos())) {
                const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
                if (message && !message->getSenderId().isEmpty()) {
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    clearTextSelection();
                    emit avatarContextMenuRequested(message->getSenderId(), mouseGlobalPosition(event));
                    event->accept();
                    return;
                }
            }

            const QString url = delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                showUrlMenu(mouseGlobalPosition(event), url);
                event->accept();
                return;
            }

            const int cursor = delegate->characterIndexAt(option, index, event->pos());
            if (delegate->selectionContains(index, cursor)) {
                showSelectionMenu(mouseGlobalPosition(event));
                event->accept();
                return;
            }
        }

        clearTextSelection();
    }

    if (delegate && event->button() == Qt::LeftButton) {
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            if (delegate->avatarHitTest(option, index, event->pos())) {
                const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
                if (message && !message->getSenderId().isEmpty()) {
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    clearTextSelection();
                    const QRect avatarRect = delegate->avatarRectForIndex(option, index);
                    const QPoint anchor = message->isFromMe()
                            ? viewport()->mapToGlobal(avatarRect.topLeft() - QPoint(kAvatarProfilePopupGap, 0))
                            : viewport()->mapToGlobal(avatarRect.topRight() + QPoint(kAvatarProfilePopupGap + 1, 0));
                    emit avatarClicked(message->getSenderId(), anchor);
                    event->accept();
                    return;
                }
            }

            const bool hitBubble = delegate->bubbleHitTest(option, index, event->pos());
            if (delegate->triggerReeditIfHit(option, index, event->pos())) {
                clearTextSelection();
                event->accept();
                return;
            }
            const QString imageSource = delegate->imageSourceAt(option, index, event->pos());
            if (!imageSource.isEmpty()) {
                const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
                if (message && message->getIsSelected()) {
                    openImageViewer(imageSource);
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    clearTextSelection();
                    event->accept();
                    return;
                }
            }
            if (hitBubble) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);
            }

            const QString url = delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                if (model()) {
                    static_cast<ChatListModel*>(model())->clearSelection();
                }
                m_pressedUrlIndex = QPersistentModelIndex(index);
                m_pressedUrlPos = event->pos();
                m_pressedUrl = url;
                event->accept();
                return;
            }

            const int cursor = delegate->characterIndexAt(option, index, event->pos());
            if (cursor >= 0) {
                if (model()) {
                    static_cast<ChatListModel*>(model())->clearSelection();
                }
                m_dragging = true;
                m_dragIndex = QPersistentModelIndex(index);
                m_dragAnchor = cursor;
                delegate->setSelection(index, m_dragAnchor, cursor);
                viewport()->update();
                event->accept();
                return;
            }

            if (hitBubble) {
                clearTextSelection();
                if (model()) {
                    model()->setData(index, true, Qt::UserRole + 1);
                }
                event->accept();
                return;
            }
        }

        m_activeBubbleIndex = QPersistentModelIndex();
        clearTextSelection();
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() && model()) {
        static_cast<ChatListModel*>(model())->clearSelection();
    }
    OverlayScrollListView::mousePressEvent(event);
}

void ChatListView::mouseDoubleClickEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            if (delegate->bubbleHitTest(option, index, event->pos())) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);

                const QString imageSource = delegate->imageSourceAt(option, index, event->pos());
                if (!imageSource.isEmpty()) {
                    openImageViewer(imageSource);
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    clearTextSelection();
                    event->accept();
                    return;
                }

                if (delegate->urlAt(option, index, event->pos()).isEmpty() &&
                        delegate->selectWordAt(option, index, event->pos())) {
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    viewport()->update();
                    event->accept();
                    return;
                }
            }
        }
    }

    OverlayScrollListView::mouseDoubleClickEvent(event);
}

void ChatListView::mouseMoveEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && m_dragging && m_dragIndex.isValid()) {
        const int cursor = characterIndexForDrag(m_dragIndex, event->pos());
        if (cursor >= 0) {
            delegate->setSelection(m_dragIndex, m_dragAnchor, cursor);
            viewport()->update();
        }
        event->accept();
        return;
    }

    if (!m_pressedUrl.isEmpty()) {
        if ((event->pos() - m_pressedUrlPos).manhattanLength() >
                QApplication::startDragDistance()) {
            m_pressedUrlIndex = QPersistentModelIndex();
            m_pressedUrl.clear();
        }
        event->accept();
        return;
    }

    bool overUrl = false;
    bool overText = false;
    if (delegate) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            overUrl = !delegate->urlAt(option, index, event->pos()).isEmpty();
            if (!overUrl && delegate->reeditHitTest(option, index, event->pos())) {
                viewport()->setCursor(Qt::PointingHandCursor);
                OverlayScrollListView::mouseMoveEvent(event);
                return;
            }
            overText = overUrl || delegate->characterIndexAt(option, index, event->pos()) >= 0;
        }
    }
    viewport()->setCursor(overUrl ? Qt::PointingHandCursor
                                  : (overText ? Qt::IBeamCursor : Qt::ArrowCursor));

    OverlayScrollListView::mouseMoveEvent(event);
}

void ChatListView::mouseReleaseEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && event->button() == Qt::LeftButton && !m_pressedUrl.isEmpty()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && m_pressedUrlIndex == index) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString releaseUrl = delegate->urlAt(option, index, event->pos());
            if (releaseUrl == m_pressedUrl) {
                openUrl(m_pressedUrl);
            }
        }

        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        viewport()->update();
        event->accept();
        return;
    }

    OverlayScrollListView::mouseReleaseEvent(event);
}

void ChatListView::leaveEvent(QEvent* event)
{
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

void ChatListView::onModelRowsChanged()
{
    QTimer::singleShot(0, this, [this]() {
        doItemsLayout();
        updateGeometries();
        updateOverlayScrollBar();
    });
}

void ChatListView::onScrollValueChanged(int value)
{
    if (!m_programmaticScrollChange && value < m_lastScrollValue && !isAtBottom()) {
        m_stickToBottom = false;
        if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
        }
    }

    m_lastScrollValue = value;
    if (isAtBottom()) {
        m_stickToBottom = true;
    }
}

void ChatListView::animateScrollToValue(int targetValue,
                                        bool accelerateFarDistance,
                                        bool programmaticUpwardScroll)
{
    doItemsLayout();
    updateGeometries();

    QScrollBar* scrollBar = verticalScrollBar();
    targetValue = qBound(scrollBar->minimum(), targetValue, scrollBar->maximum());
    const int startValue = scrollBar->value();
    const int distance = qAbs(targetValue - startValue);

    if (distance <= 0) {
        m_scrollAnimation->stop();
        m_programmaticScrollChange = false;
        m_lastScrollValue = startValue;
        updateOverlayScrollBar();
        return;
    }

    m_scrollAnimation->stop();
    m_programmaticScrollChange = programmaticUpwardScroll;
    const int duration = scrollAnimationDuration(distance,
                                                 viewport()->height(),
                                                 accelerateFarDistance);
    m_scrollAnimation->setDuration(duration);
    m_scrollAnimation->setStartValue(startValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();
}

void ChatListView::setScrollBarToBottom()
{
    QScrollBar* scrollBar = verticalScrollBar();
    m_programmaticScrollChange = true;
    scrollBar->setValue(scrollBar->maximum());
    m_programmaticScrollChange = false;
    m_lastScrollValue = scrollBar->value();
    m_stickToBottom = true;
    updateOverlayScrollBar();
}

void ChatListView::unlockBottomLockForUserScrollUp()
{
    if (verticalScrollBar()->value() > verticalScrollBar()->minimum()) {
        m_stickToBottom = false;
        if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
        }
    }
}

bool ChatListView::isAtBottom() const
{
    const QScrollBar* scrollBar = verticalScrollBar();
    return scrollBar->maximum() - scrollBar->value() <= kBottomReachedThreshold;
}

bool ChatListView::hasUpwardScrollIntent(const QWheelEvent* event) const
{
    if (!event->pixelDelta().isNull()) {
        return event->pixelDelta().y() > 0;
    }

    return event->angleDelta().y() > 0;
}

bool ChatListView::hasDownwardScrollIntent(const QWheelEvent* event) const
{
    if (!event->pixelDelta().isNull()) {
        return event->pixelDelta().y() < 0;
    }

    return event->angleDelta().y() < 0;
}

ChatItemDelegate* ChatListView::chatDelegate() const
{
    return qobject_cast<ChatItemDelegate*>(itemDelegate());
}

QStyleOptionViewItem ChatListView::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

int ChatListView::characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || !index.isValid()) {
        return -1;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);
    const int cursor = delegate->characterIndexAt(option, index, pos, true);
    if (cursor >= 0) {
        return cursor;
    }

    const QRect itemRect = visualRect(index);
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return -1;
    }

    if (pos.y() <= itemRect.top()) {
        return 0;
    }
    if (pos.y() >= itemRect.bottom()) {
        return message->getContent().size();
    }
    return -1;
}

void ChatListView::copySelectionToClipboard()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate) {
        return;
    }

    const QString selectedText = delegate->selectedText();
    if (selectedText.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(selectedText);
}

void ChatListView::selectAllTextInActiveBubble()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || !m_activeBubbleIndex.isValid()) {
        return;
    }

    const ChatMessage* message = m_activeBubbleIndex.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text || message->getContent().isEmpty()) {
        return;
    }

    if (model()) {
        static_cast<ChatListModel*>(model())->clearSelection();
    }
    delegate->setSelection(m_activeBubbleIndex, 0, message->getContent().size());
    viewport()->update();
}

void ChatListView::showSelectionMenu(const QPoint& globalPos)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        copySelectionToClipboard();
    });

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void ChatListView::showUrlMenu(const QPoint& globalPos, const QString& url)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [url]() {
        QApplication::clipboard()->setText(url);
    });

    QAction* openAction = menu->addAction(QStringLiteral("用默认浏览器打开"));
    connect(openAction, &QAction::triggered, this, [this, url]() {
        openUrl(url);
    });

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void ChatListView::openUrl(const QString& url)
{
    const QUrl resolvedUrl = QUrl::fromUserInput(url);
    if (!resolvedUrl.isValid()) {
        return;
    }

    QDesktopServices::openUrl(resolvedUrl);
}

void ChatListView::openImageViewer(const QString& imageSource)
{
    if (imageSource.isEmpty() || !ImageService::instance().sourceSize(imageSource).isValid()) {
        return;
    }

    auto* viewer = new ImageViewer(imageSource, window());
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}
