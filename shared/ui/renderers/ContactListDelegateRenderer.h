#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QRect>
#include <QString>

class QPainter;

namespace ContactListDelegateRenderer {

struct PaintColors {
    bool dark = false;
    QColor panelBackground;
    QColor listHover;
    QColor listSelected;
    QColor noticeSelectedBackground;
    QColor primaryText;
    QColor secondaryText;
    QColor tertiaryText;
    QColor selectedText;
    QColor selectedSecondaryText;
    QColor imagePlaceholder;
};

PaintColors currentPaintColors();

void drawHoverOrSelectedOverlay(QPainter* painter,
                                const QRect& rect,
                                bool selected,
                                bool hovered,
                                const PaintColors& colors);

void drawRightChevron(QPainter* painter,
                      const QPointF& center,
                      const QColor& color,
                      qreal penWidth = 1.3);

void drawDisclosureArrow(QPainter* painter,
                         const QRect& rect,
                         qreal progress,
                         const QColor& color);

void drawNoticeRow(QPainter* painter,
                   const QRect& rect,
                   const QString& title,
                   int unreadCount,
                   bool selected,
                   bool hovered,
                   int rightPadding,
                   int arrowCenterYOffset,
                   const QFont& titleFont,
                   const QFontMetrics& titleMetrics,
                   const PaintColors& colors);

void drawSectionHeaderRow(QPainter* painter,
                          const QRect& rect,
                          const QString& title,
                          const QString& countText,
                          qreal progress,
                          bool hovered,
                          int leftPadding,
                          int rightPadding,
                          int arrowSize,
                          int arrowYOffset,
                          const QFont& titleFont,
                          const QFontMetrics& titleMetrics,
                          const QFont& countFont,
                          const QFontMetrics& countMetrics,
                          const PaintColors& colors);

} // namespace ContactListDelegateRenderer
