#include "AiChatFloatingInputBar.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QImage>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/CustomTooltip.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/TransparentTextEdit.h"

namespace {

constexpr int kLightPanelAlpha = 246;
constexpr int kDarkPanelAlpha = 226;

QString submittedText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QPoint menuPopupPos(QWidget* anchor, StyledActionMenu* menu)
{
    menu->ensurePolished();
    const QSize hint = menu->sizeHint();
    const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
    QPoint pos(anchorRect.left(), anchorRect.bottom() + 8);

    QScreen* screen = QGuiApplication::screenAt(anchorRect.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return pos;
    }

    const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
    if (pos.y() + hint.height() > available.bottom()) {
        pos.setY(anchorRect.top() - hint.height() - 8);
    }
    if (pos.x() + hint.width() > available.right()) {
        pos.setX(available.right() - hint.width());
    }
    pos.setX(qMax(available.left(), pos.x()));
    pos.setY(qMax(available.top(), pos.y()));
    return pos;
}

class PlusButton final : public QAbstractButton
{
public:
    explicit PlusButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor color = ThemeManager::instance().color(ThemeColor::SecondaryText);
        painter.setPen(QPen(color, 1.45, Qt::SolidLine, Qt::RoundCap));
        const QPointF center = rect().center();
        const qreal arm = 7.0;
        painter.drawLine(QPointF(center.x() - arm, center.y()),
                         QPointF(center.x() + arm, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - arm),
                         QPointF(center.x(), center.y() + arm));
    }
};

class MenuTextButton final : public QAbstractButton
{
public:
    explicit MenuTextButton(const QString& text, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_text(text)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        QFont font = this->font();
        font.setPixelSize(14);
        const QFontMetrics metrics(font);
        int width = textPixelWidth(metrics, m_text) + kTrailingTextPadding;
        if (!m_secondaryText.isEmpty()) {
            width += kSecondaryTextGap + textPixelWidth(metrics, m_secondaryText);
        }
        if (m_showFastIcon) {
            width += kFastIconSide + kLeadingGap;
        }
        return QSize(width, 28);
    }

    void setText(const QString& text, const QString& secondaryText = QString())
    {
        if (m_text == text && m_secondaryText == secondaryText) {
            return;
        }

        m_text = text;
        m_secondaryText = secondaryText;
        updateGeometry();
        update();
    }

    void setFastIconVisible(bool visible)
    {
        if (m_showFastIcon == visible) {
            return;
        }

        m_showFastIcon = visible;
        updateGeometry();
        update();
    }

    void setMenuOpen(bool open)
    {
        if (m_menuOpen == open) {
            return;
        }

        m_menuOpen = open;
        if (!open) {
            m_suppressHoverUntilLeave = true;
        }
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            m_suppressHoverUntilLeave = false;
            update();
        }
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        QColor hover = ThemeManager::instance().color(ThemeColor::ContextMenuHover);
        hover.setAlpha((m_menuOpen || (underMouse() && !m_suppressHoverUntilLeave)) ? 150 : 0);
        if (hover.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(hover);
            painter.drawRoundedRect(rect(), 8, 8);
        }

        QFont textFont = painter.font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);

        int contentLeft = 0;
        if (m_showFastIcon) {
            const QRect iconRect(contentLeft,
                                 rect().center().y() - kFastIconSide / 2,
                                 kFastIconSide,
                                 kFastIconSide);
            paintFastIcon(painter, iconRect);
            contentLeft = iconRect.right() + 1 + kLeadingGap;
        }

        const QRect textRect = rect().adjusted(contentLeft, 0, -kTrailingTextPadding, 0);
        paintText(painter, textRect);

        const int cx = rect().right() - 8;
        const int cy = rect().center().y() + 1;
        QPainterPath chevron;
        chevron.moveTo(cx - 4, cy - 2);
        chevron.lineTo(cx, cy + 2);
        chevron.lineTo(cx + 4, cy - 2);
        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::SecondaryText), 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(chevron);
    }

