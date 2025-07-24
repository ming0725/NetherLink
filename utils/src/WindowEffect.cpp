
/* include ---------------------------------------------------------------- 80 */ /* ! ----------------------------- 120 */

#include "WindowEffect.h"
#include <QDebug>

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

WindowEffect::WindowEffect() : m_pSetWindowCompAttr(nullptr) {
    // Dynamically load the (undocumented) API
    HMODULE hUser = GetModuleHandleW(L"user32.dll");

    if (hUser) {
        m_pSetWindowCompAttr = reinterpret_cast <PFN_SetWindowCompositionAttribute>(GetProcAddress(hUser, "SetWindowCompositionAttribute"));
    }

    if (!m_pSetWindowCompAttr) {
        qWarning() << "Failed to load SetWindowCompositionAttribute";
    }
}

void WindowEffect::setAcrylicEffect(HWND hWnd, const QColor& color, bool enableShadow, DWORD animId) {
    if (!m_pSetWindowCompAttr)
        return;
    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;

    // Set shadow flags if requested (0x20|0x40|0x80|0x100) matches Python code
    policy.AccentFlags = enableShadow ? (0x20 | 0x40 | 0x80 | 0x100) : 0;

    // Use Qt’s ARGB directly (0xAARRGGBB)
    policy.GradientColor = static_cast <DWORD>(color.rgba());
    policy.AnimationId = animId;

    WINDOWCOMPOSITIONATTRIBDATA data = {};

    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    m_pSetWindowCompAttr(hWnd, &data);
}

void WindowEffect::setAeroEffect(HWND hWnd) {
    if (!m_pSetWindowCompAttr)
        return;
    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_BLURBEHIND;

    // other fields zero
    WINDOWCOMPOSITIONATTRIBDATA data = {};

    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    m_pSetWindowCompAttr(hWnd, &data);
}

void WindowEffect::moveWindow(HWND hWnd) {
    // Release mouse capture & send WM_SYSCOMMAND to drag the title bar
    ReleaseCapture();
    SendMessageW(hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}
