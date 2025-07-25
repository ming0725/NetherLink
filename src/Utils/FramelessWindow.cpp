/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMouseEvent>

#include "Utils/FramelessWindow.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
constexpr int RESIZE_WINDOW_WIDTH = 8;

// 如果 DWMWA_WINDOW_CORNER_PREFERENCE 未定义，则手动声明
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE

/* enum ------------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// 枚举值定义参考 Windows 11 SDK dwmapi.h
enum DWM_WINDOW_CORNER_PREFERENCE {
    DWMWCP_DEFAULT = 0,     // 系统决定是否圆角
    DWMWCP_DONOTROUND = 1, // 不圆角
    DWMWCP_ROUND = 2, // 常规圆角
    DWMWCP_ROUNDSMALL = 3 // 小半径圆角
};

static const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;  // 窗口圆角策略标识符
#endif

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

FramelessWindow::FramelessWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);

    HWND hwnd = HWND(winId());
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX);
    UINT preference = DWMWCP_ROUND;

    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
}

bool FramelessWindow::nativeEvent(const QByteArray& eventType, void* message, qint64* result) {
    MSG* msg = reinterpret_cast <MSG*>(message);

    switch (msg->message) {
        case WM_NCCALCSIZE:
        {
            *result = 0;

            return (true);
        }
        case WM_NCHITTEST:
        {
            long x = GET_X_LPARAM(msg->lParam);
            long y = GET_Y_LPARAM(msg->lParam);
            QPoint mouse_pos(x, y);

            *result = this->adjustResizeWindow(mouse_pos);

            if (0 != *result)
                return (true);
            return (false);
        }
        default:
            return (QWidget::nativeEvent(eventType, message, result));
    }
}

int FramelessWindow::adjustResizeWindow(const QPoint& pos) {
    int result = 0;
    RECT winrect;

    GetWindowRect(HWND(this->winId()), &winrect);

    int mouse_x = pos.x();
    int mouse_y = pos.y();
    bool resizeWidth = this->minimumWidth() != this->maximumWidth();
    bool resizeHieght = this->minimumHeight() != this->maximumHeight();

    if (resizeWidth) {
        if ((mouse_x > winrect.left) && (mouse_x < winrect.left + RESIZE_WINDOW_WIDTH))
            result = HTLEFT;


        if ((mouse_x < winrect.right) && (mouse_x >= winrect.right - RESIZE_WINDOW_WIDTH))
            result = HTRIGHT;
    }

    if (resizeHieght) {
        if ((mouse_y < winrect.top + RESIZE_WINDOW_WIDTH) && (mouse_y >= winrect.top))
            result = HTTOP;


        if ((mouse_y <= winrect.bottom) && (mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH))
            result = HTBOTTOM;
    }

    if (resizeWidth && resizeHieght) {
        // topleft corner
        if ((mouse_x >= winrect.left) && (mouse_x < winrect.left + RESIZE_WINDOW_WIDTH) && (mouse_y >= winrect.top) && (mouse_y < winrect.top + RESIZE_WINDOW_WIDTH)) {
            result = HTTOPLEFT;
        }

        // topRight corner
        if ((mouse_x <= winrect.right) && (mouse_x > winrect.right - RESIZE_WINDOW_WIDTH) && (mouse_y >= winrect.top) && (mouse_y < winrect.top + RESIZE_WINDOW_WIDTH))
            result = HTTOPRIGHT;

        // leftBottom  corner
        if ((mouse_x >= winrect.left) && (mouse_x < winrect.left + RESIZE_WINDOW_WIDTH) && (mouse_y <= winrect.bottom) && (mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH))
            result = HTBOTTOMLEFT;

        // rightbottom  corner
        if ((mouse_x <= winrect.right) && (mouse_x > winrect.right - RESIZE_WINDOW_WIDTH) && (mouse_y <= winrect.bottom) && (mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH))
            result = HTBOTTOMRIGHT;
    }
    return (result);
}

void FramelessWindow::mousePressEvent(QMouseEvent* event) {
    if ((event->button() == Qt::LeftButton) && (event->pos().y() <= 63)) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}