private:
    QPixmap fastIconPixmap() const
    {
        QImage image(QStringLiteral(":/resources/icon/power.png"));
        if (image.isNull()) {
            return {};
        }

        image = image.convertToFormat(QImage::Format_ARGB32);
        if (ThemeManager::instance().isDark()) {
            for (int y = 0; y < image.height(); ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const QColor color = QColor::fromRgba(line[x]);
                    line[x] = qRgba(255 - color.red(),
                                    255 - color.green(),
                                    255 - color.blue(),
                                    color.alpha());
                }
            }
        }
        return QPixmap::fromImage(image);
    }

    void paintFastIcon(QPainter& painter, const QRect& iconRect) const
    {
        const QPixmap icon = fastIconPixmap();
        if (icon.isNull()) {
            return;
        }

        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(iconRect, icon);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    void paintText(QPainter& painter, const QRect& textRect) const
    {
        const QFontMetrics metrics = painter.fontMetrics();
        const int textWidth = textRect.width();
        if (textWidth <= 0) {
            return;
        }

        const QColor primaryColor = ThemeManager::instance().color(ThemeColor::SecondaryText);
        if (m_secondaryText.isEmpty()) {
            painter.setPen(primaryColor);
            painter.drawText(textRect,
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             metrics.elidedText(m_text, Qt::ElideRight, textWidth));
            return;
        }

        QColor secondaryColor = primaryColor;
        secondaryColor.setAlpha(ThemeManager::instance().isDark() ? 150 : 118);

        const int naturalSecondaryWidth = textPixelWidth(metrics, m_secondaryText);
        const int naturalPrimaryWidth = textPixelWidth(metrics, m_text);
        const int naturalTextWidth = naturalPrimaryWidth + kSecondaryTextGap + naturalSecondaryWidth;

        int primaryWidth = naturalPrimaryWidth;
        int secondaryWidth = naturalSecondaryWidth;
        if (textWidth < naturalTextWidth) {
            secondaryWidth = qMin(naturalSecondaryWidth, textWidth);
            primaryWidth = qMax(0, textWidth - secondaryWidth - kSecondaryTextGap);
        }

        const QRect primaryRect(textRect.left(), textRect.top(), primaryWidth, textRect.height());
        painter.setPen(primaryColor);
        painter.drawText(primaryRect,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         metrics.elidedText(m_text, Qt::ElideRight, primaryWidth));

        const QRect secondaryRect(primaryRect.right() + 1 + kSecondaryTextGap,
                                  textRect.top(),
                                  secondaryWidth,
                                  textRect.height());
        painter.setPen(secondaryColor);
        painter.drawText(secondaryRect,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         metrics.elidedText(m_secondaryText, Qt::ElideRight, secondaryWidth));
    }

    static constexpr int kFastIconSide = 15;
    static constexpr int kLeadingGap = 5;
    static constexpr int kSecondaryTextGap = 5;
    static constexpr int kTrailingTextPadding = 26;
    static constexpr int kTextElideSlack = 4;

    static int textPixelWidth(const QFontMetrics& metrics, const QString& text)
    {
        return text.isEmpty() ? 0 : metrics.horizontalAdvance(text) + kTextElideSlack;
    }

    QString m_text;
    QString m_secondaryText;
    bool m_showFastIcon = false;
    bool m_menuOpen = false;
    bool m_suppressHoverUntilLeave = false;
};

class ContextUsageIndicator final : public QWidget
{
public:
    explicit ContextUsageIndicator(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setCursor(Qt::ArrowCursor);
        setMouseTracking(true);
        setAttribute(Qt::WA_TranslucentBackground);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        hide();
    }

    QSize sizeHint() const override
    {
        return QSize(kIndicatorSide, kIndicatorSide);
    }

