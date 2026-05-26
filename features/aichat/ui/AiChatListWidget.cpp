#include "AiChatListWidget.h"
#include "shared/services/AppFonts.h"

#include <QApplication>
#include <QAction>
#include <QCursor>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>

#include "AiChatListDelegate.h"
#include "features/aichat/model/AiChatListModel.h"
#include "features/aichat/ui/AiChatSessionController.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/InWindowPopupOverlay.h"
#include "shared/ui/StyledActionMenu.h"

namespace {

QPoint mouseGlobalPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

} // namespace

AiChatListWidget::AiChatListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new AiChatListModel(this))
    , m_delegate(new AiChatListDelegate(this))
    , m_streamingSpinnerTimer(new QTimer(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setThemeBackgroundRole(ThemeColor::PageBackground);
    refreshTheme();
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AiChatListWidget::onCurrentChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateStickyHeader();
        if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 160) {
            loadMoreEntries();
        }
    });
    connect(m_model, &QAbstractItemModel::modelReset,
            this, [this]() { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                updateStickyHeader();
            });
    m_streamingSpinnerTimer->setInterval(16);
    connect(m_streamingSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_spinnerProgress += 0.018;
        if (m_spinnerProgress >= 1.0) {
            m_spinnerProgress -= 1.0;
        }
        m_delegate->setSpinnerProgress(m_spinnerProgress);
        const int row = m_model->rowOfConversation(m_delegate->streamingConversationId());
        if (row >= 0) {
            viewport()->update(visualRect(m_model->index(row, 0)));
        } else {
            viewport()->update();
        }
    });
    updateStickyHeader();
}

void AiChatListWidget::setController(AiChatSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;
    if (!m_controller) {
        m_model->setEntries({});
        setStreamingConversationId(QString());
        m_initialized = false;
        return;
    }

    connect(m_controller, &AiChatSessionController::conversationsChanged, this, [this]() {
        if (!m_initialized) {
            return;
        }

        const QString selectedConversationId = currentIndex().data(AiChatListModel::ConversationIdRole).toString();
        reloadEntries(selectedConversationId);
    });
    connect(m_controller, &AiChatSessionController::conversationsLoaded,
            this, &AiChatListWidget::onEntriesLoaded);
    connect(m_controller, &AiChatSessionController::aiReplyStarted,
            this, &AiChatListWidget::setStreamingConversationId);
    connect(m_controller, &AiChatSessionController::aiReplyFinished, this,
            [this](const QString& conversationId, const QString&) {
                if (m_delegate->streamingConversationId() == conversationId) {
                    m_model->setConversationUnreadDot(conversationId, true);
                    setStreamingConversationId(QString());
                }
            });
    connect(m_controller, &AiChatSessionController::aiReplyCanceled, this,
            [this](const QString& conversationId, const QString&) {
                if (m_delegate->streamingConversationId() == conversationId) {
                    setStreamingConversationId(QString());
                }
            });
    setStreamingConversationId(m_controller->activeStreamConversationId());
}

void AiChatListWidget::ensureInitialized()
{
    if (m_initialized) {
        return;
    }

    m_initialized = true;
    QTimer::singleShot(0, this, [this]() {
        if (!m_initialized || !m_controller) {
            return;
        }
        reloadEntries();
        updateStickyHeader();
    });
}

void AiChatListWidget::createNewConversation()
{
    m_initialized = true;
    clearSelection();
    if (selectionModel()) {
        selectionModel()->clearCurrentIndex();
    }
    setCurrentIndex(QModelIndex());
    updateStickyHeader();
    emit conversationCleared();
}

void AiChatListWidget::selectConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    m_initialized = true;
    reloadEntries(conversationId);
    updateStickyHeader();
}

void AiChatListWidget::reloadEntries(const QString& selectedConversationId)
{
    m_nextOffset = 0;
    m_hasMore = true;
    m_loadingEntries = false;
    m_selectFirstAfterLoad = false;
    m_pendingSelectedConversationId = selectedConversationId;
    loadMoreEntries();
}

void AiChatListWidget::loadMoreEntries()
{
    if (!m_controller || !m_hasMore || m_loadingEntries) {
        return;
    }

    const AiChatListRequest query {
            m_nextOffset,
            kPageSize + 1
    };
    m_loadingEntries = true;
    m_pendingEntriesRequestId = m_controller->loadConversationsAsync(query);
}

void AiChatListWidget::onEntriesLoaded(int requestId,
                                       const AiChatListRequest& query,
                                       const QVector<AiChatListEntry>& loadedEntries)
{
    if (requestId != m_pendingEntriesRequestId) {
        return;
    }

    m_loadingEntries = false;
    QVector<AiChatListEntry> entries = loadedEntries;
    const bool hasMore = entries.size() > kPageSize;
    if (hasMore) {
        entries.resize(kPageSize);
    }
    const int addedCount = entries.size();

    if (query.offset == 0) {
        m_model->setEntries(std::move(entries));
    } else {
        m_model->appendEntries(entries);
    }

    m_nextOffset += addedCount;
    m_hasMore = hasMore;
    updateStickyHeader();

    if (!m_pendingSelectedConversationId.isEmpty() &&
            m_model->rowOfConversation(m_pendingSelectedConversationId) < 0 &&
            m_hasMore) {
        loadMoreEntries();
        return;
    }

    finishPendingSelection();
}

