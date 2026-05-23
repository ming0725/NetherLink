#include "InWindowPopupOverlay.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/FastGaussianBlur.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

#ifdef Q_OS_MACOS
#include "shared/ui/FloatingInputBar.h"
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

constexpr int kPopupRadius = 12;
constexpr int kPopupPadding = 24;
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 34;
constexpr int kInputHeight = 36;
constexpr int kBlurAnimationDurationMs = 180;
constexpr int kPopupShadowMargin = 34;
constexpr qreal kPopupInitialScale = 0.985;
constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr auto kSystemFloatingBarsSuppressedOpacityProperty = "systemFloatingBarsSuppressedOpacity";
constexpr auto kUsesSystemFloatingBarBridgeProperty = "usesSystemFloatingBarBridge";

QColor popupStrokeColor()
{
    return ThemeManager::instance().isDark()
            ? QColor(0x88, 0x88, 0x88, 190)
            : QColor(0x88, 0x88, 0x88, 170);
}

QColor colorCompositedOver(const QColor& foreground, const QColor& background)
{
    const QColor fg = foreground.toRgb();
    const QColor bg = background.toRgb();
    const qreal alpha = fg.alphaF();
    return QColor(qRound(fg.red() * alpha + bg.red() * (1.0 - alpha)),
                  qRound(fg.green() * alpha + bg.green() * (1.0 - alpha)),
                  qRound(fg.blue() * alpha + bg.blue() * (1.0 - alpha)));
}

QWidget* popupHostFor(QWidget* parent)
{
    if (parent) {
        if (QWidget* window = parent->window()) {
            return window;
        }
    }
    return QApplication::activeWindow();
}

QImage renderBlurredSnapshot(QImage scaledSource, qreal blurRadius, qreal devicePixelRatio)
{
    if (scaledSource.isNull()) {
        return QImage();
    }

    FastGaussianBlur blur;
    QImage blurred = blur.blur(scaledSource, blurRadius);
    blurred.setDevicePixelRatio(devicePixelRatio);
    return blurred;
}

class PopupFrame : public QFrame
{
public:
    explicit PopupFrame(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF frameRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));
        painter.drawRoundedRect(frameRect, kPopupRadius, kPopupRadius);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(popupStrokeColor(), 1));
        painter.drawRoundedRect(frameRect, kPopupRadius, kPopupRadius);
    }
};

PaintedLabel* createTitleLabel(const QString& title)
{
    auto* label = new PaintedLabel(title);
    label->setObjectName(QStringLiteral("inWindowPopupTitle"));
    label->setWordWrap(true);
    QFont font = QApplication::font();
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    return label;
}

PaintedLabel* createBodyLabel(const QString& text)
{
    auto* label = new PaintedLabel(text);
    label->setObjectName(QStringLiteral("inWindowPopupBody"));
    label->setWordWrap(true);
    label->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    QFont font = QApplication::font();
    font.setPixelSize(14);
    label->setFont(font);
    return label;
}

StatefulPushButton* createButton(const QString& text, QWidget* parent = nullptr)
{
    auto* button = new StatefulPushButton(text, parent);
    button->setFixedSize(kButtonWidth, kButtonHeight);
    return button;
}

void applyPopupDefaultButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }

    button->setDefaultStyle();
    button->setBorderColor(popupStrokeColor());
    button->setBorderWidth(1);
}

InWindowPopupOverlay* overlayFor(QWidget* widget)
{
    QWidget* cursor = widget;
    while (cursor) {
        if (auto* overlay = qobject_cast<InWindowPopupOverlay*>(cursor)) {
            return overlay;
        }
        cursor = cursor->parentWidget();
    }
    return nullptr;
}

QWidget* createMessageContent(const QString& title,
                              const QString& text,
                              InWindowPopup::Button defaultButton,
                              InWindowPopup::Button* result)
{
    auto* content = new QWidget;
    content->setMinimumWidth(360);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
    layout->setSpacing(14);

    layout->addWidget(createTitleLabel(title));
    layout->addWidget(createBodyLabel(text));
    layout->addSpacing(2);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto* noButton = createButton(QStringLiteral("取消"), content);
    applyPopupDefaultButtonStyle(noButton);
    auto* yesButton = createButton(QStringLiteral("确认"), content);
    yesButton->setPrimaryStyle();
    if (defaultButton == InWindowPopup::Button::No) {
        noButton->setFocus();
    } else {
        yesButton->setFocus();
    }

    buttonLayout->addWidget(noButton);
    buttonLayout->addWidget(yesButton);
    layout->addLayout(buttonLayout);

    QObject::connect(noButton, &QPushButton::clicked, content, [result, content]() {
        *result = InWindowPopup::Button::No;
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });
    QObject::connect(yesButton, &QPushButton::clicked, content, [result, content]() {
        *result = InWindowPopup::Button::Yes;
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    });

    return content;
}

} // namespace