    void setUsage(const AiChatContextUsage& usage)
    {
        m_usage = usage;
        setVisible(usage.available);
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            showContextTooltip();
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
            hideContextTooltip();
        }
        return QWidget::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (!m_usage.available) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool dark = ThemeManager::instance().isDark();
        const qreal ratio = contextUsageRatio();
        const QRectF arcRect = QRectF(rect()).adjusted(5.2, 5.2, -5.2, -5.2);
        const QColor trackColor = dark ? QColor(255, 255, 255, 48) : QColor(0, 0, 0, 32);
        QColor progressColor;
        if (ratio >= 0.82) {
            progressColor = dark ? QColor(255, 112, 112) : QColor(190, 58, 58);
        } else if (ratio >= 0.62) {
            progressColor = dark ? QColor(255, 190, 88) : QColor(178, 112, 28);
        } else {
            progressColor = dark ? QColor(112, 205, 255) : QColor(30, 124, 190);
        }

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(trackColor, 2.3, Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(arcRect);
        painter.setPen(QPen(progressColor, 2.3, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 90 * 16, qRound(-360.0 * ratio * 16.0));
    }

private:
    qreal contextUsageRatio() const
    {
        return qBound<qreal>(0.0,
                             static_cast<qreal>(m_usage.usedTokens) /
                                     static_cast<qreal>(qMax(1, m_usage.maxTokens)),
                             1.0);
    }

    QString contextTooltipText() const
    {
        const int percent = qRound(contextUsageRatio() * 100.0);
        const QLocale locale;
        return QStringLiteral("上下文使用情况：%1%\n已使用：%2 / %3 tokens\n\n该数值由当前对话长度模拟生成，包含历史消息、系统提示和待发送内容。接近上限时，较早内容可能会被压缩或裁剪；需要完整保留上下文时，建议开启新对话或精简历史。")
                .arg(percent)
                .arg(locale.toString(m_usage.usedTokens))
                .arg(locale.toString(m_usage.maxTokens));
    }

    void showContextTooltip()
    {
        if (!m_usage.available) {
            hideContextTooltip();
            return;
        }

        if (!m_contextTooltip) {
            m_contextTooltip = new CustomTooltip(this);
            m_contextTooltip->setBackgroundOpacity(1.0);
        }
        m_contextTooltip->setText(contextTooltipText());
        const QPoint tooltipAnchor(width() / 2 - m_contextTooltip->width() / 2, -10);
        m_contextTooltip->showTooltip(mapToGlobal(tooltipAnchor));
    }

    void hideContextTooltip()
    {
        if (m_contextTooltip) {
            m_contextTooltip->hide();
        }
    }

    static constexpr int kIndicatorSide = 28;

    AiChatContextUsage m_usage;
    CustomTooltip* m_contextTooltip = nullptr;
};

class SendButton final : public QAbstractButton
{
public:
    explicit SendButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        updateCursor();
        setFocusPolicy(Qt::NoFocus);
    }

    void setStreaming(bool streaming)
    {
        if (m_streaming == streaming) {
            return;
        }
        m_streaming = streaming;
        updateCursor();
        update();
    }

    void setHasText(bool hasText)
    {
        if (m_hasText == hasText) {
            return;
        }
        m_hasText = hasText;
        updateCursor();
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QString iconPath = m_streaming
                ? QStringLiteral(":/resources/icon/fail.png")
                : QStringLiteral(":/resources/icon/arrow.png");
        const QPixmap icon(iconPath);
        if (icon.isNull()) {
            return;
        }

        const int iconSide = qRound(qMin(width(), height()) * (m_streaming ? 0.58 : 0.68));
        QRect iconRect(0, 0, iconSide, iconSide);
        iconRect.moveCenter(rect().center());

        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(iconRect, icon);
    }

private:
    void updateCursor()
    {
        setCursor((m_streaming || m_hasText) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    bool m_streaming = false;
    bool m_hasText = false;
};

QAction* addMenuAction(StyledActionMenu* menu, const QString& text, bool checked = false)
{
    QAction* action = menu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    return action;
}

void setMenuButtonText(QAbstractButton* button,
                       const QString& text,
                       const QString& secondaryText = QString())
{
    if (auto* menuButton = dynamic_cast<MenuTextButton*>(button)) {
        menuButton->setText(text, secondaryText);
    }
}

} // namespace

AiChatFloatingInputBar::AiChatFloatingInputBar(QWidget* parent)
    : QWidget(parent)
    , m_inputEdit(new TransparentTextEdit(this))
    , m_addButton(new PlusButton(this))
    , m_contextUsageIndicator(new ContextUsageIndicator(this))
    , m_permissionButton(new MenuTextButton(QStringLiteral("Default permissions"), this))
    , m_modelButton(new MenuTextButton(QString(), this))
    , m_actionButton(new SendButton(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);

    m_inputEdit->setFocusPolicy(Qt::StrongFocus);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(QStringLiteral("输入你想提问的任何事情"));
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_inputEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_inputEdit->setMinimumHeight(kMinInputHeight);
    m_inputEdit->setViewportPadding(kInputTextHorizontalPadding,
                                    kInputTextTopPadding,
                                    kInputTextHorizontalPadding,
                                    kInputTextBottomPadding);
    m_inputEdit->viewport()->setCursor(Qt::IBeamCursor);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->viewport()->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    connect(m_permissionButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::showPermissionMenu);
    connect(m_modelButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::showModelMenu);
    connect(m_actionButton, &QAbstractButton::clicked,
            this, &AiChatFloatingInputBar::onActionButtonClicked);
    connect(m_inputEdit, &QTextEdit::textChanged, this, [this]() {
        updateActionButtonIcon();
        updatePreferredHeight();
    });

    QFont font = m_inputEdit->font();
    font.setPixelSize(15);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    QColor shadowColor = ThemeManager::instance().color(ThemeColor::FloatingPanelShadow);
    shadowColor.setAlpha(ThemeManager::instance().isDark() ? 70 : 42);
    shadow->setColor(shadowColor);
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AiChatFloatingInputBar::applyTheme);

