#include "AiChatConversationWidget.h"
#include <QDateTime>
#include <QEasingCurve>
#include <QModelIndex>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QVariantAnimation>

#include <algorithm>
#include <utility>

#include "features/aichat/model/AiChatMessageListModel.h"
#include "features/aichat/ui/AiChatFloatingInputBar.h"
#include "features/aichat/ui/AiChatMessageDelegate.h"
#include "features/aichat/ui/AiChatMessageListView.h"
#include "features/aichat/ui/AiChatSessionController.h"
#include "features/chat/ui/NewMessageNotifier.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/BottomFadeOverlay.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/PaintedLabel.h"

namespace {

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

QString joinedStatusUrls(const QVector<QString>& urls)
{
    QStringList parts;
    parts.reserve(qMin(urls.size(), 3));
    for (const QString& url : urls) {
        const QString trimmed = url.trimmed();
        if (!trimmed.isEmpty()) {
            parts.push_back(trimmed);
        }
        if (parts.size() == 3) {
            break;
        }
    }
    return parts.join(QStringLiteral("  "));
}

QString traceRowPrefix(const QString& messageId)
{
    return QStringLiteral("__ai_trace_%1_").arg(messageId);
}

QString statusActionText(const QString& phase, const QString& status)
{
    const QString normalizedPhase = phase.trimmed().toLower();
    const QString normalizedStatus = status.trimmed().toLower();
    const bool completed = normalizedStatus == QStringLiteral("completed");
    const bool failed = normalizedStatus == QStringLiteral("failed");

    if (normalizedPhase == QStringLiteral("searching")) {
        return failed ? QStringLiteral("搜索失败")
                      : (completed ? QStringLiteral("搜索完成") : QStringLiteral("正在搜索"));
    }
    if (normalizedPhase == QStringLiteral("reading")) {
        return failed ? QStringLiteral("读取失败")
                      : (completed ? QStringLiteral("读取完成") : QStringLiteral("正在读取"));
    }
    if (normalizedPhase == QStringLiteral("tool")) {
        return failed ? QStringLiteral("工具调用失败")
                      : (completed ? QStringLiteral("工具调用完成") : QStringLiteral("正在调用工具"));
    }
    if (normalizedPhase == QStringLiteral("answering")) {
        return failed ? QStringLiteral("生成失败")
                      : (completed ? QStringLiteral("生成完成") : QStringLiteral("正在生成回答"));
    }

    return failed ? QStringLiteral("思考失败")
                  : (completed ? QStringLiteral("思考完成") : QStringLiteral("正在思考"));
}

QString streamStatusDisplayText(const AiChatStreamStatus& status)
{
    const QString message = status.message.trimmed();
    if (status.phase.trimmed().isEmpty() &&
            status.status.trimmed().isEmpty() &&
            status.query.trimmed().isEmpty() &&
            status.urls.isEmpty() &&
            !message.isEmpty()) {
        return message;
    }

    const QString action = statusActionText(status.phase, status.status);
    const QString query = status.query.trimmed();
    const QString tool = status.tool.trimmed();
    const QString urls = joinedStatusUrls(status.urls);

    QString detail = query;
    if (detail.isEmpty()) {
        detail = tool;
    }
    if (detail.isEmpty()) {
        detail = message;
    }
    if (!urls.isEmpty()) {
        detail = detail.isEmpty() ? urls : QStringLiteral("%1  %2").arg(detail, urls);
    }

    QString text = action;
    if (!detail.isEmpty()) {
        text += QStringLiteral("：%1").arg(detail);
    }

    if (text.trimmed().isEmpty()) {
        return message.isEmpty()
                ? QStringLiteral("正在思考")
                : message;
    }
    return text;
}

} // namespace