void AiChatListWidget::finishPendingSelection()
{
    if (m_selectFirstAfterLoad) {
        m_selectFirstAfterLoad = false;
        if (m_model->rowCount() > 0 && selectionModel()) {
            selectionModel()->setCurrentIndex(m_model->index(0, 0),
                                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            clearSelection();
            if (selectionModel()) {
                selectionModel()->clearCurrentIndex();
            }
            setCurrentIndex(QModelIndex());
            emit conversationCleared();
        }
        return;
    }

    if (m_pendingSelectedConversationId.isEmpty()) {
        return;
    }

    const QString selectedConversationId = m_pendingSelectedConversationId;
    m_pendingSelectedConversationId.clear();
    const int row = m_model->rowOfConversation(selectedConversationId);
    if (row < 0) {
        clearSelection();
        if (selectionModel()) {
            selectionModel()->clearCurrentIndex();
        }
        setCurrentIndex(QModelIndex());
        emit conversationCleared();
        return;
    }

    selectionModel()->setCurrentIndex(m_model->index(row, 0),
                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void AiChatListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            QStyleOptionViewItem option = viewOptionForIndex(index);
            const bool hasUnreadDot = index.data(AiChatListModel::HasUnreadDotRole).toBool();
            if (!isStreamingConversation(index) &&
                    !hasUnreadDot &&
                    m_delegate->moreButtonRect(option, index).contains(event->pos())) {
                showItemMenu(index, viewport()->mapToGlobal(m_delegate->moreButtonRect(option, index).bottomLeft()));
                event->accept();
                return;
            }
        }
    }

    if (event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            if (selectionModel()) {
                selectionModel()->setCurrentIndex(index,
                                                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            showItemMenu(index, mouseGlobalPosition(event));
            event->accept();
            return;
        }
    }

    OverlayScrollListView::mousePressEvent(event);
}

void AiChatListWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    bool overMoreButton = false;
    if (index.isValid()) {
        const QStyleOptionViewItem option = viewOptionForIndex(index);
        const bool hasUnreadDot = index.data(AiChatListModel::HasUnreadDotRole).toBool();
        overMoreButton = !isStreamingConversation(index) &&
                !hasUnreadDot &&
                m_delegate->moreButtonRect(option, index).contains(event->pos());
    }

    viewport()->setCursor(overMoreButton ? Qt::PointingHandCursor : Qt::ArrowCursor);
    OverlayScrollListView::mouseMoveEvent(event);
    viewport()->update();
}

void AiChatListWidget::leaveEvent(QEvent* event)
{
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

void AiChatListWidget::paintEvent(QPaintEvent* event)
{
    OverlayScrollListView::paintEvent(event);
    drawStickyHeader();
}

void AiChatListWidget::showEvent(QShowEvent* event)
{
    OverlayScrollListView::showEvent(event);
    ensureInitialized();
}

void AiChatListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if (!current.isValid()) {
        emit conversationCleared();
        return;
    }

    const AiChatListEntry entry = m_model->entryAt(current);
    if (entry.conversationId.isEmpty()) {
        emit conversationCleared();
        return;
    }

    emit conversationActivated(entry);
}

void AiChatListWidget::updateStickyHeader()
{
    const StickyHeaderState state = calculateStickyHeaderState();
    m_stickyVisible = state.visible;
    m_stickyOffsetY = state.offsetY;

    if (!m_stickyVisible) {
        m_stickyTitle.clear();
        viewport()->update();
        return;
    }

    m_stickyTitle = state.title;

    viewport()->update();
}

AiChatListWidget::StickyHeaderState AiChatListWidget::calculateStickyHeaderState() const
{
    StickyHeaderState state;
    if (m_model->rowCount() <= 0 || verticalScrollBar()->value() <= 0) {
        return state;
    }

    QModelIndex firstVisible = indexAt(QPoint(1, 1));
    if (!firstVisible.isValid()) {
        firstVisible = m_model->index(0, 0);
    }
    if (!firstVisible.isValid()) {
        return state;
    }

    const int currentSectionRow = m_model->sectionStartRowForRow(firstVisible.row());
    if (currentSectionRow < 0) {
        return state;
    }

    state.visible = true;
    state.title = m_model->sectionTitleAt(currentSectionRow);

    const int nextSectionRow = m_model->nextSectionStartRow(currentSectionRow);
    if (nextSectionRow >= 0) {
        const QRect nextRect = visualRect(m_model->index(nextSectionRow, 0));
        if (nextRect.isValid()) {
            state.offsetY = qMin(0, nextRect.top() - m_delegate->stickyHeaderHeight());
        }
    }

    return state;
}

