#include "MainWindow.h"
#include "features/chat/ui/MessageApplication.h"
#include "features/friend/ui/FriendApplication.h"
#include "features/aichat/ui/AiChatApplication.h"
#include "features/post/ui/PostApplication.h"
#include "SettingsWindow.h"
#include "platform/windows/WindowsWindowControlButton.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/FloatingInputBar.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/theme/ThemeManager.h"
#include <QAbstractButton>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QApplication>
namespace {

constexpr int kMainWindowMinimumWidth = 820;

IconLineEdit* iconLineEditForWidget(QWidget* widget)
{
    QWidget* current = widget;
    while (current) {
        if (auto* input = qobject_cast<IconLineEdit*>(current)) {
            return input;
        }
        current = current->parentWidget();
    }
    return nullptr;
}

QWidget* focusedInnerLineEdit()
{
    if (auto* lineEdit = qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
        return lineEdit;
    }
    if (auto* textEdit = qobject_cast<QTextEdit*>(QApplication::focusWidget())) {
        return textEdit;
    }
    return nullptr;
}

InWindowPopupOverlay* popupOverlayForWidget(QWidget* widget)
{
    QWidget* current = widget;
    while (current) {
        if (auto* overlay = qobject_cast<InWindowPopupOverlay*>(current)) {
            return overlay;
        }
        current = current->parentWidget();
    }
    return nullptr;
}

bool shouldClearLineEditFocus(QWidget* watched, const QPoint& globalPos)
{
    QWidget* focusedLineEdit = focusedInnerLineEdit();
    if (!focusedLineEdit) {
        return false;
    }

    if (!watched) {
        return true;
    }

    auto* focusedInput = iconLineEditForWidget(focusedLineEdit);
    if (focusedInput) {
        const QPoint localPos = focusedInput->mapFromGlobal(globalPos);
        return !focusedInput->rect().contains(localPos);
    }

    if (auto* focusedFloatingBar = qobject_cast<FloatingInputBar*>(focusedLineEdit->parentWidget())) {
        if (watched == focusedFloatingBar || focusedFloatingBar->isAncestorOf(watched) || watched->isAncestorOf(focusedFloatingBar)) {
            const QPoint localPos = focusedFloatingBar->mapFromGlobal(globalPos);
            return !focusedFloatingBar->rect().contains(localPos);
        }
    }

    if (focusedLineEdit == watched ||
        focusedLineEdit->isAncestorOf(watched) ||
        watched->isAncestorOf(focusedLineEdit)) {
        return false;
    }

    return true;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : SystemWindow(parent)
    , stack(new QStackedWidget(this))
    , btnMinimize(nullptr)
    , btnMaximize(nullptr)
    , btnClose(nullptr)
{
    // 窗口基础设置
    setCompactTrafficLightsEnabled(true);
    resize(950, 650);
    setMinimumHeight(525);
    setMinimumWidth(kMainWindowMinimumWidth);
    setAttribute(Qt::WA_TranslucentBackground);

    updateBackdropTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateBackdropTheme();
    });

    appBar = new ApplicationBar(this);
    appBar->setFixedWidth(54);

    titleBar = new QWidget(this);
    titleBar->setFixedHeight(32);
    titleBar->setAttribute(Qt::WA_StyledBackground, false);

#ifndef Q_OS_MACOS
    btnMinimize = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Minimize, titleBar);
    btnMaximize = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Maximize, titleBar);
    btnClose = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Close, titleBar);
#ifdef Q_OS_WIN
    btnMinimize->setFixedSize(28, 28);
    btnMaximize->setFixedSize(28, 28);
    btnClose->setFixedSize(28, 28);
#else
    btnMinimize->setFixedSize(32, 32);
    btnMaximize->setFixedSize(32, 32);
    btnClose->setFixedSize(32, 32);

    auto hl = new QHBoxLayout(titleBar);
    hl->setContentsMargins(0,0,0,0);
    hl->addStretch();
    hl->addWidget(btnMinimize);
    hl->addWidget(btnMaximize);
    hl->addWidget(btnClose);
    hl->setSpacing(0);
#endif

    connect(btnMinimize, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QAbstractButton::clicked, this, [this]() {
#ifdef Q_OS_WIN
        toggleSystemMaximized();
#else
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
#endif
    });
    connect(btnClose, &QAbstractButton::clicked, this, &QWidget::close);
#ifdef Q_OS_WIN
    setSystemMaximizeButton(btnMaximize);