AiChatConversationWidget::AiChatConversationWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageView(new AiChatMessageListView(this))
    , m_messageModel(new AiChatMessageListModel(this))
    , m_inputBar(new AiChatFloatingInputBar(this))
    , m_titleLabel(new PaintedLabel(this))
    , m_headerDivider(new ThemeDivider(this))
    , m_bottomGapGradientOverlay(new BottomFadeOverlay(this))
    , m_newMessageNotifier(new NewMessageNotifier(this))
    , m_emptyLabel(new PaintedLabel(QStringLiteral("今天需要做什么？"), this))
    , m_thinkingAnimationTimer(new QTimer(this))
{
    m_messageView->setModel(m_messageModel);
    m_newMessageNotifier->setDisplayMode(NewMessageNotifier::DisplayMode::IconOnly);
    m_newMessageNotifier->hide();
    static_cast<BottomFadeOverlay*>(m_bottomGapGradientOverlay)->setBackgroundRole(ThemeColor::PanelBackground);
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
    m_thinkingAnimationTimer->setInterval(kThinkingAnimationFrameMs);
    connect(m_thinkingAnimationTimer, &QTimer::timeout, this, [this]() {
        if (!m_messageModel->hasActiveThinkingMessages()) {
            m_thinkingAnimationTimer->stop();
            return;
        }
        if (m_messageView && m_messageView->isVisible()) {
            m_messageView->viewport()->update();
        }
    });
    connect(m_inputBar, &AiChatFloatingInputBar::stopStreamingRequested,
            this, &AiChatConversationWidget::onStopStreamingRequested);
    connect(m_inputBar, &AiChatFloatingInputBar::inputFocused,
            m_messageView, &AiChatMessageListView::clearTextSelection);
    connect(m_inputBar, &AiChatFloatingInputBar::preferredHeightChanged,
            this, [this]() { updateLayout(); });
    connect(m_messageView, &AiChatMessageListView::regenerateAiReplyRequested,
            this, &AiChatConversationWidget::onRegenerateAiReplyRequested);
    connect(m_messageView, &AiChatMessageListView::traceToggleRequested,
            this, &AiChatConversationWidget::onTraceToggleRequested);
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

    m_controller->setCurrentConversationId(m_currentConversation.conversationId);

    connect(m_controller, &AiChatSessionController::aiReplyStarted,
            this, &AiChatConversationWidget::onAiReplyStarted);
    connect(m_controller, &AiChatSessionController::aiReplyMessageAdded,
            this, &AiChatConversationWidget::onAiReplyMessageAdded);
    connect(m_controller, &AiChatSessionController::aiReplyMessageUpdated,
            this, &AiChatConversationWidget::onAiReplyMessageUpdated);
    connect(m_controller, &AiChatSessionController::aiReplyMessageReplaced,
            this, &AiChatConversationWidget::onAiReplyMessageReplaced);
    connect(m_controller, &AiChatSessionController::aiReplyMessageRemoved,
            this, &AiChatConversationWidget::onAiReplyMessageRemoved);
    connect(m_controller, &AiChatSessionController::aiReplyProgressChanged,
            this, &AiChatConversationWidget::onAiReplyProgressChanged);
    connect(m_controller, &AiChatSessionController::aiReplyThinkingChanged,
            this, &AiChatConversationWidget::onAiReplyThinkingChanged);
    connect(m_controller, &AiChatSessionController::aiReplyStreamStatusChanged,
            this, &AiChatConversationWidget::onAiReplyStreamStatusChanged);
    connect(m_controller, &AiChatSessionController::aiReplyFinished,
            this, &AiChatConversationWidget::onAiReplyFinished);
    connect(m_controller, &AiChatSessionController::aiReplyCanceled,
            this, &AiChatConversationWidget::onAiReplyCanceled);
    connect(m_controller, &AiChatSessionController::aiReplyFailed,
            this, &AiChatConversationWidget::onAiReplyFailed);
    connect(m_controller, &AiChatSessionController::conversationIdChanged,
            this, [this](const QString& previousConversationId, const AiChatListEntry& entry) {
                if (m_currentConversation.conversationId != previousConversationId) {
                    return;
                }

                m_currentConversation.conversationId = entry.conversationId;
                m_controller->setCurrentConversationId(entry.conversationId);
                if (!entry.title.isEmpty()) {
                    m_currentConversation.title = entry.title;
                }
                if (entry.time.isValid()) {
                    m_currentConversation.time = entry.time;
                }
                m_pendingMessagesConversationId = entry.conversationId;
                m_pendingContextUsageConversationId = entry.conversationId;
                emit conversationCreatedFromStartPage(entry.conversationId);
                updateHeader();
            });
    connect(m_controller, &AiChatSessionController::conversationTitleChanged,
            this, [this](const QString& conversationId, const QString& title) {
                if (m_currentConversation.conversationId != conversationId) {
                    return;
                }

                m_currentConversation.title = title;
                updateHeader();
            });
    connect(m_controller, &AiChatSessionController::conversationDeleted,
            this, [this](const QString& conversationId) {
                if (m_currentConversation.conversationId == conversationId) {
                    showStartPage();
                }
            });
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

    if (m_controller) {
        m_controller->setCurrentConversationId(entry.conversationId);
    }

    if (m_currentConversation.conversationId == entry.conversationId) {
        m_currentConversation = entry;
        if (m_controller) {
            m_controller->clearConversationUnreadDot(entry.conversationId);
            const bool currentConversationStreaming =
                    m_controller->activeStreamConversationId() == entry.conversationId;
            m_inputBar->setStreaming(currentConversationStreaming);
            m_messageView->messageDelegate()->setStreamingMessageId(
                    currentConversationStreaming ? m_controller->activeStreamMessageId() : QString());
        }
        updateHeader();
        updateLayout();
        return;
    }

    m_currentConversation = entry;
    if (m_controller) {
        m_controller->clearConversationUnreadDot(entry.conversationId);
    }
    m_pendingMessagesConversationId = entry.conversationId;
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    m_pendingMessagesRequestId = 0;
    const bool currentConversationStreaming = m_controller &&
            m_controller->activeStreamConversationId() == entry.conversationId;
    m_messageView->messageDelegate()->setStreamingMessageId(
            currentConversationStreaming ? m_controller->activeStreamMessageId() : QString());
    cancelScheduledThinkingPlaceholder();
    m_thinkingMessageId.clear();
    resetStreamPresentation();
    m_thinkingAnimationTimer->stop();
    stopTraceExpansionAnimations();
    m_messageModel->clear();
    m_messageTraces.clear();
    m_expandedTraceMessageIds.clear();
    m_collapsingTraceMessageIds.clear();
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
    m_inputBar->setStreaming(currentConversationStreaming, false);
    updateHeader();
    updateLayout();
    const QPointer<AiChatConversationWidget> self(this);
    const QString expectedConversationId = entry.conversationId;
    QTimer::singleShot(0, this, [self, expectedConversationId]() {
        if (!self || !self->m_controller) {
            return;
        }
        if (self->m_currentConversation.conversationId != expectedConversationId) {
            return;
        }

        self->m_pendingMessagesRequestId = self->m_controller->loadMessagesAsync(expectedConversationId);
        self->requestContextUsage();
    });
}

