#include "SystemWindow.h"

#include "shared/theme/ThemeManager.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWindow>

#ifdef Q_OS_WIN
#include "platform/windows/WindowsWindowControlButton.h"

#include <QAbstractButton>
#include <QStyle>
#endif

#ifdef Q_OS_MACOS
#include "platform/macos/MacWindowBridge_p.h"
#endif

namespace {
QPoint eventGlobalPos(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
}

SystemWindow::SystemWindow(QWidget* parent)
    : QWidget(parent)
    , m_backdropColor(ThemeManager::instance().color(ThemeColor::WindowBackdropTint))
{
#ifdef Q_OS_WIN
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

    HMODULE user32Handle = GetModuleHandleW(L"user32.dll");
    if (user32Handle) {
        m_setWindowCompositionAttribute = reinterpret_cast<PFN_SetWindowCompositionAttribute>(
                GetProcAddress(user32Handle, "SetWindowCompositionAttribute"));
    }
#elif !defined(Q_OS_MACOS)
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    if (qApp) {
        qApp->installEventFilter(this);
        m_appEventFilterInstalled = true;
    }
#endif

    setAttribute(Qt::WA_TranslucentBackground);
    updateWindowMargins();
}

SystemWindow::~SystemWindow()
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    if (m_appEventFilterInstalled && qApp) {
        qApp->removeEventFilter(this);
    }
#endif
}

void SystemWindow::setDragTitleBar(QWidget* titleBar)
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    if (m_titleBar) {
        m_titleBar->removeEventFilter(this);
    }
#endif

    m_titleBar = titleBar;

#ifdef Q_OS_MACOS
    if (m_titleBar) {
        m_titleBar->installEventFilter(this);
    }
#elif !defined(Q_OS_WIN)
    if (m_titleBar) {
        m_titleBar->installEventFilter(this);
    }
#endif
}

#ifdef Q_OS_WIN
void SystemWindow::setSystemMaximizeButton(QWidget* button)
{
    m_systemMaximizeButton = button;
}

void SystemWindow::toggleSystemMaximized()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ::SendMessageW(hwnd, WM_SYSCOMMAND, isMaximized() ? SC_RESTORE : SC_MAXIMIZE, 0);
}
#endif

void SystemWindow::setBackdropColor(const QColor& color)
{
    m_backdropColor = color;
    if (!m_platformChromeReady) {
        return;
    }

#ifdef Q_OS_WIN
    applyWindowsBackdrop();
#elif defined(Q_OS_MACOS)
    refreshPlatformMetrics();
#endif
}

void SystemWindow::setCompactTrafficLightsEnabled(bool enabled)
{
    if (m_compactTrafficLightsEnabled == enabled) {
        return;
    }

    m_compactTrafficLightsEnabled = enabled;
#ifdef Q_OS_MACOS
    if (m_platformChromeReady) {
        refreshPlatformMetrics();
    }
#endif
}

bool SystemWindow::usesSystemTitleButtons() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