InWindowPopupOverlay::Options::Options()
    : popupMargins(36, 36, 36, 36)
    , maximumPopupSize(560, 420)
    , blurRadius(12.0)
    , blurRenderScale(0.58)
    , overlayTint(0, 0, 0, 74)
    , dismissOnOutsideClick(true)
    , dismissOnEscape(true)
    , deleteContentOnClose(true)
{
}

InWindowPopupOverlay::InWindowPopupOverlay(QWidget* host, QWidget* content, const Options& options)
    : QWidget(host)
    , m_options(options)
    , m_host(host)
    , m_previousFocus(QApplication::focusWidget())
    , m_content(content)
    , m_popupContainer(new QWidget(this))
    , m_popupFrame(new PopupFrame(m_popupContainer))
{
    setObjectName(QStringLiteral("inWindowPopupOverlay"));
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    suppressSystemFloatingBars();
    captureBlurredSnapshot();
    setOpenProgress(m_snapshot.isNull() ? 1.0 : 0.0);

    m_popupContainer->setObjectName(QStringLiteral("inWindowPopupContainer"));
    m_popupContainer->setAttribute(Qt::WA_StyledBackground, false);
    m_popupContainer->setAttribute(Qt::WA_NoSystemBackground, true);

    m_popupOpacityEffect = new QGraphicsOpacityEffect(m_popupContainer);
    m_popupOpacityEffect->setOpacity(m_popupProgress);
    m_popupContainer->setGraphicsEffect(m_popupOpacityEffect);

    m_popupFrame->setObjectName(QStringLiteral("inWindowPopupFrame"));
    m_popupFrame->setAttribute(Qt::WA_StyledBackground, false);

    auto* shadow = new QGraphicsDropShadowEffect(m_popupFrame);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 10);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::PopupShadow));
    m_popupFrame->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(m_popupFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    if (m_content) {
        m_content->setParent(m_popupFrame);
        layout->addWidget(m_content);
    }

    if (m_host) {
        setGeometry(m_host->rect());
    }
    updatePopupGeometry();

    qApp->installEventFilter(this);
}

InWindowPopupOverlay::~InWindowPopupOverlay()
{
    qApp->removeEventFilter(this);
    restoreSystemFloatingBars();
    if (m_content && !m_options.deleteContentOnClose) {
        m_content->setParent(nullptr);
    }
    if (m_previousFocus) {
        m_previousFocus->setFocus(Qt::OtherFocusReason);
    }
}

InWindowPopupOverlay* InWindowPopupOverlay::showPopup(QWidget* parent,
                                                     QWidget* content,
                                                     const Options& options)
{
    QWidget* host = popupHostFor(parent);
    if (!host || !content) {
        if (content) {
            content->deleteLater();
        }
        return nullptr;
    }

    auto* overlay = new InWindowPopupOverlay(host, content, options);
    overlay->show();
    overlay->raise();
    if (content->focusProxy()) {
        content->focusProxy()->setFocus(Qt::PopupFocusReason);
    } else {
        content->setFocus(Qt::PopupFocusReason);
    }
    return overlay;
}

QWidget* InWindowPopupOverlay::contentWidget() const
{
    return m_content;
}

InWindowPopupOverlay::DismissReason InWindowPopupOverlay::dismissReason() const
{
    return m_dismissReason;
}

void InWindowPopupOverlay::closePopup(DismissReason reason)
{
    if (m_closing) {
        return;
    }

    m_closing = true;
    m_dismissReason = reason;
    emit dismissed(reason);
    close();
}