    applyTheme();
    updateActionButtonIcon();
    updateModelButtonText();
    updatePreferredHeight();
    updateInputGeometry();
}

QSize AiChatFloatingInputBar::sizeHint() const
{
    return QSize(640, preferredHeightForWidth(width() > 0 ? width() : 640));
}

void AiChatFloatingInputBar::focusInput()
{
    m_inputEdit->setFocus();
    m_inputEdit->moveCursor(QTextCursor::End);
}

void AiChatFloatingInputBar::setStreaming(bool streaming)
{
    if (m_streaming == streaming) {
        return;
    }

    m_streaming = streaming;
    updateActionButtonIcon();
}

QString AiChatFloatingInputBar::text() const
{
    return m_inputEdit->toPlainText();
}

void AiChatFloatingInputBar::setText(const QString& text)
{
    m_inputEdit->setPlainText(text);
    m_inputEdit->moveCursor(QTextCursor::End);
    updatePreferredHeight();
}

void AiChatFloatingInputBar::clearText()
{
    m_inputEdit->clear();
    updatePreferredHeight();
}

void AiChatFloatingInputBar::setContextUsage(const AiChatContextUsage& usage)
{
    if (auto* indicator = dynamic_cast<ContextUsageIndicator*>(m_contextUsageIndicator)) {
        indicator->setUsage(usage);
    }
    updateInputGeometry();
}

void AiChatFloatingInputBar::setContextUsageVisible(bool visible)
{
    if (!m_contextUsageIndicator) {
        return;
    }

    m_contextUsageIndicator->setVisible(visible);
    updateInputGeometry();
}

int AiChatFloatingInputBar::preferredHeightForWidth(int width) const
{
    const int inputWidth = qMax(1, width - kInputLeftMargin - kInputRightPadding -
                                      kInputTextHorizontalPadding * 2);

    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(m_inputEdit->font());
    QTextOption option = m_inputEdit->document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(option);
    const QString content = m_inputEdit->toPlainText().isEmpty()
            ? QStringLiteral(" ")
            : m_inputEdit->toPlainText();
    document.setPlainText(content);
    document.setTextWidth(inputWidth);

    const int documentHeight = qCeil(document.size().height());
    const int desiredInputHeight = documentHeight +
            kInputTextTopPadding + kInputTextBottomPadding + 2;
    const int inputHeight = qBound(kMinInputHeight, desiredInputHeight, kMaxInputHeight);
    return qBound(kMinHeight,
                  kInputTopMargin + inputHeight + kInputToolbarGap +
                          kToolbarHeight + kToolbarBottomMargin,
                  kMaxHeight);
}

bool AiChatFloatingInputBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_inputEdit) {
        if (event->type() == QEvent::FocusIn) {
            emit inputFocused();
        } else if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentText();
                return true;
            }
        }
    }

    if (watched == m_inputEdit->viewport() && event->type() == QEvent::MouseButtonPress) {
        m_inputEdit->setFocus(Qt::MouseFocusReason);
    }

    return QWidget::eventFilter(watched, event);
}

void AiChatFloatingInputBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        focusInput();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void AiChatFloatingInputBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panelRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(panelRect, kCornerRadius, kCornerRadius);

    QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
    background.setAlpha(ThemeManager::instance().isDark() ? kDarkPanelAlpha : kLightPanelAlpha);
    painter.fillPath(path, background);

    QColor border = ThemeManager::instance().color(ThemeColor::Divider);
    border.setAlpha(ThemeManager::instance().isDark() ? 130 : 190);
    painter.setPen(QPen(border, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void AiChatFloatingInputBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePreferredHeight();
    updateInputGeometry();
}

void AiChatFloatingInputBar::applyTheme()
{
    QPalette palette = m_inputEdit->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
    palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
    palette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::TextOnAccent));
    m_inputEdit->setPalette(palette);

    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
        QColor shadowColor = ThemeManager::instance().color(ThemeColor::FloatingPanelShadow);
        shadowColor.setAlpha(ThemeManager::instance().isDark() ? 70 : 42);
        shadow->setColor(shadowColor);
    }

    updateActionButtonIcon();
    m_addButton->update();
    m_contextUsageIndicator->update();
    m_permissionButton->update();
    m_modelButton->update();
    update();
}

void AiChatFloatingInputBar::onActionButtonClicked()
{
    if (m_streaming) {
        emit stopStreamingRequested();
        return;
    }

    sendCurrentText();
}

void AiChatFloatingInputBar::showPermissionMenu()
{
    auto* menu = new StyledActionMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (auto* button = dynamic_cast<MenuTextButton*>(m_permissionButton)) {
        button->setMenuOpen(true);
    }
    addMenuAction(menu, QStringLiteral("Default permissions"), true);
    addMenuAction(menu, QStringLiteral("Ask every time"));
    addMenuAction(menu, QStringLiteral("Read-only"));
    connect(menu, &QMenu::triggered, this, [this](QAction* action) {
        if (!action || action->isSeparator()) {
            return;
        }
        setMenuButtonText(m_permissionButton, action->text());
        updateInputGeometry();
    });
    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        if (auto* button = dynamic_cast<MenuTextButton*>(m_permissionButton)) {
            button->setMenuOpen(false);
        }
        menu->deleteLater();
    });
    menu->popup(menuPopupPos(m_permissionButton, menu));
}

void AiChatFloatingInputBar::showModelMenu()
{
    auto* menu = new StyledActionMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
        button->setMenuOpen(true);
    }

    auto* thinkingGroup = new QActionGroup(menu);
    thinkingGroup->setExclusive(true);
    const QStringList thinkingLevels = {
        QStringLiteral("Low"),
        QStringLiteral("Medium"),
        QStringLiteral("High"),
        QStringLiteral("Extra High"),
    };
    for (const QString& level : thinkingLevels) {
        QAction* action = addMenuAction(menu, level, level == m_selectedThinkingLevel);
        thinkingGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, level]() {
            m_selectedThinkingLevel = level;
            updateModelButtonText();
            updateInputGeometry();
        });
    }

    menu->addSeparator();

    auto* modelGroup = new QActionGroup(menu);
    modelGroup->setExclusive(true);

    StyledActionMenu* deepSeekMenu = menu->addStyledMenu(QStringLiteral("DeepSeek V4 Pro"));
    const QStringList deepSeekModels = {
        QStringLiteral("DeepSeek V4 Pro"),
        QStringLiteral("DeepSeek R1"),
        QStringLiteral("Qwen3 Max"),
        QStringLiteral("Claude Sonnet 4.5"),
        QStringLiteral("GPT-5.5"),
        QStringLiteral("Gemini 2.5 Pro"),
    };
    for (const QString& model : deepSeekModels) {
        QAction* action = addMenuAction(deepSeekMenu, model, model == m_selectedModelName);
        modelGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, model]() {
            m_selectedModelName = model;
            updateModelButtonText();
            updateInputGeometry();
        });
    }

    auto* speedModeGroup = new QActionGroup(menu);
    speedModeGroup->setExclusive(true);

    StyledActionMenu* speedMenu = menu->addStyledMenu(QStringLiteral("Speed"));
    const QStringList speedModes = {
        QStringLiteral("Standard"),
        QStringLiteral("Speed"),
    };
    for (const QString& speedMode : speedModes) {
        QAction* action = addMenuAction(speedMenu,
                                        speedMode,
                                        speedMode == m_selectedSpeedMode);
        speedModeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, speedMode]() {
            m_selectedSpeedMode = speedMode;
            updateModelButtonText();
            updateInputGeometry();
        });
    }

    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
            button->setMenuOpen(false);
        }
        menu->deleteLater();
    });
    menu->popup(menuPopupPos(m_modelButton, menu));
}