bool SystemWindow::nativeEvent(const QByteArray& eventType, void* message, qint64* result)
{
#ifdef Q_OS_WIN
    MSG* msg = reinterpret_cast<MSG*>(message);
    switch (msg->message) {
        case WM_GETMINMAXINFO: {
            auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            HMONITOR monitor = ::MonitorFromWindow(reinterpret_cast<HWND>(winId()), MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo = {};
            monitorInfo.cbSize = sizeof(monitorInfo);
            if (::GetMonitorInfoW(monitor, &monitorInfo)) {
                const RECT workArea = monitorInfo.rcWork;
                const RECT monitorArea = monitorInfo.rcMonitor;
                minMaxInfo->ptMaxPosition.x = workArea.left - monitorArea.left;
                minMaxInfo->ptMaxPosition.y = workArea.top - monitorArea.top;
                minMaxInfo->ptMaxSize.x = workArea.right - workArea.left;
                minMaxInfo->ptMaxSize.y = workArea.bottom - workArea.top;
            }
            *result = 0;
            return true;
        }

        case WM_NCCALCSIZE:
            *result = 0;
            return true;

        case WM_NCHITTEST: {
            const LONG globalX = GET_X_LPARAM(msg->lParam);
            const LONG globalY = GET_Y_LPARAM(msg->lParam);
            const QPoint globalPos(globalX, globalY);

            if (isSystemMaximizeButtonHit(globalPos)) {
                *result = HTMAXBUTTON;
                return true;
            }

            const int resizeHit = hitTestResize(globalPos);
            if (resizeHit) {
                *result = resizeHit;
                return true;
            }

            if (isDragRegion(globalPos)) {
                *result = HTCAPTION;
                return true;
            }

            return false;
        }

        case WM_NCMOUSEMOVE: {
            if (msg->wParam == HTMAXBUTTON) {
                setSystemMaximizeButtonState(true, m_systemMaximizeButtonPressed);
                TRACKMOUSEEVENT trackEvent = {};
                trackEvent.cbSize = sizeof(trackEvent);
                trackEvent.dwFlags = TME_LEAVE | TME_NONCLIENT;
                trackEvent.hwndTrack = reinterpret_cast<HWND>(winId());
                ::TrackMouseEvent(&trackEvent);
                break;
            }
            setSystemMaximizeButtonState(false, false);
            break;
        }

        case WM_NCMOUSELEAVE:
            setSystemMaximizeButtonState(false, false);
            break;

        case WM_NCLBUTTONDOWN:
            if (msg->wParam == HTMAXBUTTON) {
                setSystemMaximizeButtonState(true, true);
                *result = 0;
                return true;
            }
            break;

        case WM_NCLBUTTONUP:
            if (m_systemMaximizeButtonPressed) {
                const LONG globalX = GET_X_LPARAM(msg->lParam);
                const LONG globalY = GET_Y_LPARAM(msg->lParam);
                const bool stillOnButton = isSystemMaximizeButtonHit(QPoint(globalX, globalY));
                setSystemMaximizeButtonState(stillOnButton, false);
                if (stillOnButton) {
                    toggleSystemMaximized();
                }
                *result = 0;
                return true;
            }
            break;

        default:
            return QWidget::nativeEvent(eventType, message, result);
    }

    return QWidget::nativeEvent(eventType, message, result);
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return QWidget::nativeEvent(eventType, message, result);
#endif
}

bool SystemWindow::eventFilter(QObject* watched, QEvent* event)
{
#ifdef Q_OS_MACOS
    if (watched == m_titleBar && event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && isDragRegion(eventGlobalPos(mouseEvent))) {
            if (MacWindowBridge::performWindowZoom(this)) {
                event->accept();
                return true;
            }
        }
    }

    if (watched == m_titleBar && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && isDragRegion(eventGlobalPos(mouseEvent))) {
            if (MacWindowBridge::startWindowDrag(this)) {
                event->accept();
                return true;
            }
        }
    }
#elif !defined(Q_OS_WIN)
    if (!isManagedWidget(watched)) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
            if (QWindow* handle = windowHandle()) {
                const Qt::Edges edges = hitTestResize(eventGlobalPos(mouseEvent));
                if (edges != Qt::Edges() && handle->startSystemResize(edges)) {
                    event->accept();
                    return true;
                }
            }
        }
    }

    if (watched == m_titleBar) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && isDragRegion(eventGlobalPos(mouseEvent))) {
                isMaximized() ? showNormal() : showMaximized();
                event->accept();
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && isDragRegion(eventGlobalPos(mouseEvent))) {
                if (QWindow* handle = windowHandle(); handle && handle->startSystemMove()) {
                    event->accept();
                    return true;
                }
            }
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif

    return QWidget::eventFilter(watched, event);
}

bool SystemWindow::event(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        const bool maximized = isMaximized();
        if (maximized != m_isMaximized) {
            m_isMaximized = maximized;
            updateWindowMargins();
#ifdef Q_OS_WIN
            if (m_systemMaximizeButton) {
                m_systemMaximizeButton->update();
            }
#endif
        }
    }

    return QWidget::event(event);
}

void SystemWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensurePlatformChrome();
}