bool InWindowPopupOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (m_closing) {
        return QWidget::eventFilter(watched, event);
    }

    auto* watchedWidget = qobject_cast<QWidget*>(watched);
    const bool overlayEvent = watchedWidget && isOverlayChild(watchedWidget);

    switch (event->type()) {
    case QEvent::KeyPress: {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (m_options.dismissOnEscape && keyEvent->key() == Qt::Key_Escape) {
            closePopup(DismissReason::Escape);
            return true;
        }
        return watchedWidget && !overlayEvent;
    }
    case QEvent::KeyRelease:
    case QEvent::Shortcut:
    case QEvent::ShortcutOverride:
        return watchedWidget && !overlayEvent;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
        return watchedWidget && !overlayEvent;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void InWindowPopupOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_blurredSnapshot.isNull()) {
        if (m_blurProgress < 1.0 && !m_snapshot.isNull()) {
            painter.drawImage(rect(), m_snapshot);
            painter.save();
            painter.setOpacity(qBound<qreal>(0.0, m_blurProgress, 1.0));
            painter.drawImage(rect(), m_blurredSnapshot);
            painter.restore();
        } else {
            painter.drawImage(rect(), m_blurredSnapshot);
        }
    } else if (!m_snapshot.isNull()) {
        painter.drawImage(rect(), m_snapshot);
    } else {
        painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::WindowBackground));
    }

    QColor tint = m_options.overlayTint;
    tint.setAlpha(qRound(tint.alpha() * qBound<qreal>(0.0, m_blurProgress, 1.0)));
    painter.fillRect(rect(), tint);
}

void InWindowPopupOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePopupGeometry();
}

void InWindowPopupOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    startOpenAnimation();
}

