#include "ReferenceMessageNotifier.h"

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kHeight = 34;
constexpr int kPreferredWidth = 520;
constexpr int kHorizontalPadding = 14;
constexpr int kCloseSize = 18;
constexpr int kCloseRightPadding = 12;
constexpr int kTextCloseGap = 10;
constexpr int kCornerRadius = 8;

} // namespace

ReferenceMessageNotifier::ReferenceMessageNotifier(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        update();
    });

    QWidget::hide();
}

void ReferenceMessageNotifier::setMessage(const ChatMessage* message)
{
    m_displayText = displayTextForMessage(message);
    update();
}

QSize ReferenceMessageNotifier::sizeHint() const
{
    return QSize(kPreferredWidth, kHeight);
}

void ReferenceMessageNotifier::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), kCornerRadius, kCornerRadius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::MessageReferenceBackground));
    painter.drawPath(path);

    const QRect closeButtonRect = closeRect();
    if (m_closeHovered || m_closePressed) {
        painter.setBrush(ThemeManager::instance().color(
                m_closePressed ? ThemeColor::ControlPressed : ThemeColor::ControlHover));
        painter.drawRoundedRect(closeButtonRect, closeButtonRect.height() / 2, closeButtonRect.height() / 2);
    }

    QFont textFont = font();
    textFont.setPixelSize(13);
    painter.setFont(textFont);
    painter.setPen(ThemeManager::instance().color(ThemeColor::MessageReferenceText));
    const QFontMetrics fm(textFont);

    const QRect textRect(kHorizontalPadding,
                         0,
                         qMax(0, closeButtonRect.left() - kTextCloseGap - kHorizontalPadding),
                         height());
    painter.drawText(textRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     fm.elidedText(m_displayText, Qt::ElideRight, textRect.width()));

    const QColor closeColor = ThemeManager::instance().color(ThemeColor::TertiaryText);
    painter.setPen(QPen(closeColor, 1.6, Qt::SolidLine, Qt::RoundCap));
    const int inset = 5;
    painter.drawLine(closeButtonRect.topLeft() + QPoint(inset, inset),
                     closeButtonRect.bottomRight() - QPoint(inset, inset));
    painter.drawLine(closeButtonRect.topRight() + QPoint(-inset, inset),
                     closeButtonRect.bottomLeft() + QPoint(inset, -inset));
}

void ReferenceMessageNotifier::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && closeRect().contains(event->pos())) {
        m_closePressed = true;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ReferenceMessageNotifier::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_closePressed) {
        const bool shouldClose = closeRect().contains(event->pos());
        m_closePressed = false;
        update();
        if (shouldClose) {
            emit closeRequested();
        }
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ReferenceMessageNotifier::enterEvent(QEnterEvent* event)
#else
void ReferenceMessageNotifier::enterEvent(QEvent* event)
#endif
{
    QWidget::enterEvent(event);
    setCursor(Qt::ArrowCursor);
}

void ReferenceMessageNotifier::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_closeHovered = false;
    m_closePressed = false;
    unsetCursor();
    update();
}

void ReferenceMessageNotifier::mouseMoveEvent(QMouseEvent* event)
{
    const bool closeHovered = closeRect().contains(event->pos());
    if (m_closeHovered != closeHovered) {
        m_closeHovered = closeHovered;
        setCursor(m_closeHovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

QRect ReferenceMessageNotifier::closeRect() const
{
    return QRect(width() - kCloseRightPadding - kCloseSize,
                 (height() - kCloseSize) / 2,
                 kCloseSize,
                 kCloseSize);
}

QString ReferenceMessageNotifier::displayTextForMessage(const ChatMessage* message) const
{
    if (!message || message->getType() == MessageType::Recall) {
        return QStringLiteral("该消息已撤回");
    }

    QString senderName = message->getSenderName().trimmed();
    if (senderName.isEmpty()) {
        senderName = message->getSenderId();
    }

    const QString content = message->getType() == MessageType::Image
            ? QStringLiteral("[图片]")
            : message->getContent().simplified();
    return QStringLiteral("%1：%2").arg(senderName, content);
}