void SystemWindow::ensurePlatformChrome()
{
    if (m_platformChromeReady) {
        return;
    }

    winId();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    ::SetWindowLongPtrW(hwnd,
                        GWL_STYLE,
                        style | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);
    ::SetWindowPos(hwnd,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    UINT cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &cornerPreference,
                          sizeof(cornerPreference));

    applyWindowsBackdrop();
    m_topInset = 0;
    m_leadingInset = 0;
#elif defined(Q_OS_MACOS)
    refreshPlatformMetrics();
#else
    m_topInset = 0;
    m_leadingInset = 0;
#endif

    m_platformChromeReady = true;
    updateWindowMargins();
}

void SystemWindow::refreshPlatformMetrics()
{
#ifdef Q_OS_MACOS
    const auto metrics = MacWindowBridge::configureWindow(this,
                                                          m_backdropColor,
                                                          m_compactTrafficLightsEnabled);
    m_topInset = metrics.topInset;
    m_leadingInset = metrics.leadingInset;
#endif
}

void SystemWindow::updateWindowMargins()
{
#ifdef Q_OS_WIN
    const int margin = isMaximized() ? SYSTEM_WINDOW_RESIZE_BORDER : 0;
    setContentsMargins(margin, margin, margin, margin);
#else
    setContentsMargins(0, 0, 0, 0);
#endif
}

#ifdef Q_OS_WIN
void SystemWindow::applyWindowsBackdrop()
{
    if (!m_setWindowCompositionAttribute) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(winId());
    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.AccentFlags = 0x20 | 0x40 | 0x80 | 0x100;
    policy.GradientColor = static_cast<DWORD>(m_backdropColor.rgba());

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    m_setWindowCompositionAttribute(hwnd, &data);
}

int SystemWindow::hitTestResize(const QPoint& globalPos) const
{
    int result = 0;

    RECT windowRect;
    GetWindowRect(reinterpret_cast<HWND>(winId()), &windowRect);

    const bool resizeWidth = minimumWidth() != maximumWidth();
    const bool resizeHeight = minimumHeight() != maximumHeight();

    if (resizeWidth) {
        if (globalPos.x() > windowRect.left && globalPos.x() < windowRect.left + SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTLEFT;
        }
        if (globalPos.x() < windowRect.right && globalPos.x() >= windowRect.right - SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTRIGHT;
        }
    }

    if (resizeHeight) {
        if (globalPos.y() >= windowRect.top && globalPos.y() < windowRect.top + SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTTOP;
        }
        if (globalPos.y() <= windowRect.bottom && globalPos.y() > windowRect.bottom - SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTBOTTOM;
        }
    }

    if (resizeWidth && resizeHeight) {
        if (globalPos.x() >= windowRect.left && globalPos.x() < windowRect.left + SYSTEM_WINDOW_RESIZE_BORDER
            && globalPos.y() >= windowRect.top && globalPos.y() < windowRect.top + SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTTOPLEFT;
        }
        if (globalPos.x() <= windowRect.right && globalPos.x() > windowRect.right - SYSTEM_WINDOW_RESIZE_BORDER
            && globalPos.y() >= windowRect.top && globalPos.y() < windowRect.top + SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTTOPRIGHT;
        }
        if (globalPos.x() >= windowRect.left && globalPos.x() < windowRect.left + SYSTEM_WINDOW_RESIZE_BORDER
            && globalPos.y() <= windowRect.bottom && globalPos.y() > windowRect.bottom - SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTBOTTOMLEFT;
        }
        if (globalPos.x() <= windowRect.right && globalPos.x() > windowRect.right - SYSTEM_WINDOW_RESIZE_BORDER
            && globalPos.y() <= windowRect.bottom && globalPos.y() > windowRect.bottom - SYSTEM_WINDOW_RESIZE_BORDER) {
            result = HTBOTTOMRIGHT;
        }
    }

    return result;
}

