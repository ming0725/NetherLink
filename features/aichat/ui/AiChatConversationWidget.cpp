#include "AiChatConversationWidget.h"
#include "shared/services/AppFonts.h"

#include <QLinearGradient>
#include <QDateTime>
#include <QLoggingCategory>
#include <QModelIndex>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

#include "features/aichat/model/AiChatMessageListModel.h"
#include "features/aichat/ui/AiChatFloatingInputBar.h"
#include "features/aichat/ui/AiChatMessageDelegate.h"
#include "features/aichat/ui/AiChatMessageListView.h"
#include "features/aichat/ui/AiChatSessionController.h"
#include "features/chat/ui/NewMessageNotifier.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/PaintedLabel.h"

namespace {

constexpr int kBottomGradientFadeHeight = 32;
constexpr int kBottomGradientSolidAlpha = 192;

Q_LOGGING_CATEGORY(lcAiChatUnread, "netherlink.aichat.unread")

class ThemeDivider : public QWidget
{
public:
    explicit ThemeDivider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::Divider));
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
                         static_cast<qreal>(kBottomGradientFadeHeight) / rect().height(),
                         1.0)
                : 1.0;
        QColor pageBackground = ThemeManager::instance().color(ThemeColor::PanelBackground);
        pageBackground.setAlpha(0);
        gradient.setColorAt(0.0, pageBackground);
        pageBackground.setAlpha(kBottomGradientSolidAlpha);
        gradient.setColorAt(fadeStop, pageBackground);
        gradient.setColorAt(1.0, pageBackground);
        painter.fillRect(rect(), gradient);
    }
};

} // namespace

AiChatConversationWidget::AiChatConversationWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageView(new AiChatMessageListView(this))
    , m_messageModel(new AiChatMessageListModel(this))
    , m_inputBar(new AiChatFloatingInputBar(this))
    , m_titleLabel(new PaintedLabel(this))
    , m_headerDivider(new ThemeDivider(this))
    , m_bottomGapGradientOverlay(new BottomGapGradientOverlay(this))
    , m_newMessageNotifier(new NewMessageNotifier(this))
    , m_emptyLabel(new PaintedLabel(QStringLiteral("今天需要做什么？"), this))
{
    m_messageView->setModel(m_messageModel);
    m_newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
    m_newMessageNotifier->hide();
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(17);
    m_titleLabel->setFont(titleFont);

    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setPixelSize(30);
    m_emptyLabel->setFont(emptyFont);

    connect(m_inputBar, &AiChatFloatingInputBar::sendText,
            this, &AiChatConversationWidget::onSendText);
    connect(m_inputBar, &AiChatFloatingInputBar::stopStreamingRequested,
            this, &AiChatConversationWidget::onStopStreamingRequested);
    connect(m_inputBar, &AiChatFloatingInputBar::inputFocused,
            m_messageView, &AiChatMessageListView::clearTextSelection);
    connect(m_inputBar, &AiChatFloatingInputBar::preferredHeightChanged,
            this, [this]() { updateLayout(); });
    connect(m_messageView, &AiChatMessageListView::regenerateAiReplyRequested,
            this, &AiChatConversationWidget::onRegenerateAiReplyRequested);
    connect(m_messageView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { updateNewMessageNotifier(); });
    connect(m_messageView, &AiChatMessageListView::userScrollUpIntent, this, [this]() {
        if (!hasActiveAiReplyStream() && m_unreadAiReplyCount == 0) {
            m_newMessageNotifierRevealedByDownScroll = false;
            updateNewMessageNotifier();
        }
    });
    connect(m_messageView, &AiChatMessageListView::userScrollDownIntent, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            if (hasActiveAiReplyStream()) {
                if (shouldShowNewMessageNotifier()) {
                    m_streamingNotifierHeld = true;
                    updateNewMessageNotifier();
                }
                return;
            }

            if (m_unreadAiReplyCount == 0 && shouldShowNewMessageNotifier()) {
                m_newMessageNotifierRevealedByDownScroll = true;
                updateNewMessageNotifier();
            }
        });
    });
    connect(m_newMessageNotifier, &NewMessageNotifier::clicked, this, [this]() {
        m_messageView->scrollToBottom(true);
        if (m_unreadAiReplyCount > 0) {
            qCDebug(lcAiChatUnread).noquote()
                    << "AICHAT_UNREAD clearByNotifier"
                    << "conversationId=" << m_currentConversation.conversationId
                    << "unreadBefore=" << m_unreadAiReplyCount;
        }
        m_unreadAiReplyCount = 0;
        m_newMessageNotifierRevealedByDownScroll = false;
        m_streamingNotifierHeld = false;
        m_newMessageNotifier->hide();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateHeader();
        update();
        m_headerDivider->update();
        m_bottomGapGradientOverlay->update();
        m_newMessageNotifier->update();
        m_messageView->viewport()->update();
    });

    showStartPage();
}