void InWindowPopupOverlay::mousePressEvent(QMouseEvent* event)
{
    if (m_options.dismissOnOutsideClick && !isInsidePopup(event->pos())) {
        closePopup(DismissReason::OutsideClick);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void InWindowPopupOverlay::captureBlurredSnapshot()
{
    if (!m_host) {
        return;
    }

    const QPixmap snapshot = m_host->grab();
    QImage source = snapshot.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (source.isNull()) {
        return;
    }

    const qreal devicePixelRatio = qMax<qreal>(1.0, snapshot.devicePixelRatio());

    QColor baseColor = ThemeManager::instance().color(ThemeColor::WindowBackground);
    baseColor.setAlpha(255);
    QImage opaqueSource(source.size(), QImage::Format_ARGB32_Premultiplied);
    opaqueSource.fill(baseColor);
    {
        QPainter sourcePainter(&opaqueSource);
        sourcePainter.drawImage(opaqueSource.rect(), source, source.rect());
    }
    source = opaqueSource;
    source.setDevicePixelRatio(devicePixelRatio);
    m_snapshot = source;

    const qreal scale = qBound<qreal>(0.2, m_options.blurRenderScale, 1.0);
    const QSize scaledSize(qMax(1, qRound(source.width() * scale)),
                           qMax(1, qRound(source.height() * scale)));
    QImage scaledSource = source.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaledSource.setDevicePixelRatio(devicePixelRatio * scale);

    m_blurWatcher = new QFutureWatcher<QImage>(this);
    m_blurInFlight = true;
    connect(m_blurWatcher, &QFutureWatcher<QImage>::finished, this, [this]() {
        auto* watcher = m_blurWatcher;
        if (!watcher) {
            return;
        }

        m_blurredSnapshot = watcher->result();
        m_blurInFlight = false;
        m_blurWatcher = nullptr;
        watcher->deleteLater();

        if (m_closing) {
            return;
        }
        if (m_blurredSnapshot.isNull()) {
            setOpenProgress(1.0);
            return;
        }
        if (isVisible()) {
            startOpenAnimation();
        }
        update();
    });

    const qreal blurRadius = m_options.blurRadius * scale * devicePixelRatio;
    const qreal blurredDevicePixelRatio = devicePixelRatio * scale;
    m_blurWatcher->setFuture(QtConcurrent::run(renderBlurredSnapshot,
                                               scaledSource,
                                               blurRadius,
                                               blurredDevicePixelRatio));
}

void InWindowPopupOverlay::startOpenAnimation()
{
    if (m_openAnimationStarted) {
        return;
    }
    if (m_blurInFlight) {
        return;
    }
    m_openAnimationStarted = true;

    if (m_blurredSnapshot.isNull()) {
        setOpenProgress(1.0);
        m_snapshot = QImage();
        return;
    }

    auto* animation = new QVariantAnimation(this);
    animation->setDuration(kBlurAnimationDurationMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setOpenProgress(value.toReal());
    });
    connect(animation, &QVariantAnimation::finished, this, [this]() {
        setOpenProgress(1.0);
        if (m_popupOpacityEffect) {
            m_popupContainer->setGraphicsEffect(nullptr);
            m_popupOpacityEffect = nullptr;
        }
        m_snapshot = QImage();
        update();
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void InWindowPopupOverlay::setOpenProgress(qreal progress)
{
    progress = qBound<qreal>(0.0, progress, 1.0);
    m_blurProgress = progress;
    m_popupProgress = progress;
    updateSuppressedSystemFloatingBars(progress);
    if (m_popupOpacityEffect) {
        m_popupOpacityEffect->setOpacity(progress);
    }
    updatePopupGeometry();
    update();
}

void InWindowPopupOverlay::suppressSystemFloatingBars()
{
#ifdef Q_OS_MACOS
    if (!m_host) {
        return;
    }

    const auto floatingWidgets = m_host->findChildren<QWidget*>();
    for (QWidget* widget : floatingWidgets) {
        if (!widget || widget == this || isOverlayChild(widget)) {
            continue;
        }
        if (!widget->property(kUsesSystemFloatingBarBridgeProperty).toBool()) {
            continue;
        }

        SuppressedWidgetState state;
        state.widget = widget;
        state.previousSuppressedProperty = widget->property(kSystemFloatingBarsSuppressedProperty);
        state.previousSuppressedOpacityProperty =
                widget->property(kSystemFloatingBarsSuppressedOpacityProperty);
        state.hadSuppressedProperty = state.previousSuppressedProperty.isValid();
        state.hadSuppressedOpacityProperty = state.previousSuppressedOpacityProperty.isValid();
        state.wasVisible = widget->isVisible();
        m_suppressedWidgets.push_back(state);

        widget->setProperty(kSystemFloatingBarsSuppressedProperty, true);
        if (state.wasVisible) {
            if (qobject_cast<FloatingInputBar*>(widget)) {
                widget->setProperty(kSystemFloatingBarsSuppressedOpacityProperty, 1.0);
            } else {
                widget->hide();
            }
        }
    }
#endif
}

void InWindowPopupOverlay::updateSuppressedSystemFloatingBars(qreal openProgress)
{
#ifdef Q_OS_MACOS
    const qreal opacity = 1.0 - qBound<qreal>(0.0, openProgress, 1.0);
    for (SuppressedWidgetState& state : m_suppressedWidgets) {
        QWidget* widget = state.widget;
        if (!widget || !state.wasVisible) {
            continue;
        }
        if (!qobject_cast<FloatingInputBar*>(widget)) {
            continue;
        }

        widget->setProperty(kSystemFloatingBarsSuppressedOpacityProperty, opacity);
        if (opacity > 0.001) {
            MacFloatingInputBarBridge::syncInputBar(widget, QString(), opacity);
            continue;
        }

        MacFloatingInputBarBridge::clearInputBar(widget);
        if (!widget->isHidden()) {
            widget->hide();
        }
    }
#else
    Q_UNUSED(openProgress);
#endif
}

void InWindowPopupOverlay::restoreSystemFloatingBars()
{
#ifdef Q_OS_MACOS
    for (const SuppressedWidgetState& state : m_suppressedWidgets) {
        QWidget* widget = state.widget;
        if (!widget) {
            continue;
        }

        if (state.hadSuppressedProperty) {
            widget->setProperty(kSystemFloatingBarsSuppressedProperty, state.previousSuppressedProperty);
        } else {
            widget->setProperty(kSystemFloatingBarsSuppressedProperty, QVariant());
        }
        if (state.hadSuppressedOpacityProperty) {
            widget->setProperty(kSystemFloatingBarsSuppressedOpacityProperty,
                                state.previousSuppressedOpacityProperty);
        } else {
            widget->setProperty(kSystemFloatingBarsSuppressedOpacityProperty, QVariant());
        }

        if (state.wasVisible && !widget->property(kSystemFloatingBarsSuppressedProperty).toBool()) {
            widget->show();
            widget->raise();
        }
    }
    m_suppressedWidgets.clear();
#endif
}

void InWindowPopupOverlay::updatePopupGeometry()
{
    if (!m_popupContainer || !m_popupFrame) {
        return;
    }

    const QSize popupSize = boundedPopupSize();
    const qreal progress = qBound<qreal>(0.0, m_popupProgress, 1.0);
    const qreal scale = kPopupInitialScale + (1.0 - kPopupInitialScale) * progress;
    const QSize animatedPopupSize(qMax(1, qRound(popupSize.width() * scale)),
                                  qMax(1, qRound(popupSize.height() * scale)));
    const QSize containerSize(animatedPopupSize.width() + kPopupShadowMargin * 2,
                              animatedPopupSize.height() + kPopupShadowMargin * 2);
    const int x = (width() - containerSize.width()) / 2;
    const int y = (height() - containerSize.height()) / 2;
    m_popupContainer->setGeometry(QRect(QPoint(qMax(0, x), qMax(0, y)), containerSize));
    m_popupFrame->setGeometry(QRect(QPoint(kPopupShadowMargin, kPopupShadowMargin), animatedPopupSize));
}

QSize InWindowPopupOverlay::boundedPopupSize() const
{
    if (!m_content) {
        return QSize(320, 160);
    }

    QSize hint = m_content->sizeHint();
    if (!hint.isValid() || hint.isEmpty()) {
        hint = QSize(360, 180);
    }
    hint = hint.expandedTo(m_content->minimumSizeHint()).expandedTo(m_content->minimumSize());

    const QSize available(qMax(1, width() - m_options.popupMargins.left() - m_options.popupMargins.right()),
                          qMax(1, height() - m_options.popupMargins.top() - m_options.popupMargins.bottom()));
    QSize maxSize = m_options.maximumPopupSize;
    if (!maxSize.isValid() || maxSize.isEmpty()) {
        maxSize = available;
    } else {
        maxSize = maxSize.boundedTo(available);
    }

    const QSize contentMaximum = m_content->maximumSize();
    if (contentMaximum.width() < QWIDGETSIZE_MAX || contentMaximum.height() < QWIDGETSIZE_MAX) {
        maxSize = maxSize.boundedTo(contentMaximum);
    }

    return hint.boundedTo(maxSize).expandedTo(QSize(qMin(hint.width(), maxSize.width()),
                                                   qMin(hint.height(), maxSize.height())));
}

bool InWindowPopupOverlay::isInsidePopup(const QPoint& position) const
{
    if (!m_popupFrame) {
        return false;
    }
    const QRect popupRect(m_popupFrame->mapTo(this, QPoint(0, 0)), m_popupFrame->size());
    return popupRect.contains(position);
}

bool InWindowPopupOverlay::isOverlayChild(QObject* object) const
{
    if (!object) {
        return false;
    }
    if (object == this) {
        return true;
    }
    auto* widget = qobject_cast<QWidget*>(object);
    return widget && (widget == this || isAncestorOf(widget));
}

InWindowPopup::Button InWindowPopup::question(QWidget* parent,
                                             const QString& title,
                                             const QString& text,
                                             Button defaultButton)
{
    Button result = Button::No;
    QWidget* content = createMessageContent(title, text, defaultButton, &result);

    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(460, 260);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        return result;
    }

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

QString InWindowPopup::getText(QWidget* parent,
                               const QString& title,
                               const QString& label,
                               QLineEdit::EchoMode echo,
                               const QString& text,
                               bool* accepted)
{
    if (accepted) {
        *accepted = false;
    }

    auto* content = new QWidget;
    content->setMinimumWidth(380);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
    layout->setSpacing(14);
    layout->addWidget(createTitleLabel(title));
    layout->addWidget(createBodyLabel(label));

    auto* edit = new InlineEditableText(content);
    edit->setTrimTextOnCommit(false);
    edit->setEchoMode(echo);
    edit->setText(text);
    edit->setMinimumHeight(kInputHeight);
    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor selectionBackground = ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedTextColor = ThemeManager::textColorOn(
            colorCompositedOver(selectionBackground, inputBackground));
    edit->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    edit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    edit->setNormalBackgroundColor(inputBackground);
    edit->setHoverBackgroundColor(inputBackground);
    edit->setFocusBackgroundColor(inputBackground);
    edit->setNormalBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setSelectionBackgroundColor(selectionBackground);
    edit->setSelectedTextColor(selectedTextColor);
    edit->setBorderWidth(1);
    edit->setRadius(8);
    edit->setHorizontalPadding(10);
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(edit);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto* cancelButton = createButton(QStringLiteral("取消"), content);
    applyPopupDefaultButtonStyle(cancelButton);
    auto* okButton = createButton(QStringLiteral("确认"), content);
    okButton->setPrimaryStyle();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    QString result = text;
    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content);
    if (!overlay) {
        return QString();
    }

    auto acceptInput = [accepted, edit, overlay, &result]() {
        result = edit->text();
        if (accepted) {
            *accepted = true;
        }
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    QObject::connect(okButton, &QPushButton::clicked, edit, acceptInput);
    QObject::connect(edit, &InlineEditableText::returnPressed, edit, acceptInput);
    QObject::connect(cancelButton, &QPushButton::clicked, edit, [overlay]() {
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });

    QTimer::singleShot(0, edit, [edit]() {
        edit->startEditing();
    });

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}