bool SystemWindow::isSystemMaximizeButtonHit(const QPoint& globalPos) const
{
    if (!m_systemMaximizeButton || !m_systemMaximizeButton->isVisible() || !m_systemMaximizeButton->isEnabled()) {
        return false;
    }

    if (minimumWidth() == maximumWidth() && minimumHeight() == maximumHeight()) {
        return false;
    }

    const qreal dpr = devicePixelRatioF();
    const QPoint localPos = m_systemMaximizeButton->mapFromGlobal(QPoint(globalPos.x() / dpr, globalPos.y() / dpr));
    return m_systemMaximizeButton->rect().contains(localPos);
}

void SystemWindow::setSystemMaximizeButtonState(bool hovered, bool pressed)
{
    if (!m_systemMaximizeButton) {
        m_systemMaximizeButtonHovered = false;
        m_systemMaximizeButtonPressed = false;
        return;
    }

    if (auto* button = qobject_cast<WindowsWindowControlButton*>(m_systemMaximizeButton.data())) {
        m_systemMaximizeButtonHovered = hovered;
        m_systemMaximizeButtonPressed = pressed;
        button->setNativeState(hovered, pressed);
        return;
    }

    if (m_systemMaximizeButtonHovered != hovered) {
        m_systemMaximizeButtonHovered = hovered;
        m_systemMaximizeButton->setProperty("nativeHover", hovered);
        if (m_systemMaximizeButton->style()) {
            m_systemMaximizeButton->style()->unpolish(m_systemMaximizeButton);
            m_systemMaximizeButton->style()->polish(m_systemMaximizeButton);
        }
        m_systemMaximizeButton->update();

        QEvent event(hovered ? QEvent::Enter : QEvent::Leave);
        QApplication::sendEvent(m_systemMaximizeButton, &event);
    }

    if (auto* button = qobject_cast<QAbstractButton*>(m_systemMaximizeButton.data())) {
        button->setDown(pressed);
    }
    m_systemMaximizeButtonPressed = pressed;
}

bool SystemWindow::isDragRegion(const QPoint& globalPos) const
{
    if (!m_titleBar) {
        return false;
    }

    const qreal dpr = devicePixelRatioF();
    const QPoint localPos = m_titleBar->mapFromGlobal(QPoint(globalPos.x() / dpr, globalPos.y() / dpr));
    if (!m_titleBar->rect().contains(localPos)) {
        return false;
    }

    return m_titleBar->childAt(localPos) == nullptr;
}
#else
Qt::Edges SystemWindow::hitTestResize(const QPoint& globalPos) const
{
    Qt::Edges edges;
    const QRect windowRect = frameGeometry();

    const bool resizeWidth = minimumWidth() != maximumWidth();
    const bool resizeHeight = minimumHeight() != maximumHeight();

    if (resizeWidth) {
        if (globalPos.x() >= windowRect.left() && globalPos.x() < windowRect.left() + SYSTEM_WINDOW_RESIZE_BORDER) {
            edges |= Qt::LeftEdge;
        } else if (globalPos.x() <= windowRect.right()
                   && globalPos.x() > windowRect.right() - SYSTEM_WINDOW_RESIZE_BORDER) {
            edges |= Qt::RightEdge;
        }
    }

    if (resizeHeight) {
        if (globalPos.y() >= windowRect.top() && globalPos.y() < windowRect.top() + SYSTEM_WINDOW_RESIZE_BORDER) {
            edges |= Qt::TopEdge;
        } else if (globalPos.y() <= windowRect.bottom()
                   && globalPos.y() > windowRect.bottom() - SYSTEM_WINDOW_RESIZE_BORDER) {
            edges |= Qt::BottomEdge;
        }
    }

    return edges;
}

bool SystemWindow::isDragRegion(const QPoint& globalPos) const
{
    if (!m_titleBar) {
        return false;
    }

    const QPoint localPos = m_titleBar->mapFromGlobal(globalPos);
    if (!m_titleBar->rect().contains(localPos)) {
        return false;
    }

    return m_titleBar->childAt(localPos) == nullptr;
}

bool SystemWindow::isManagedWidget(const QObject* watched) const
{
    const QWidget* widget = qobject_cast<const QWidget*>(watched);
    if (!widget) {
        return false;
    }

    return widget == this || isAncestorOf(const_cast<QWidget*>(widget));
}
#endif
