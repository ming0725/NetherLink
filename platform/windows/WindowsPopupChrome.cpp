#include "platform/windows/WindowsPopupChrome.h"

#ifdef Q_OS_WIN

#include <QWidget>

#include <Windows.h>
#include <dwmapi.h>

namespace {

constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr DWORD kDwmwaBorderColor = 34;
constexpr int kDwmwcpRoundSmall = 3;
constexpr COLORREF kDwmwaColorNone = 0xFFFFFFFE;

bool isCompositionEnabled()
{
    BOOL enabled = FALSE;
    return SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled;
}

} // namespace


namespace WindowsPopupChrome {

void applyModernShadow(QWidget* widget)
{
    if (!widget || !isCompositionEnabled()) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) {
        return;
    }

    const DWMNCRENDERINGPOLICY renderingPolicy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd,
                          DWMWA_NCRENDERING_POLICY,
                          &renderingPolicy,
                          sizeof(renderingPolicy));

    const int cornerPreference = kDwmwcpRoundSmall;
    DwmSetWindowAttribute(hwnd,
                          kDwmwaWindowCornerPreference,
                          &cornerPreference,
                          sizeof(cornerPreference));

    const COLORREF borderColor = kDwmwaColorNone;
    DwmSetWindowAttribute(hwnd, kDwmwaBorderColor, &borderColor, sizeof(borderColor));

    const MARGINS shadowMargins = {2, 2, 1, 2};
    DwmExtendFrameIntoClientArea(hwnd, &shadowMargins);

    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
}

} // namespace WindowsPopupChrome

#endif