QStyleOptionViewItem AiChatListWidget::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    if (selectionModel() && selectionModel()->currentIndex() == index) {
        option.state |= QStyle::State_Selected;
    }
    if (viewport()->rect().contains(viewport()->mapFromGlobal(QCursor::pos()))
        && indexAt(viewport()->mapFromGlobal(QCursor::pos())) == index) {
        option.state |= QStyle::State_MouseOver;
    }
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

void AiChatListWidget::setStreamingConversationId(const QString& conversationId)
{
    if (m_delegate->streamingConversationId() == conversationId) {
        return;
    }

    const QString previousConversationId = m_delegate->streamingConversationId();
    m_delegate->setStreamingConversationId(conversationId);
    m_spinnerProgress = 0.0;
    m_delegate->setSpinnerProgress(m_spinnerProgress);

    if (conversationId.isEmpty()) {
        m_streamingSpinnerTimer->stop();
    } else if (!m_streamingSpinnerTimer->isActive()) {
        m_streamingSpinnerTimer->start();
    }

    const int previousRow = m_model->rowOfConversation(previousConversationId);
    if (previousRow >= 0) {
        viewport()->update(visualRect(m_model->index(previousRow, 0)));
    }
    const int currentRow = m_model->rowOfConversation(conversationId);
    if (currentRow >= 0) {
        viewport()->update(visualRect(m_model->index(currentRow, 0)));
    } else {
        viewport()->update();
    }
}

bool AiChatListWidget::isStreamingConversation(const QModelIndex& index) const
{
    return index.isValid() &&
            !m_delegate->streamingConversationId().isEmpty() &&
            index.data(AiChatListModel::ConversationIdRole).toString() ==
                    m_delegate->streamingConversationId();
}

void AiChatListWidget::showItemMenu(const QModelIndex& index, const QPoint& globalPos)
{
    if (!index.isValid()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    const QPersistentModelIndex persistentIndex(index);

    QAction* renameAction = menu->addAction(QStringLiteral("重命名"));
    connect(renameAction, &QAction::triggered, this, [this, persistentIndex]() {
        if (persistentIndex.isValid()) {
            renameItem(persistentIndex);
        }
    });

    QAction* deleteAction = menu->addAction(QStringLiteral("删除"));
    StyledActionMenu::setActionColors(deleteAction,
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionText),
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionBackground),
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionText));
    connect(deleteAction, &QAction::triggered, this, [this, persistentIndex]() {
        if (persistentIndex.isValid()) {
            deleteItem(persistentIndex);
        }
    });

    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popupWhenMouseReleased(globalPos);
}

void AiChatListWidget::renameItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const AiChatListEntry entry = m_model->entryAt(index);
    bool ok = false;
    const QString newTitle = InWindowPopup::getText(this,
                                                    QStringLiteral("重命名"),
                                                    QStringLiteral("新标题："),
                                                    QLineEdit::Normal,
                                                    entry.title,
                                                    &ok).trimmed();
    if (!ok || newTitle.isEmpty()) {
        return;
    }

    if (m_controller && m_controller->renameConversation(entry.conversationId, newTitle)) {
        reloadEntries(entry.conversationId);
    }
}

void AiChatListWidget::deleteItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const int removedRow = index.row();
    const QString removedId = index.data(AiChatListModel::ConversationIdRole).toString();
    const QString currentSelectionId = currentIndex().data(AiChatListModel::ConversationIdRole).toString();
    const bool wasCurrent = currentSelectionId == removedId;
    QString nextSelectionId;
    if (m_model->rowCount() > 1) {
        const int nextRow = qBound(0, removedRow, m_model->rowCount() - 2);
        const QModelIndex nextIndex = m_model->index(nextRow, 0);
        nextSelectionId = nextIndex.data(AiChatListModel::ConversationIdRole).toString();
        if (nextSelectionId == removedId && removedRow > 0) {
            nextSelectionId = m_model->index(removedRow - 1, 0).data(AiChatListModel::ConversationIdRole).toString();
        }
    }

    if (!m_controller || !m_controller->deleteConversation(removedId)) {
        return;
    }

    const QString restoredSelectionId = wasCurrent ? nextSelectionId : currentSelectionId;
    QTimer::singleShot(0, this, [this, restoredSelectionId, removedId]() {
        reloadEntries(restoredSelectionId == removedId ? QString() : restoredSelectionId);
    });
}

void AiChatListWidget::drawStickyHeader() const
{
    if (!m_stickyVisible || m_stickyTitle.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), m_delegate->stickyHeaderHeight()));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), m_delegate->stickyHeaderHeight());
    painter.fillRect(headerRect, ThemeManager::instance().color(ThemeColor::PageBackground));

    QFont headerFont = QApplication::font();
    headerFont.setPixelSize(13);
    headerFont.setBold(true);
    painter.setFont(headerFont);

    const QRect textRect(12, headerRect.top(), headerRect.width() - 24, headerRect.height());
    painter.setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_stickyTitle);
}