void AiChatConversationWidget::setController(AiChatSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;
    if (!m_controller) {
        closeConversation();
        return;
    }

    connect(m_controller, &AiChatSessionController::aiReplyStarted,
            this, &AiChatConversationWidget::onAiReplyStarted);
    connect(m_controller, &AiChatSessionController::aiReplyMessageAdded,
            this, &AiChatConversationWidget::onAiReplyMessageAdded);
    connect(m_controller, &AiChatSessionController::aiReplyMessageUpdated,
            this, &AiChatConversationWidget::onAiReplyMessageUpdated);
    connect(m_controller, &AiChatSessionController::aiReplyMessageRemoved,
            this, &AiChatConversationWidget::onAiReplyMessageRemoved);
    connect(m_controller, &AiChatSessionController::aiReplyFinished,
            this, &AiChatConversationWidget::onAiReplyFinished);
    connect(m_controller, &AiChatSessionController::aiReplyCanceled,
            this, &AiChatConversationWidget::onAiReplyCanceled);
    connect(m_controller, &AiChatSessionController::messagesLoaded,
            this, &AiChatConversationWidget::onConversationMessagesLoaded);
    connect(m_controller, &AiChatSessionController::contextUsageLoaded,
            this, &AiChatConversationWidget::onContextUsageLoaded);
}

void AiChatConversationWidget::openConversation(const AiChatListEntry& entry)
{
    if (entry.conversationId.isEmpty()) {
        showStartPage();
        return;
    }

    if (isStartPage()) {
        saveStartPageDraft();
        m_inputBar->clearText();
    }

    if (m_currentConversation.conversationId == entry.conversationId) {
        m_currentConversation = entry;
        updateHeader();
        updateLayout();
        return;
    }

    cancelActiveAiReplyStream();
    m_currentConversation = entry;
    m_pendingMessagesConversationId = entry.conversationId;
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    m_pendingMessagesRequestId = 0;
    m_messageView->messageDelegate()->setStreamingMessageId(QString());
    m_messageModel->clear();
    m_messageView->clearTextSelection();
    m_messageView->show();
    m_inputBar->show();
    m_emptyLabel->hide();
    m_titleLabel->show();
    m_headerDivider->show();
    m_unreadAiReplyCount = 0;
    m_newMessageNotifierRevealedByDownScroll = false;
    m_streamingNotifierHeld = false;
    m_newMessageNotifier->hide();
    m_inputBar->setContextUsageVisible(false);
    updateHeader();
    updateLayout();
    if (m_controller) {
        m_pendingMessagesRequestId = m_controller->loadMessagesAsync(entry.conversationId);
        requestContextUsage();
    }
    QTimer::singleShot(0, m_inputBar, [this]() {
        if (m_inputBar->isVisible() && !m_currentConversation.conversationId.isEmpty()) {
            m_inputBar->focusInput();
        }
    });
}

void AiChatConversationWidget::closeConversation()
{
    showStartPage();
}

void AiChatConversationWidget::showStartPage()
{
    cancelActiveAiReplyStream();
    if (isStartPage()) {
        saveStartPageDraft();
    }
    m_currentConversation = {};
    m_pendingMessagesRequestId = 0;
    m_pendingMessagesConversationId.clear();
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    m_messageView->messageDelegate()->setStreamingMessageId(QString());
    m_messageModel->clear();
    m_messageView->clearTextSelection();
    m_messageView->hide();
    m_inputBar->setText(m_startPageDraft);
    m_inputBar->show();
    m_titleLabel->hide();
    m_headerDivider->hide();
    m_unreadAiReplyCount = 0;
    m_newMessageNotifierRevealedByDownScroll = false;
    m_streamingNotifierHeld = false;
    m_newMessageNotifier->hide();
    m_inputBar->setContextUsageVisible(false);
    m_emptyLabel->show();
    updateHeader();
    updateLayout();
    QTimer::singleShot(0, m_inputBar, [this]() {
        if (isStartPage() && m_inputBar->isVisible()) {
            m_inputBar->focusInput();
        }
    });
}

void AiChatConversationWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PanelBackground));
}

void AiChatConversationWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void AiChatConversationWidget::onSendText(const QString& text)
{
    if (!m_controller || hasActiveAiReplyStream()) {
        return;
    }

    if (m_currentConversation.conversationId.isEmpty()) {
        const QString title = titleForPrompt(text);
        const QString conversationId = m_controller->createConversation(title);
        if (conversationId.isEmpty()) {
            m_inputBar->setText(text);
            return;
        }

        m_startPageDraft.clear();
        m_currentConversation = {conversationId, title, QDateTime::currentDateTime()};
        m_messageModel->clear();
        m_messageView->clearTextSelection();
        m_messageView->show();
        m_emptyLabel->hide();
        m_titleLabel->show();
        m_headerDivider->show();
        emit conversationCreatedFromStartPage(conversationId);
        updateHeader();
        updateLayout();
    }

    const AiChatMessage message = m_controller->submitUserMessage(m_currentConversation.conversationId, text);
    if (message.messageId.isEmpty()) {
        m_inputBar->setText(text);
        return;
    }

    m_messageModel->appendMessage(message);
    m_messageView->clearTextSelection();
    m_messageView->scrollToBottom(true);
    m_unreadAiReplyCount = 0;
    m_newMessageNotifierRevealedByDownScroll = false;
    m_streamingNotifierHeld = false;
    requestContextUsage();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::onStopStreamingRequested()
{
    cancelActiveAiReplyStream();
}

void AiChatConversationWidget::onRegenerateAiReplyRequested(const QString& conversationId,
                                                            const QString& messageId)
{
    if (!m_controller ||
            m_currentConversation.conversationId.isEmpty() ||
            conversationId != m_currentConversation.conversationId) {
        return;
    }

    if (!m_controller->regenerateAiReply(conversationId, messageId)) {
        GlobalNotification::showFailure(this, QStringLiteral("只能重新生成最后一条回复"));
    }
}

void AiChatConversationWidget::updateLayout()
{
    if (isStartPage()) {
        m_titleLabel->setGeometry(0, 0, 0, 0);
        m_headerDivider->setGeometry(0, 0, 0, 0);
        m_messageView->setGeometry(0, 0, width(), height());

        const int inputWidth = qMin(kStartPageMaxInputWidth,
                                    qMax(0, width() - kInputBarSideMargin * 2));
        const int inputHeight = m_inputBar->preferredHeightForWidth(inputWidth);
        const int inputX = (width() - inputWidth) / 2;
        const int blockHeight = kStartPageTitleHeight + kStartPageTitleInputGap + kInputBarHeight;
        const int blockTop = qMax(24, (height() - blockHeight) / 2 - 12);
        m_emptyLabel->setGeometry(0, blockTop, width(), kStartPageTitleHeight);
        m_inputBar->setGeometry(inputX,
                                blockTop + kStartPageTitleHeight + kStartPageTitleInputGap,
                                inputWidth,
                                inputHeight);
        m_bottomGapGradientOverlay->hide();
        m_newMessageNotifier->hide();
        m_inputBar->raise();
        m_emptyLabel->raise();
        return;
    }

    const int titleY = kHeaderHeight - kHeaderTitleBottomMargin - kHeaderTitleHeight;
    m_titleLabel->setGeometry(kHeaderTitleLeft,
                              titleY,
                              qMax(0, width() - kHeaderTitleLeft - kHeaderTitleRight),
                              kHeaderTitleHeight);
    m_headerDivider->setGeometry(0, kHeaderHeight, width(), 1);
    m_messageView->setGeometry(0, kHeaderHeight + 1, width(), qMax(0, height() - kHeaderHeight - 1));
    m_emptyLabel->setGeometry(0, kHeaderHeight + 1, width(), qMax(0, height() - kHeaderHeight - 1));

    const int inputWidth = qMax(0, width() - kInputBarSideMargin * 2);
    const int inputHeight = m_inputBar->preferredHeightForWidth(inputWidth);
    const int inputX = kInputBarSideMargin;
    const int inputY = height() - kInputBarBottomMargin - inputHeight;
    m_inputBar->setGeometry(inputX,
                            qMax(kHeaderHeight + 1, inputY),
                            inputWidth,
                            inputHeight);

    updateBottomSpace();
    m_inputBar->raise();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::updateBottomSpace()
{
    if (!m_messageView || !m_inputBar || !m_bottomGapGradientOverlay) {
        return;
    }

    const QRect messageViewRect = m_messageView->geometry();
    const QRect inputBarRect = m_inputBar->geometry();
    const int inputBarOverlapHeight = qMax(0, messageViewRect.bottom() - inputBarRect.top() + 1);
    const int safeBottomSpace = inputBarOverlapHeight + kListBottomPadding;

    m_messageView->setBottomViewportMargin(safeBottomSpace);
    m_bottomGapGradientOverlay->hide();

    m_inputBar->raise();
    updateNewMessageNotifierPosition();
}

void AiChatConversationWidget::requestContextUsage()
{
    if (!m_controller || m_currentConversation.conversationId.isEmpty()) {
        if (m_inputBar) {
            m_inputBar->setContextUsageVisible(false);
        }
        return;
    }

    const AiChatContextUsageRequest request {m_currentConversation.conversationId};
    m_pendingContextUsageConversationId = request.conversationId;
    m_pendingContextUsageRequestId = m_controller->loadContextUsageAsync(request);
}

void AiChatConversationWidget::updateHeader()
{
    const bool hasConversation = !m_currentConversation.conversationId.isEmpty();
    m_titleLabel->setText(hasConversation ? m_currentConversation.title : QStringLiteral("AI 对话"));
    m_titleLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    m_emptyLabel->setText(QStringLiteral("今天需要做什么？"));
    m_emptyLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
}

void AiChatConversationWidget::saveStartPageDraft()
{
    if (m_inputBar && isStartPage()) {
        m_startPageDraft = m_inputBar->text();
    }
}

QString AiChatConversationWidget::titleForPrompt(const QString& text) const
{
    QString title = text.simplified();
    const int lineBreak = title.indexOf(QLatin1Char('\n'));
    if (lineBreak >= 0) {
        title = title.left(lineBreak).trimmed();
    }
    if (title.size() > 24) {
        title = title.left(24).trimmed() + QStringLiteral("...");
    }
    return title.isEmpty() ? QStringLiteral("新对话") : title;
}

void AiChatConversationWidget::updateNewMessageNotifier()
{
    if (!m_newMessageNotifier) {
        return;
    }

    const bool streaming = hasActiveAiReplyStream() &&
            !m_messageView->messageDelegate()->streamingMessageId().isEmpty();
    const bool hasUnread = m_unreadAiReplyCount > 0;

    if (isMessageViewAtBottom()) {
        if (m_unreadAiReplyCount > 0) {
            qCDebug(lcAiChatUnread).noquote()
                    << "AICHAT_UNREAD clearAtBottom"
                    << "conversationId=" << m_currentConversation.conversationId
                    << "unreadBefore=" << m_unreadAiReplyCount;
        }
        m_unreadAiReplyCount = 0;
        m_streamingNotifierHeld = false;
        m_newMessageNotifierRevealedByDownScroll = false;
        m_newMessageNotifier->hide();
        return;
    }

    if (hasUnread) {
        m_newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::Count);
        m_newMessageNotifier->setMessageCount(m_unreadAiReplyCount);
        m_newMessageNotifier->show();
        m_newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        return;
    }

    if (streaming && (m_streamingNotifierHeld || shouldShowNewMessageNotifier())) {
        m_newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::Dots);
        m_streamingNotifierHeld = true;
        m_newMessageNotifier->show();
        m_newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        return;
    }

    if (!streaming &&
            m_newMessageNotifierRevealedByDownScroll &&
            shouldShowNewMessageNotifier()) {
        m_newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
        m_newMessageNotifier->show();
        m_newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
        return;
    }

    m_newMessageNotifier->hide();
}

