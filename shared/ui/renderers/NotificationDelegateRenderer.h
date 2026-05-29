#pragma once

#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QRect>
#include <QString>

class QPainter;

namespace NotificationDelegateRenderer {

struct CardMetrics {
    int cardMarginH = 16;
    int cardMarginV = 6;
    int cardPaddingH = 16;
    int cardPaddingV = 16;
    int cardRadius = 8;
    int avatarSize = 48;
    int avatarToContentGap = 12;
    int buttonWidth = 54;
    int buttonHeight = 26;
    int buttonRadius = 6;
    int buttonGap = 8;
};

QString formatTime(const QDateTime& time);

QRect cardRect(const QRect& optionRect, const CardMetrics& metrics);
QRect avatarRect(const QRect& card, const CardMetrics& metrics);
QRect contentBounds(const QRect& card, const CardMetrics& metrics);
QRect acceptButtonRect(const QRect& card, int y, const CardMetrics& metrics);
QRect rejectButtonRect(const QRect& card, int y, const CardMetrics& metrics);
QRect actionRect(const QRect& acceptRect, const QRect& rejectRect);

void drawCard(QPainter* painter, const QRect& card, const CardMetrics& metrics);
void drawAvatar(QPainter* painter,
                const QRect& rect,
                const QString& avatarPath,
                int avatarSize);
void drawPendingDots(QPainter* painter, const QRect& rect, const QColor& color);
void drawActionButtons(QPainter* painter,
                       const QRect& acceptRect,
                       const QRect& rejectRect,
                       int hoveredButton,
                       const QFont& font,
                       const CardMetrics& metrics);

} // namespace NotificationDelegateRenderer