void AiChatConversationWidget::closeConversation()
{
    showStartPage();
}

void AiChatConversationWidget::showStartPage()
{
    if (isStartPage()) {
        saveStartPageDraft();
    }
    m_currentConversation = {};
    if (m_controller) {
        m_controller->setCurrentConversationId(QString());
    }
    m_pendingMessagesRequestId = 0;
    m_pendingMessagesConversationId.clear();
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    m_messageView->messageDelegate()->setStreamingMessageId(QString());
    cancelScheduledThinkingPlaceholder();
    m_thinkingMessageId.clear();
    resetStreamPresentation();
    m_thinkingAnimationTimer->stop();
    stopTraceExpansionAnimations();
    m_messageModel->clear();
    m_messageTraces.clear();
    m_expandedTraceMessageIds.clear();
    m_collapsingTraceMessageIds.clear();
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
    m_inputBar->setStreaming(false, false);
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

void AiChatConversationWidget::onSendText(const QString& text, const AiChatRequestOptions& options)
{
    if (!m_controller || m_controller->hasActiveAiReplyStream()) {
        return;
    }

    if (m_currentConversation.conversationId.isEmpty()) {
        const AiChatListEntry entry = m_controller->createConversationFromFirstMessage(text);
        if (entry.conversationId.isEmpty()) {
            m_inputBar->setText(text);
            return;
        }

        m_startPageDraft.clear();
        m_currentConversation = entry;
        m_controller->setCurrentConversationId(entry.conversationId);
        m_messageModel->clear();
        m_messageView->clearTextSelection();
        m_messageView->show();
        m_emptyLabel->hide();
        m_titleLabel->show();
        m_headerDivider->show();
        emit conversationCreatedFromStartPage(entry.conversationId);
        updateHeader();
        updateLayout();
        QTimer::singleShot(0, m_inputBar, [inputBar = m_inputBar]() {
            if (inputBar->isVisible()) {
                inputBar->refocusInputAfterPositionChange();
            }
        });
    }

    const AiChatMessage message = m_controller->submitUserMessage(m_currentConversation.conversationId,
                                                                  text,
                                                                  options);
    if (message.messageId.isEmpty()) {
        m_inputBar->setText(text);
        return;
    }

    m_messageView->setUpdatesEnabled(false);
    m_messageModel->appendMessage(message);
    showScheduledThinkingPlaceholder(message.conversationId);
    m_messageView->setUpdatesEnabled(true);
    m_messageView->viewport()->update();
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

    if (!m_controller->regenerateAiReply(conversationId, messageId, m_inputBar->requestOptions())) {
        GlobalNotification::showFailure(this, QStringLiteral("只能重新生成最后一条回复"));
        return;
    }

    showScheduledThinkingPlaceholder(conversationId);
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
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    if (m_inputBar) {
        m_inputBar->setContextUsageVisible(false);
    }
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

void AiChatConversationWidget::scheduleThinkingPlaceholder(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    m_pendingThinkingConversationId = conversationId;
}

void AiChatConversationWidget::cancelScheduledThinkingPlaceholder()
{
    m_pendingThinkingConversationId.clear();
}

void AiChatConversationWidget::showScheduledThinkingPlaceholder(const QString& conversationId)
{
    if (conversationId.isEmpty() ||
            m_pendingThinkingConversationId != conversationId ||
            !m_controller ||
            m_controller->activeStreamConversationId() != conversationId ||
            m_streamHasAnswerText) {
        return;
    }

    showThinkingPlaceholder(conversationId, QStringLiteral("正在思考"));
}

void AiChatConversationWidget::showThinkingPlaceholder(const QString& conversationId, const QString& text)
{
    if (conversationId.isEmpty() ||
            m_currentConversation.conversationId != conversationId ||
            hasThinkingPlaceholder()) {
        return;
    }

    const QString displayText = text.trimmed().isEmpty()
            ? QStringLiteral("正在思考")
            : text.trimmed();
    AiChatMessage message;
    message.messageId = QStringLiteral("__ai_thinking_%1_%2")
            .arg(conversationId)
            .arg(++m_streamActionSerial);
    message.conversationId = conversationId;
    message.text = displayText;
    message.isFromUser = false;
    message.time = QDateTime::currentDateTime();

    const bool shouldFollowReply = m_messageView->isBottomLocked();
    m_pendingThinkingConversationId.clear();
    m_thinkingMessageId = message.messageId;
    m_messageModel->appendMessage(message);
    if (!shouldFollowReply) {
        ++m_unreadAiReplyCount;
    }
    if (!m_thinkingAnimationTimer->isActive()) {
        m_thinkingAnimationTimer->start();
    }
    m_messageView->scrollToBottomIfLocked();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::showOrUpdateThinkingPlaceholder(const QString& conversationId,
                                                               const QString& text,
                                                               bool retain,
                                                               bool active)
{
    if (conversationId.isEmpty() ||
            m_currentConversation.conversationId != conversationId) {
        return;
    }

    const QString displayText = text.trimmed().isEmpty()
            ? QStringLiteral("正在思考")
            : text.trimmed();
    cancelScheduledThinkingPlaceholder();
    m_retainThinkingPlaceholder = retain;
    if (!hasThinkingPlaceholder()) {
        showThinkingPlaceholder(conversationId, displayText);
    } else {
        m_messageModel->updateMessageText(m_thinkingMessageId, displayText);
    }

    m_messageModel->setThinkingMessageActive(m_thinkingMessageId, active);
    if (active && !m_thinkingAnimationTimer->isActive()) {
        m_thinkingAnimationTimer->start();
    } else if (!active) {
        m_thinkingAnimationTimer->stop();
    }
    m_messageView->scrollToBottomIfLocked();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::hideThinkingPlaceholder(const QString& conversationId)
{
    cancelScheduledThinkingPlaceholder();
    if (m_thinkingMessageId.isEmpty() ||
            (!conversationId.isEmpty() && m_currentConversation.conversationId != conversationId)) {
        return;
    }

    m_messageModel->removeMessage(m_thinkingMessageId);
    m_thinkingMessageId.clear();
    m_thinkingAnimationTimer->stop();
    m_messageView->scrollToBottomIfLocked();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::finishThinkingPlaceholder(const QString& conversationId)
{
    cancelScheduledThinkingPlaceholder();
    if (m_thinkingMessageId.isEmpty() ||
            (!conversationId.isEmpty() && m_currentConversation.conversationId != conversationId)) {
        return;
    }

    m_thinkingAnimationTimer->stop();
    m_messageModel->setThinkingMessageActive(m_thinkingMessageId, false);
    if (!m_retainThinkingPlaceholder) {
        hideThinkingPlaceholder(conversationId);
    }
}

bool AiChatConversationWidget::hasThinkingPlaceholder() const
{
    return !m_thinkingMessageId.isEmpty();
}

void AiChatConversationWidget::presentStreamAnswer(const AiChatMessage& message,
                                                   const QString& text)
{
    if (message.messageId.isEmpty() || text.isEmpty()) {
        return;
    }

    m_streamAggregateAnswerText = text;
    const bool shouldFollowReply = m_messageView->isBottomLocked();
    const bool hadThinkingPlaceholder = hasThinkingPlaceholder();
    cancelScheduledThinkingPlaceholder();
    if (hadThinkingPlaceholder) {
        hideThinkingPlaceholder(message.conversationId);
    }

    if (m_streamCurrentAnswerMessageId.isEmpty()) {
        m_streamCurrentAnswerMessageId = message.messageId;
        m_messageModel->appendMessage(message);
        if (!message.isFromUser && !shouldFollowReply && !hadThinkingPlaceholder) {
            ++m_unreadAiReplyCount;
        }
    } else {
        m_messageModel->updateMessageText(m_streamCurrentAnswerMessageId,
                                          text);
    }

    m_streamHasAnswerText = true;
    m_messageView->messageDelegate()->setStreamingMessageId(
            m_streamCurrentAnswerMessageId);
    m_messageView->scrollToBottomIfLocked();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::rebuildStreamWorkRows()
{
    if (m_streamWorkPrefix.isEmpty()) {
        return;
    }

    QVector<StreamWorkStep> steps = m_streamWorkSteps.values().toVector();
    std::sort(steps.begin(), steps.end(), [](const StreamWorkStep& lhs,
                                             const StreamWorkStep& rhs) {
        if (lhs.sequence > 0 && rhs.sequence > 0 && lhs.sequence != rhs.sequence) {
            return lhs.sequence < rhs.sequence;
        }
        if (lhs.sequence > 0 && rhs.sequence <= 0) {
            return true;
        }
        if (lhs.sequence <= 0 && rhs.sequence > 0) {
            return false;
        }
        return lhs.encounterOrder < rhs.encounterOrder;
    });

    QVector<AiChatMessage> rows;
    QSet<QString> activeIds;
    for (const StreamWorkStep& step : steps) {
        QString progressText;
        for (const QString& segmentId : step.progressSegmentOrder) {
            const AiChatProgressSegment segment = step.progressSegments.value(segmentId);
            if (!segment.text.isEmpty()) {
                if (!progressText.isEmpty() && !progressText.endsWith(QLatin1Char('\n'))) {
                    progressText += QLatin1Char(' ');
                }
                progressText += segment.text.trimmed();
            }
        }

        const QString safeStepId = QString(step.stepId).replace(QLatin1Char('/'), QLatin1Char('_'));
        if (!progressText.isEmpty()) {
            AiChatMessage actionRow;
            actionRow.messageId = m_streamWorkPrefix + QStringLiteral("action_") + safeStepId;
            actionRow.conversationId = m_currentConversation.conversationId;
            actionRow.text = progressText;
            actionRow.time = QDateTime::currentDateTime();
            rows.push_back(actionRow);
        }

        if (step.hasToolStatus && !progressText.isEmpty()) {
            AiChatMessage toolRow;
            toolRow.messageId = m_streamWorkPrefix + QStringLiteral("tool_") + safeStepId;
            toolRow.conversationId = m_currentConversation.conversationId;
            toolRow.text = streamStatusDisplayText(step.toolStatus);
            toolRow.time = QDateTime::currentDateTime();
            rows.push_back(toolRow);
            if (step.toolStatus.status.trimmed().toLower() == QStringLiteral("running")) {
                activeIds.insert(toolRow.messageId);
            }
        }
    }

    m_messageModel->setTransientMessages(m_streamWorkPrefix,
                                         rows,
                                         activeIds,
                                         m_streamCurrentAnswerMessageId);
    if (!activeIds.isEmpty()) {
        if (!m_thinkingAnimationTimer->isActive()) {
            m_thinkingAnimationTimer->start();
        }
    } else if (!m_messageModel->hasActiveThinkingMessages()) {
        m_thinkingAnimationTimer->stop();
    }
    m_messageView->scrollToBottomIfLocked();
    updateNewMessageNotifier();
}

void AiChatConversationWidget::clearStreamWorkRows()
{
    if (!m_streamWorkPrefix.isEmpty()) {
        m_messageModel->setTransientMessages(m_streamWorkPrefix, {}, {});
    }
    m_streamWorkSteps.clear();
    m_progressSegmentStepIds.clear();
}

QVector<AiChatMessage> AiChatConversationWidget::traceRows(const AiChatMessage& message) const
{
    QVector<AiChatMessage> rows;
    if (!message.trace.isValid() || message.messageId.isEmpty()) {
        return rows;
    }

    const QString prefix = traceRowPrefix(message.messageId);
    const bool expanded = m_expandedTraceMessageIds.contains(message.messageId);
    const bool showDetails = expanded || m_collapsingTraceMessageIds.contains(message.messageId);
    AiChatMessage summaryRow;
    summaryRow.messageId = prefix + QStringLiteral("summary");
    summaryRow.conversationId = message.conversationId;
    summaryRow.text = expanded
            ? QStringLiteral("点击收起")
            : QStringLiteral("点击展开");
    summaryRow.time = message.time;
    rows.push_back(summaryRow);

    if (!showDetails) {
        return rows;
    }

    QVector<AiChatTraceStep> steps = message.trace.steps;
    std::sort(steps.begin(), steps.end(), [](const AiChatTraceStep& lhs,
                                             const AiChatTraceStep& rhs) {
        return lhs.sequence < rhs.sequence;
    });
    int fallbackIndex = 0;
    for (const AiChatTraceStep& step : steps) {
        const QString stepId = step.stepId.isEmpty()
                ? QStringLiteral("step-%1").arg(++fallbackIndex)
                : step.stepId;
        const QString safeStepId = QString(stepId).replace(QLatin1Char('/'), QLatin1Char('_'));
        if (!step.text.trimmed().isEmpty()) {
            AiChatMessage actionRow;
            actionRow.messageId = prefix + QStringLiteral("action_") + safeStepId;
            actionRow.conversationId = message.conversationId;
            actionRow.text = step.text.trimmed();
            actionRow.time = message.time;
            rows.push_back(actionRow);
        }

        const QString phase = step.phase.trimmed().toLower();
        if (phase == QStringLiteral("searching") ||
                phase == QStringLiteral("reading") ||
                phase == QStringLiteral("tool")) {
            AiChatStreamStatus status;
            status.stepId = step.stepId;
            status.sequence = step.sequence;
            status.phase = step.phase;
            status.status = step.status;
            status.tool = step.tool;
            status.query = step.query;
            status.urls = step.urls;
            status.message = step.text;

            AiChatMessage toolRow;
            toolRow.messageId = prefix + QStringLiteral("tool_") + safeStepId;
            toolRow.conversationId = message.conversationId;
            toolRow.text = streamStatusDisplayText(status);
            toolRow.time = message.time;
            rows.push_back(toolRow);
        }
    }
    return rows;
}

void AiChatConversationWidget::installTraceRows(const AiChatMessage& message, qreal expansionProgress)
{
    if (!message.trace.isValid() || message.messageId.isEmpty()) {
        return;
    }

    m_messageTraces.insert(message.messageId, message.trace);
    const QString prefix = traceRowPrefix(message.messageId);
    m_messageModel->setTransientMessages(prefix,
                                         traceRows(message),
                                         {},
                                         message.messageId,
                                         expansionProgress);
}

void AiChatConversationWidget::onTraceToggleRequested(const QString& messageId)
{
    const AiChatTrace trace = m_messageTraces.value(messageId);
    const AiChatMessage message = m_messageModel->messageById(messageId);
    if (!trace.isValid() || message.messageId.isEmpty()) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_traceExpansionAnimations.value(messageId)) {
        running->stop();
        running->deleteLater();
    }

    const QString prefix = traceRowPrefix(messageId);
    const qreal startProgress = m_messageModel->traceExpansionProgress(prefix);
    const bool expand = !m_expandedTraceMessageIds.contains(messageId);

    if (expand) {
        m_collapsingTraceMessageIds.remove(messageId);
        m_expandedTraceMessageIds.insert(messageId);
    } else {
        m_expandedTraceMessageIds.remove(messageId);
        m_collapsingTraceMessageIds.insert(messageId);
    }

    AiChatMessage tracedMessage = message;
    tracedMessage.trace = trace;
    installTraceRows(tracedMessage, startProgress);
    m_messageView->refreshMessageLayout(false);

    const qreal endProgress = expand ? 1.0 : 0.0;
    auto* animation = new QVariantAnimation(this);
    m_traceExpansionAnimations.insert(messageId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(endProgress);
    animation->setDuration(kTraceExpandAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, prefix](const QVariant& value) {
        m_messageModel->setTraceExpansionProgress(prefix, value.toReal());
        m_messageView->refreshMessageLayout(false);
    });
    connect(animation, &QVariantAnimation::finished, this, [this, messageId, prefix, tracedMessage, expand, animation]() {
        if (expand) {
            m_messageModel->setTraceExpansionProgress(prefix, 1.0);
        } else {
            m_collapsingTraceMessageIds.remove(messageId);
            installTraceRows(tracedMessage, 0.0);
        }
        m_traceExpansionAnimations.remove(messageId);
        animation->deleteLater();
        m_messageView->refreshMessageLayout(false);
    });

    animation->start();
}

void AiChatConversationWidget::stopTraceExpansionAnimations()
{
    for (const QPointer<QVariantAnimation>& animation : std::as_const(m_traceExpansionAnimations)) {
        if (animation) {
            animation->stop();
            animation->deleteLater();
        }
    }
    m_traceExpansionAnimations.clear();
}

void AiChatConversationWidget::resetStreamPresentation()
{
    m_streamHasAnswerText = false;
    m_retainThinkingPlaceholder = false;
    m_streamAggregateAnswerText.clear();
    m_streamCurrentAnswerMessageId.clear();
    m_streamWorkPrefix.clear();
    m_streamWorkSteps.clear();
    m_progressSegmentStepIds.clear();
    m_nextWorkEncounterOrder = 1;
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
        m_messageModel->deactivateAllThinkingMessages();
        m_thinkingMessageId.clear();
        m_thinkingAnimationTimer->stop();
        resetStreamPresentation();
        m_streamWorkPrefix = QStringLiteral("__ai_work_%1_%2_")
                .arg(conversationId)
                .arg(++m_streamActionSerial);
        m_inputBar->setStreaming(true);
        scheduleThinkingPlaceholder(conversationId);
        m_streamingNotifierHeld = false;
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyMessageAdded(const AiChatMessage& message,
                                                     bool isProgress)
{
    Q_UNUSED(isProgress)
    if (m_currentConversation.conversationId == message.conversationId) {
        presentStreamAnswer(message, message.text);
        QTimer::singleShot(0, this, [this]() {
            updateNewMessageNotifier();
        });
        return;
    }
}

void AiChatConversationWidget::onAiReplyMessageUpdated(const QString& conversationId,
                                                       const QString& messageId,
                                                       const QString& text,
                                                       bool isProgress)
{
    Q_UNUSED(isProgress)
    if (m_currentConversation.conversationId == conversationId) {
        AiChatMessage message;
        message.messageId = messageId;
        message.conversationId = conversationId;
        message.text = text;
        message.isFromUser = false;
        message.time = QDateTime::currentDateTime();
        presentStreamAnswer(message, text);
    }
}

void AiChatConversationWidget::onAiReplyMessageReplaced(const QString& conversationId,
                                                        const QString& messageId,
                                                        const AiChatMessage& replacement)
{
    if (m_currentConversation.conversationId == conversationId) {
        cancelScheduledThinkingPlaceholder();
        finishThinkingPlaceholder(conversationId);

        if (m_messageView->messageDelegate()->streamingMessageId() == messageId ||
                m_messageView->messageDelegate()->streamingMessageId() ==
                        m_streamCurrentAnswerMessageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(replacement.messageId);
        }
        const QString replacementTarget = m_streamCurrentAnswerMessageId.isEmpty()
                ? messageId
                : m_streamCurrentAnswerMessageId;
        m_messageModel->replaceMessage(replacementTarget, replacement);
        m_streamCurrentAnswerMessageId = replacement.messageId;
        m_streamAggregateAnswerText = replacement.text;
        if (replacement.trace.isValid()) {
            clearStreamWorkRows();
            installTraceRows(replacement);
        } else {
            m_messageModel->deactivateAllThinkingMessages();
        }
        m_messageView->scrollToBottomIfLocked();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyProgressChanged(
        const QString& conversationId,
        const AiChatProgressSegment& progress)
{
    if (m_currentConversation.conversationId != conversationId ||
            progress.segmentId.isEmpty()) {
        return;
    }

    if (hasThinkingPlaceholder() && !progress.text.isEmpty()) {
        hideThinkingPlaceholder(conversationId);
    }

    QString stepId = progress.stepId;
    if (stepId.isEmpty()) {
        stepId = m_progressSegmentStepIds.value(progress.segmentId);
    }
    if (stepId.isEmpty()) {
        stepId = QStringLiteral("progress-%1").arg(m_nextWorkEncounterOrder);
    }
    m_progressSegmentStepIds.insert(progress.segmentId, stepId);

    StreamWorkStep& step = m_streamWorkSteps[stepId];
    if (step.stepId.isEmpty()) {
        step.stepId = stepId;
        step.encounterOrder = m_nextWorkEncounterOrder++;
    }
    if (!step.progressSegments.contains(progress.segmentId)) {
        step.progressSegmentOrder.push_back(progress.segmentId);
    }
    step.progressSegments.insert(progress.segmentId, progress);
    rebuildStreamWorkRows();
}

void AiChatConversationWidget::onAiReplyMessageRemoved(const QString& conversationId,
                                                       const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId) {
        cancelScheduledThinkingPlaceholder();
        if (QPointer<QVariantAnimation> running = m_traceExpansionAnimations.value(messageId)) {
            running->stop();
            running->deleteLater();
            m_traceExpansionAnimations.remove(messageId);
        }
        m_messageTraces.remove(messageId);
        m_expandedTraceMessageIds.remove(messageId);
        m_collapsingTraceMessageIds.remove(messageId);
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
        }
        m_messageModel->removeMessageWithAdjacentTransientRows(messageId);
        m_messageView->clearTextSelection();
        m_messageView->scrollToBottomIfLocked();
        requestContextUsage();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyThinkingChanged(const QString& conversationId, bool active)
{
    if (active) {
        Q_UNUSED(conversationId)
    } else {
        finishThinkingPlaceholder(conversationId);
    }
}

void AiChatConversationWidget::onAiReplyStreamStatusChanged(const QString& conversationId,
                                                            const AiChatStreamStatus& status)
{
    if (m_currentConversation.conversationId != conversationId) {
        return;
    }

    const QString phase = status.phase.trimmed().toLower();
    if (phase == QStringLiteral("answering")) {
        if (status.status.trimmed().toLower() == QStringLiteral("running") &&
                hasThinkingPlaceholder()) {
            finishThinkingPlaceholder(conversationId);
        }
        return;
    }

    const bool toolPhase = phase == QStringLiteral("searching") ||
            phase == QStringLiteral("reading") ||
            phase == QStringLiteral("tool");
    if (!toolPhase) {
        if (!status.active) {
            finishThinkingPlaceholder(conversationId);
        } else if (phase == QStringLiteral("thinking") &&
                   !m_streamHasAnswerText &&
                   m_streamWorkSteps.isEmpty()) {
            showOrUpdateThinkingPlaceholder(conversationId,
                                            QStringLiteral("正在思考"),
                                            false,
                                            true);
        }
        return;
    }

    if (status.stepId.isEmpty()) {
        return;
    }

    StreamWorkStep& step = m_streamWorkSteps[status.stepId];
    if (step.stepId.isEmpty()) {
        step.stepId = status.stepId;
        step.encounterOrder = m_nextWorkEncounterOrder++;
    }
    if (status.sequence > 0) {
        step.sequence = status.sequence;
    }
    step.toolStatus = status;
    step.hasToolStatus = true;
    rebuildStreamWorkRows();
}

void AiChatConversationWidget::onAiReplyFinished(const QString& conversationId, const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId) {
        cancelScheduledThinkingPlaceholder();
        finishThinkingPlaceholder(conversationId);
        m_messageModel->deactivateAllThinkingMessages();
        m_thinkingAnimationTimer->stop();
        if (m_controller) {
            m_controller->clearConversationUnreadDot(conversationId);
        }
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
            m_messageView->refreshMessageLayout();
        }
        m_inputBar->setStreaming(false);
        if (m_streamingNotifierHeld && shouldShowNewMessageNotifier()) {
            m_newMessageNotifierRevealedByDownScroll = true;
        }
        m_streamingNotifierHeld = false;
        m_thinkingMessageId.clear();
        resetStreamPresentation();
        requestContextUsage();
        updateNewMessageNotifier();
    }
}

void AiChatConversationWidget::onAiReplyCanceled(const QString& conversationId, const QString& messageId)
{
    if (m_currentConversation.conversationId == conversationId || m_currentConversation.conversationId.isEmpty()) {
        cancelScheduledThinkingPlaceholder();
        finishThinkingPlaceholder(conversationId);
        for (StreamWorkStep& step : m_streamWorkSteps) {
            for (AiChatProgressSegment& progress : step.progressSegments) {
                progress.complete = true;
            }
            if (step.hasToolStatus &&
                    step.toolStatus.status.trimmed().toLower() == QStringLiteral("running")) {
                step.toolStatus.status = QStringLiteral("failed");
                step.toolStatus.message = QStringLiteral("已取消");
            }
        }
        rebuildStreamWorkRows();
        m_messageModel->deactivateAllThinkingMessages();
        m_thinkingAnimationTimer->stop();
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
            m_messageView->refreshMessageLayout();
        }
        m_inputBar->setStreaming(false);
        if (m_streamingNotifierHeld && shouldShowNewMessageNotifier()) {
            m_newMessageNotifierRevealedByDownScroll = true;
        }
        m_streamingNotifierHeld = false;
        m_thinkingMessageId.clear();
        resetStreamPresentation();
    }
    updateNewMessageNotifier();
}

void AiChatConversationWidget::onAiReplyFailed(const QString& conversationId,
                                               const QString& messageId,
                                               const QString& message)
{
    if (m_currentConversation.conversationId == conversationId || m_currentConversation.conversationId.isEmpty()) {
        cancelScheduledThinkingPlaceholder();
        finishThinkingPlaceholder(conversationId);
        for (StreamWorkStep& step : m_streamWorkSteps) {
            for (AiChatProgressSegment& progress : step.progressSegments) {
                progress.complete = true;
            }
            if (step.hasToolStatus &&
                    step.toolStatus.status.trimmed().toLower() == QStringLiteral("running")) {
                step.toolStatus.status = QStringLiteral("failed");
                step.toolStatus.message = QStringLiteral("已中断");
            }
        }
        rebuildStreamWorkRows();
        m_messageModel->deactivateAllThinkingMessages();
        m_thinkingAnimationTimer->stop();
        if (m_messageView->messageDelegate()->streamingMessageId() == messageId) {
            m_messageView->messageDelegate()->setStreamingMessageId(QString());
            m_messageView->refreshMessageLayout();
        }
        m_inputBar->setStreaming(false);
        m_thinkingMessageId.clear();
        resetStreamPresentation();
        if (!message.trimmed().isEmpty()) {
            GlobalNotification::showFailure(this, message.trimmed());
        }
    }
}

void AiChatConversationWidget::onConversationMessagesLoaded(int requestId,
                                                            const QString& conversationId,
                                                            const QVector<AiChatMessage>& messages)
{
    if (requestId != m_pendingMessagesRequestId ||
            conversationId != m_pendingMessagesConversationId ||
            conversationId != m_currentConversation.conversationId) {
        Q_UNUSED(messages)
        return;
    }

    m_pendingMessagesRequestId = 0;
    m_pendingMessagesConversationId.clear();
    m_messageView->setUpdatesEnabled(false);
    cancelScheduledThinkingPlaceholder();
    m_thinkingMessageId.clear();
    resetStreamPresentation();
    m_thinkingAnimationTimer->stop();
    stopTraceExpansionAnimations();
    m_messageTraces.clear();
    m_expandedTraceMessageIds.clear();
    m_collapsingTraceMessageIds.clear();
    m_messageModel->setMessages(messages);
    for (const AiChatMessage& message : messages) {
        if (message.trace.isValid()) {
            installTraceRows(message);
        }
    }
    if (m_controller && m_controller->activeStreamConversationId() == conversationId) {
        m_messageView->messageDelegate()->setStreamingMessageId(m_controller->activeStreamMessageId());
        m_inputBar->setStreaming(true, false);
    } else {
        m_messageView->messageDelegate()->setStreamingMessageId(QString());
        m_inputBar->setStreaming(false);
    }
    m_messageView->jumpToBottom();
    m_messageView->setUpdatesEnabled(true);
    m_messageView->viewport()->update();
    requestContextUsage();
}

void AiChatConversationWidget::onContextUsageLoaded(int requestId,
                                                    const AiChatContextUsageRequest& request,
                                                    const AiChatContextUsage& usage)
{
    Q_UNUSED(requestId)
    Q_UNUSED(request)
    Q_UNUSED(usage)
    m_pendingContextUsageRequestId = 0;
    m_pendingContextUsageConversationId.clear();
    if (m_inputBar) {
        m_inputBar->setContextUsageVisible(false);
    }
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
    return m_controller &&
            m_controller->hasActiveAiReplyStream() &&
            m_controller->activeStreamConversationId() == m_currentConversation.conversationId;
}