void AiChatConversationWidget::updateNewMessageNotifierPosition()
{
    if (!m_newMessageNotifier || !m_newMessageNotifier->isVisible() || !m_inputBar) {
        return;
    }

    const QRect inputBarRect = m_inputBar->geometry();
    const int x = inputBarRect.right() + 1 - m_newMessageNotifier->width();
    const int y = inputBarRect.y() - m_newMessageNotifier->height() - kNewMessageNotifierInputGap;
    m_newMessageNotifier->move(x, y);
    m_newMessageNotifier->raise();
}

bool AiChatConversationWidget::shouldShowNewMessageNotifier() const
{
    if (!m_messageView ||
            !m_inputBar ||
            m_currentConversation.conversationId.isEmpty() ||
            m_messageView->isHidden() ||
            m_inputBar->isHidden()) {
        return false;
    }

    const QScrollBar* scrollBar = m_messageView->verticalScrollBar();
    if (!scrollBar) {
        return false;
    }

    const int bottomDistance = scrollBar->maximum() - scrollBar->value();
    const int threshold = qMax(kNewMessageNotifierMinBottomDistance,
                               m_messageView->viewport()->height() /
                                       kNewMessageNotifierViewportDistanceDivisor);
    return bottomDistance > threshold;
}

bool AiChatConversationWidget::isMessageViewAtBottom() const
{
    if (!m_messageView) {
        return true;
    }

    const QScrollBar* scrollBar = m_messageView->verticalScrollBar();
    return !scrollBar || scrollBar->maximum() - scrollBar->value() <= 2;
}

