
/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "AiChatApplication.h"

#include "AiChatWindow.h"
#include <QPainter>

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

AiChatApplication::AiChatApplication(QWidget* parent) : QWidget(parent), m_leftPane(new LeftPane(this)), m_defaultPage(new DefaultPage(this)), m_splitter(new QSplitter(Qt::Horizontal, this)), m_rightPane(new QStackedWidget(this)) {
    m_splitter->addWidget(m_leftPane);
    m_rightPane->addWidget(m_defaultPage);
    m_splitter->addWidget(m_rightPane);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setSizes({250, width() - 250});
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);
    connect(m_leftPane->chatList(), &AiChatListWidget::chatItemClicked, this, &AiChatApplication::onChatItemClicked);
    connect(m_leftPane->chatList(), &AiChatListWidget::chatItemRenamed, this, &AiChatApplication::onChatItemRenamed);
    connect(m_leftPane->chatList(), &AiChatListWidget::chatItemDeleted, this, &AiChatApplication::onChatItemDeleted);
}

void AiChatApplication::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_splitter->setGeometry(0, 0, width(), height());
}

void AiChatApplication::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::white);
}

void AiChatApplication::onChatItemClicked(AiChatListItem* item) {
    QString title = item->title();

    if (!m_chatWindows.contains(title)) {
        auto* chatWindow = new AiChatWindow(this);

        m_rightPane->addWidget(chatWindow);
        m_chatWindows[title] = chatWindow;
    }
    m_rightPane->setCurrentWidget(m_chatWindows[title]);
}

void AiChatApplication::onChatItemRenamed(AiChatListItem* item) {
    // 处理重命名逻辑
}

void AiChatApplication::onChatItemDeleted(AiChatListItem* item) {
    QString title = item->title();

    if (m_chatWindows.contains(title)) {
        auto* chatWindow = m_chatWindows.take(title);

        m_rightPane->removeWidget(chatWindow);
        delete chatWindow;
    }
}
