#include "HistoryUnreadNotifier.h"

#include "shared/services/AppFonts.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kBookmarkHeight = 36;
constexpr int kShadowLeft = 10;
constexpr int kShadowTop = 7;
constexpr int kShadowBottom = 7;
constexpr int kLeftRadius = 14;
constexpr int kLeftPadding = 13;
constexpr int kRightPadding = 14;
constexpr int kSymbolTextGap = 7;
constexpr int kArrowWidth = 12;
constexpr int kArrowHeight = 13;
constexpr int kFontPixelSize = 14;
constexpr int kFontWeight = QFont::DemiBold;

QPainterPath bookmarkPath(const QRectF& rect)
{
    const qreal radius = qMin<qreal>(kLeftRadius, rect.height() / 2.0);

    QPainterPath path;
    path.moveTo(rect.right(), rect.top());
    path.lineTo(rect.right(), rect.bottom());
    path.lineTo(rect.left() + radius, rect.bottom());
    path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - radius);
    path.lineTo(rect.left(), rect.top() + radius);
    path.quadTo(rect.left(), rect.top(), rect.left() + radius, rect.top());
    path.closeSubpath();
    return path;
}

QColor notifierBackground(bool hovered, bool pressed)
{
    if (pressed) {
        return ThemeManager::instance().color(ThemeColor::AccentPressed);
    }
    if (hovered) {
        return ThemeManager::instance().color(ThemeColor::AccentHover);
    }
    return ThemeManager::instance().color(ThemeColor::Accent);
}

} // namespace

HistoryUnreadNotifier::HistoryUnreadNotifier(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kShadowTop + kBookmarkHeight + kShadowBottom);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(10);
    shadow->setOffset(0, 2);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::NewMessageNotifierShadow));
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
            shadow->setColor(ThemeManager::instance().color(ThemeColor::NewMessageNotifierShadow));
        }
        updateSize();
        update();
    });

    QWidget::hide();
}

void HistoryUnreadNotifier::setUnreadCount(int count)
{
    const int nextCount = qMax(0, count);
    if (m_count == nextCount) {
        return;
    }

    m_count = nextCount;
    updateSize();
    update();
}

QSize HistoryUnreadNotifier::sizeHint() const
{
    const QFont font = AppFonts::applicationPixelWeightedFont(kFontPixelSize, kFontWeight);
    const QFontMetrics metrics(font);
    const int textWidth = metrics.horizontalAdvance(text());
    return QSize(kShadowLeft + kLeftPadding + kArrowWidth + kSymbolTextGap +
                         textWidth + kRightPadding,
                 kShadowTop + kBookmarkHeight + kShadowBottom);
}

void HistoryUnreadNotifier::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);

    const QRectF contentRect(kShadowLeft,
                             kShadowTop,
                             qMax(0, width() - kShadowLeft),
                             kBookmarkHeight);
    const QPainterPath path = bookmarkPath(contentRect.adjusted(0.5, 0.5, -0.5, -0.5));

    painter.setPen(Qt::NoPen);
    const QColor background = notifierBackground(m_hovered, m_pressed);
    painter.setBrush(background);
    painter.drawPath(path);

    QFont textFont = AppFonts::applicationPixelWeightedFont(kFontPixelSize, kFontWeight);
    painter.setFont(textFont);
    const QColor textColor = ThemeManager::instance().color(ThemeColor::TextOnAccent);
    painter.setPen(textColor);

    const QRect arrowRect(qRound(contentRect.left()) + kLeftPadding,
                          qRound(contentRect.top()) + (kBookmarkHeight - kArrowHeight) / 2,
                          kArrowWidth,
                          kArrowHeight);
    const qreal centerX = arrowRect.center().x() + 0.5;
    const qreal top = arrowRect.top() + 1.0;
    const qreal bottom = arrowRect.bottom() - 1.0;
    const qreal headY = top + 5.0;
    QPen arrowPen(textColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(arrowPen);
    painter.drawLine(QPointF(centerX, bottom), QPointF(centerX, top));
    painter.drawLine(QPointF(arrowRect.left() + 1.0, headY), QPointF(centerX, top));
    painter.drawLine(QPointF(centerX, top), QPointF(arrowRect.right() - 1.0, headY));

    painter.setPen(textColor);
    const QRect textRect(arrowRect.right() + 1 + kSymbolTextGap,
                         qRound(contentRect.top()),
                         qMax(0, qRound(contentRect.right()) - arrowRect.right() - 1 -
                                      kSymbolTextGap - kRightPadding),
                         kBookmarkHeight);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
}

void HistoryUnreadNotifier::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressed = true;
    update();
    AudioService::instance().playPortalClick();
    event->accept();
}

void HistoryUnreadNotifier::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const bool wasPressed = m_pressed;
    m_pressed = false;
    update();
    if (wasPressed && rect().contains(event->pos())) {
        emit clicked();
    }
    event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void HistoryUnreadNotifier::enterEvent(QEnterEvent* event)
#else
void HistoryUnreadNotifier::enterEvent(QEvent* event)
#endif
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void HistoryUnreadNotifier::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    m_pressed = false;
    update();
}

QString HistoryUnreadNotifier::text() const
{
    return QStringLiteral("有%1条未读消息").arg(m_count);
}

void HistoryUnreadNotifier::updateSize()
{
    setFixedSize(sizeHint());
}