void AiChatFloatingInputBar::updateModelButtonText()
{
    setMenuButtonText(m_modelButton, m_selectedModelName, m_selectedThinkingLevel);
    updateModelButtonState();
}

void AiChatFloatingInputBar::updateModelButtonState()
{
    if (auto* button = dynamic_cast<MenuTextButton*>(m_modelButton)) {
        button->setFastIconVisible(m_selectedSpeedMode == QStringLiteral("Speed"));
    }
}

void AiChatFloatingInputBar::sendCurrentText()
{
    if (m_streaming) {
        return;
    }

    const QString text = submittedText(m_inputEdit->toPlainText());
    if (text.isEmpty()) {
        return;
    }

    m_inputEdit->clear();
    emit sendText(text);
    focusInput();
}

void AiChatFloatingInputBar::updateActionButtonIcon()
{
    if (auto* sendButton = dynamic_cast<SendButton*>(m_actionButton)) {
        sendButton->setStreaming(m_streaming);
        sendButton->setHasText(!submittedText(m_inputEdit->toPlainText()).isEmpty());
    }
}

void AiChatFloatingInputBar::updateInputGeometry()
{
    const int toolbarY = qMax(kInputTopMargin, height() - kToolbarBottomMargin - kToolbarHeight);
    const int inputHeight = qMax(0, toolbarY - kInputTopMargin - kInputToolbarGap);
    m_inputEdit->setGeometry(kInputLeftMargin,
                             kInputTopMargin,
                             qMax(0, width() - kInputLeftMargin - kInputRightPadding),
                             inputHeight);

    const int buttonY = toolbarY + (kToolbarHeight - kActionButtonSize) / 2;
    m_addButton->setGeometry(kToolbarSideMargin, buttonY, kActionButtonSize, kActionButtonSize);

    m_actionButton->setGeometry(qMax(0, width() - kActionButtonRightMargin - kActionButtonSize),
                                buttonY,
                                kActionButtonSize,
                                kActionButtonSize);

    int rightCursorX = m_actionButton->geometry().left() - kToolbarGap;

    const bool showUsageIndicator = m_contextUsageIndicator && m_contextUsageIndicator->isVisible();
    const QSize usageSize = showUsageIndicator ? m_contextUsageIndicator->sizeHint() : QSize();

    const QSize modelSize = m_modelButton->sizeHint();
    const int modelAvailableWidth = qMax(28, rightCursorX - kToolbarSideMargin);
    const int modelWidth = qMin(modelSize.width(), modelAvailableWidth);
    const int modelX = rightCursorX - modelWidth;
    m_modelButton->setGeometry(modelX,
                               toolbarY + (kToolbarHeight - modelSize.height()) / 2,
                               modelWidth,
                               modelSize.height());

    int rightClusterLeft = m_modelButton->geometry().left();
    if (m_contextUsageIndicator && m_contextUsageIndicator->isVisible()) {
        m_contextUsageIndicator->setGeometry(modelX - kToolbarGap - usageSize.width(),
                                             toolbarY + (kToolbarHeight - usageSize.height()) / 2,
                                             usageSize.width(),
                                             usageSize.height());
        rightClusterLeft = m_contextUsageIndicator->geometry().left();
    } else if (m_contextUsageIndicator) {
        m_contextUsageIndicator->setGeometry(0, 0, 0, 0);
    }

    int cursorX = m_addButton->geometry().right() + 1 + kToolbarGap;
    const QSize permissionSize = m_permissionButton->sizeHint();
    const int permissionAvailableWidth = qMax(0,
                                              rightClusterLeft - kToolbarGap - cursorX);
    m_permissionButton->setGeometry(cursorX,
                                    toolbarY + (kToolbarHeight - permissionSize.height()) / 2,
                                    qMin(permissionSize.width(), permissionAvailableWidth),
                                    permissionSize.height());
}

void AiChatFloatingInputBar::updatePreferredHeight()
{
    const int newHeight = preferredHeightForWidth(width() > 0 ? width() : 640);
    if (m_preferredHeight == newHeight) {
        return;
    }

    m_preferredHeight = newHeight;
    updateGeometry();
    emit preferredHeightChanged(newHeight);
}