#endif
#endif

    setDragTitleBar(titleBar);
    qApp->installEventFilter(this);

    m_messageApp = new MessageApplication(this);
    stack->addWidget(m_messageApp);
    stack->addWidget(createPlaceholderPage());
    stack->addWidget(createPlaceholderPage());
    stack->addWidget(createPlaceholderPage());
    m_defaultPage = new DefaultPage(this);
    stack->addWidget(m_defaultPage);
    stack->setCurrentIndex(0);

    // 绑定点击信号，切换栈页
    connect(appBar, &ApplicationBar::applicationClicked,
            this, &MainWindow::onBarItemClicked);
    connect(appBar, &ApplicationBar::settingsRequested,
            this, &MainWindow::openSettingsWindow);
    connect(appBar, &ApplicationBar::appearanceSettingsRequested,
            this, &MainWindow::openAppearanceSettingsWindow);
    connect(appBar, &ApplicationBar::logoutRequested,
            this, &MainWindow::logoutRequested);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        move(available.center() - rect().center());
    }
}

MainWindow::~MainWindow()
{
    delete m_settingsWindow;
    qApp->removeEventFilter(this);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    delete m_settingsWindow;
    SystemWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    SystemWindow::showEvent(event);
    layoutWindow();
}


void MainWindow::resizeEvent(QResizeEvent* event)
{
    SystemWindow::resizeEvent(event);
    layoutWindow();

    if (m_settingsWindow) {
#ifdef Q_OS_WIN
        m_settingsWindow->setGeometry(contentsRect());
#else
        m_settingsWindow->setGeometry(0, 0, event->size().width(), event->size().height());
#endif
        m_settingsWindow->raise();
    }
}

void MainWindow::moveEvent(QMoveEvent* event)
{
    SystemWindow::moveEvent(event);
#ifdef Q_OS_MACOS
    layoutWindow();
#endif
}


void MainWindow::mousePressEvent(QMouseEvent* event)
{
    SystemWindow::mousePressEvent(event);
}


void MainWindow::onBarItemClicked(ApplicationBarItem *item)
{
    int idx = appBar->indexOfTopItem(item);
    if (idx >= 0 && idx < stack->count()) {
        ensureApplicationLoaded(idx);
        stack->setCurrentIndex(idx);
    }
}

QWidget* MainWindow::createPlaceholderPage() const
{
    auto* page = new QWidget(stack);
    page->setAutoFillBackground(true);
    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::PanelBackground));
    page->setPalette(pagePalette);
    return page;
}

void MainWindow::replaceStackPage(int index, QWidget* widget)
{
    if (!widget || index < 0 || index >= stack->count()) {
        return;
    }

    QWidget* existing = stack->widget(index);
    if (existing == widget) {
        return;
    }

    stack->removeWidget(existing);
    stack->insertWidget(index, widget);
    existing->deleteLater();
}

void MainWindow::ensureApplicationLoaded(int index)
{
    switch (index) {
    case 0:
        if (!m_messageApp) {
            m_messageApp = new MessageApplication(this);
            replaceStackPage(index, m_messageApp);
        }
        break;
    case 1:
        if (!m_friendApp) {
            m_friendApp = new FriendApplication(this);
            replaceStackPage(index, m_friendApp);
            connectFriendConversationRequests();
        }
        break;
    case 2:
        if (!m_postApp) {
            m_postApp = new PostApplication(this);
            replaceStackPage(index, m_postApp);
            m_postApp->setSystemFloatingBarsSuppressed(m_systemFloatingBarsSuppressed);
        }
        break;
    case 3:
        if (!m_aiChatApp) {
            m_aiChatApp = new AiChatApplication(this);
            replaceStackPage(index, m_aiChatApp);
        }
        break;
    default:
        break;
    }
}

void MainWindow::connectFriendConversationRequests()
{
    if (!m_friendApp) {
        return;
    }

    connect(m_friendApp, &FriendApplication::requestOpenConversation,
            this, &MainWindow::openConversationFromContacts,
            Qt::UniqueConnection);
}

void MainWindow::updateBackdropTheme()
{
    QColor backdropColor = ThemeManager::instance().color(ThemeColor::WindowBackground);
    backdropColor.setAlpha(92);
    setBackdropColor(backdropColor);
}

void MainWindow::openSettingsWindow()
{
    showSettingsWindow(false);
}

void MainWindow::openAppearanceSettingsWindow()
{
    showSettingsWindow(true);
}

void MainWindow::showSettingsWindow(bool openAppearancePage)
{
    if (m_settingsWindow) {
        setSystemFloatingBarsSuppressed(true);
        if (openAppearancePage) {
            m_settingsWindow->showAppearancePage();
        }
        m_settingsWindow->raise();
        m_settingsWindow->setFocus();
        return;
    }

    setSystemFloatingBarsSuppressed(true);
    auto* settings = new SettingsWindow(this);
    connect(settings, &SettingsWindow::closeRequested, this, &MainWindow::closeSettingsWindow);
    connect(settings, &SettingsWindow::hideAnimationFinished, this, [this, settings]() {
        if (m_settingsWindow == settings) {
            m_settingsWindow = nullptr;
        }
        setSystemFloatingBarsSuppressed(false);
    });
    connect(settings, &QObject::destroyed, this, [this, settings]() {
        if (m_settingsWindow == settings) {
            m_settingsWindow = nullptr;
            setSystemFloatingBarsSuppressed(false);
        }
    });
    m_settingsWindow = settings;
    if (openAppearancePage) {
        settings->showAppearancePage();
    }
    settings->showAnimated();
}

