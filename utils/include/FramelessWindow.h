
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

// #pragma comment(lib,"dwmapi.lib")

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QDialog>
#include <QWidget>

#include <dwmapi.h>

#include <windowsx.h>

// #include <WinUser.h>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class FramelessWindow : public QWidget {
    Q_OBJECT

    public:
        explicit FramelessWindow(QWidget* parent = nullptr);

    protected:
        bool nativeEvent(const QByteArray& eventType, void* message, qint64* result) override;

        void mousePressEvent(QMouseEvent* event) override;

        void mouseMoveEvent(QMouseEvent* event) override;

        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        int adjustResizeWindow(const QPoint& pos);

        bool m_dragging {false};
        QPoint m_dragPos;
};
