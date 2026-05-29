#pragma once

#include <QColor>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QString>

#include "shared/services/AppFonts.h"

class QPainter;
class QRect;

struct BadgeLayout {
    QSize size;
    QString text;
    bool drawIcon = false;
    bool invertIcon = false;
    QColor backgroundColor;
    QColor textColor;
};

namespace BadgeRenderer {

constexpr int kBadgeHeight = 16;
constexpr int kBadgeHorizontalPadding = 4;
constexpr int kDotBadgeSize = 7;

inline QFont badgeFont()
{
    return AppFonts::applicationPixelSizedFont(11, true);
}

inline QFontMetrics badgeMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(11, true);
}

// Compute badge layout from unread count and do-not-disturb state.
// Set selected=true for selected item rendering.
// Set isDark=true for dark theme.
BadgeLayout layoutForUnreadCount(int unreadCount, bool doNotDisturb,
                                 bool selected, bool isDark);

// Small red indicator without text, used where a full unread count badge is too heavy.
BadgeLayout layoutForUnreadDot();

// Draw a badge. Handles both icon mode (drawIcon) and text count mode.
void drawBadge(QPainter* painter, const QRect& rect,
               const BadgeLayout& layout, bool selected);

} // namespace BadgeRenderer