void MainWindow::closeSettingsWindow()
{
    if (!m_settingsWindow) {
        return;
    }

    SettingsWindow* settings = m_settingsWindow;
    disconnect(settings, &SettingsWindow::closeRequested, this, &MainWindow::closeSettingsWindow);
    settings->hideAnimated();
}

void MainWindow::setSystemFloatingBarsSuppressed(bool suppressed)
{
#ifndef Q_OS_MACOS
    Q_UNUSED(suppressed);
    return;
#else
    if (m_systemFloatingBarsSuppressed == suppressed) {
        return;
    }

    m_systemFloatingBarsSuppressed = suppressed;
    if (m_messageApp) {
        m_messageApp->setSystemFloatingBarsSuppressed(suppressed);
    }
    if (m_postApp) {
        m_postApp->setSystemFloatingBarsSuppressed(suppressed);
    }
#endif
}

void MainWindow::openConversationFromContacts(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    ensureApplicationLoaded(0);
    appBar->setCurrentTopIndex(0);
    stack->setCurrentIndex(0);
    if (m_messageApp) {
        m_messageApp->openConversationFromContact(conversationId);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonRelease) {
        if (auto* watchedWidget = qobject_cast<QWidget*>(watched)) {
            if (watchedWidget->window() != this) {
                return SystemWindow::eventFilter(watched, ev);
            }
            if (popupOverlayForWidget(watchedWidget)) {
                return SystemWindow::eventFilter(watched, ev);
            }
            auto* mouseEvent = static_cast<QMouseEvent*>(ev);
            if (ev->type() == QEvent::MouseButtonPress) {
                if (stack->currentWidget() == m_messageApp && m_messageApp) {
                    m_messageApp->handleGlobalMousePress(mouseEvent->globalPosition().toPoint());
                }
                m_pendingFocusClear = nullptr;
                if (shouldClearLineEditFocus(watchedWidget, mouseEvent->globalPosition().toPoint())) {
                    m_pendingFocusClear = QApplication::focusWidget();
                }
            } else if (ev->type() == QEvent::MouseButtonRelease) {
                if (m_pendingFocusClear && QApplication::focusWidget() == m_pendingFocusClear) {
                    m_pendingFocusClear->clearFocus();
                }
                m_pendingFocusClear = nullptr;
            }
        }
    }

    return SystemWindow::eventFilter(watched, ev);
}

void MainWindow::layoutWindow()
{
#ifdef Q_OS_WIN
    const QRect contentRect = contentsRect();
    const int x = contentRect.x();
    const int y = contentRect.y();
    const int w = contentRect.width();
    const int h = contentRect.height();
#else
    const int x = 0;
    const int y = 0;
    const int w = width();
    const int h = height();
#endif
    const bool useSystemTitleButtons = usesSystemTitleButtons();
    const int titleInset = useSystemTitleButtons ? topInset() : 0;
    const int titleBarHeight = qMax(32, titleInset);
    const int barW = useSystemTitleButtons ? qMax(54, leadingInset()) : 54;
    const int titleBarX = useSystemTitleButtons ? x : x + barW;
    const int titleBarW = useSystemTitleButtons ? w : w - barW;

    if (titleBar->height() != titleBarHeight) {
        titleBar->setFixedHeight(titleBarHeight);
    }

    appBar->setFixedWidth(barW);
    appBar->setTopInset(titleInset);
    appBar->setGeometry(x, y, barW, h);
    stack->setGeometry(x + barW, y, w - barW, h);
    titleBar->setGeometry(titleBarX, y, titleBarW, titleBar->height());
    titleBar->raise();

#ifdef Q_OS_WIN
    if (btnMinimize && btnMaximize && btnClose) {
        btnClose->setGeometry(titleBar->width() - btnClose->width(),
                              0,
                              btnClose->width(),
                              btnClose->height());
        btnMaximize->setGeometry(btnClose->x() - btnMaximize->width(),
                                 0,
                                 btnMaximize->width(),
                                 btnMaximize->height());
        btnMinimize->setGeometry(btnMaximize->x() - btnMinimize->width(),
                                 0,
                                 btnMinimize->width(),
                                 btnMinimize->height());
        btnMinimize->raise();
        btnMaximize->raise();
        btnClose->raise();
    }
#endif
}