bool AiChatConversationWidget::isStartPage() const
{
    return m_currentConversation.conversationId.isEmpty();
}

void AiChatConversationWidget::onAiReplyStarted(const QString& conversationId)
{
    if (m_currentConversation.conversationId == conversationId) {
        m_inputBar->setStreaming(true);
        m_streamingNotifierHeld = false;
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyMessageAdded(const AiChatMessage& message)
{
    if (m_currentConversation.conversationId == message.conversationId) {
        const bool shouldFollowReply = m_messageView->isBottomLocked();
        m_messageView->messageDelegate()->setStreamingMessageId(message.messageId);
        m_messageModel->appendMessage(message);
        if (!message.isFromUser && !shouldFollowReply) {
            ++m_unreadAiReplyCount;
            qCDebug(lcAiChatUnread).noquote()
                    << "AICHAT_UNREAD replyAdded"
                    << "conversationId=" << message.conversationId
                    << "messageId=" << message.messageId
                    << "unread=" << m_unreadAiReplyCount
                    << "bottomLocked=" << shouldFollowReply;
        }
        m_messageView->scrollToBottomIfLocked();
        updateNewMessageNotifier();
        QTimer::singleShot(0, this, [this]() {
            updateNewMessageNotifier();
        });
        return;
    }
}

void AiChatConversationWidget::onAiReplyMessageUpdated(const QString& conversationId,
                                                       const QString& messageId,
                                                       const QString& text)
{
    if (m_currentConversation.conversationId == conversationId) {
        if (m_messageView->messageDelegate()->streamingMessageId().isEmpty()) {
            m_messageView->messageDelegate()->setStreamingMessageId(messageId);
        }
        m_messageModel->updateMessageText(messageId, text);
        m_messageView->scrollToBottomIfLocked();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyMessageRemoved(const QString& conversationId,
                                                       const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId) {
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
        }
        m_messageModel->removeMessage(messageId);
        m_messageView->clearTextSelection();
        m_messageView->scrollToBottomIfLocked();
        requestContextUsage();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyFinished(const QString& conversationId, const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId) {
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
            m_messageView->refreshMessageLayout();
        }
        m_inputBar->setStreaming(false);
        if (m_streamingNotifierHeld && shouldShowNewMessageNotifier()) {
            m_newMessageNotifierRevealedByDownScroll = true;
        }
        m_streamingNotifierHeld = false;
        requestContextUsage();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyCanceled(const QString& conversationId, const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId || m_currentConversation.conversationId.isEmpty()) {
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
            m_messageView->refreshMessageLayout();
        }
        m_inputBar->setStreaming(false);
        if (m_streamingNotifierHeld && shouldShowNewMessageNotifier()) {
            m_newMessageNotifierRevealedByDownScroll = true;
        }
        m_streamingNotifierHeld = false;
    }
    updateNewMessageNotifier();
}

void AiChatConversationWidget::onConversationMessagesLoaded(int requestId,
                                                            const QString& conversationId,
                                                            const QVector<AiChatMessage>& messages)
{
    if (requestId != m_pendingMessagesRequestId ||
            conversationId != m_pendingMessagesConversationId ||
            conversationId != m_currentConversation.conversationId) {
        return;
    }

    m_pendingMessagesRequestId = 0;
    m_pendingMessagesConversationId.clear();
    m_messageModel->setMessages(messages);
    requestContextUsage();
    QTimer::singleShot(0, this, [this, conversationId]() {
        if (conversationId == m_currentConversation.conversationId) {
            m_messageView->jumpToBottom();
        }
    });
}

void AiChatConversationWidget::onContextUsageLoaded(int requestId,
                                                    const AiChatContextUsageRequest& request,
                                                    const AiChatContextUsage& usage)
{
    if (requestId != m_pendingContextUsageRequestId ||
            request.conversationId != m_pendingContextUsageConversationId ||
            request.conversationId != m_currentConversation.conversationId) {
        return;
    }

    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    m_inputBar->setContextUsage(usage);
}

void AiChatConversationWidget::cancelActiveAiReplyStream()
{
    if (m_controller) {
        m_controller->cancelActiveAiReplyStream();
    }
    if (m_inputBar) {
        m_inputBar->setStreaming(false);
    }
}

bool AiChatConversationWidget::hasActiveAiReplyStream() const
{
    return m_controller && m_controller->hasActiveAiReplyStream();
}
