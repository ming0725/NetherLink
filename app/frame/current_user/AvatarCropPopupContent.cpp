#include "app/frame/current_user/AvatarCropPopupContent.h"

#include "app/frame/current_user/AvatarCropCanvas.h"
#include "app/frame/current_user/CurrentUserPopupStyle.h"
#include "shared/services/AppFonts.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QWheelEvent>
#include <QVBoxLayout>

using namespace CurrentUserPopupStyle;

namespace {

constexpr int kAvatarCropBottomBarHeight = 50;
constexpr int kAvatarCropToolButtonSize = 34;
constexpr int kAvatarCropSliderWidth = 198;
constexpr int kAvatarCropSliderHeight = 32;
constexpr int kAvatarCropSliderTrackHeight = 8;
constexpr int kAvatarCropSliderHandleHeight = 30;
constexpr int kAvatarCropSliderHandleMinWidth = 10;
constexpr int kAvatarCropSliderWheelStep = 42;
const QString kAvatarCropSliderHandleSource(QStringLiteral(":/resources/icon/mc_slider_handle.png"));
const QString kAvatarCropSliderHandleHighlightedSource(
        QStringLiteral(":/resources/icon/mc_slider_handle_highlighted.png"));

class AvatarCropToolButton final : public QPushButton
{
public:
    AvatarCropToolButton(const QString& iconSource,
                         const QString& tooltip,
                         QWidget* parent = nullptr)
        : QPushButton(parent)
        , m_icon(iconSource)
    {
        setFixedSize(kAvatarCropToolButtonSize, kAvatarCropToolButtonSize);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(tooltip);
        setFlat(true);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QColor fill = Qt::transparent;
        if (isDown()) {
            fill = ThemeManager::instance().color(ThemeColor::Divider);
        } else if (underMouse()) {
            fill = ThemeManager::instance().color(ThemeColor::ListHover);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

        const int iconExtent = 24;
        const QRect iconRect((width() - iconExtent) / 2,
                             (height() - iconExtent) / 2,
                             iconExtent,
                             iconExtent);
        const qreal dpr = devicePixelRatioF();
        QPixmap pixmap = m_icon.pixmap(QSize(qRound(iconExtent * dpr),
                                             qRound(iconExtent * dpr)));
        if (pixmap.isNull()) {
            return;
        }

        pixmap.setDevicePixelRatio(dpr);
        QPixmap tinted(pixmap.size());
        tinted.setDevicePixelRatio(dpr);
        tinted.fill(Qt::transparent);

        QPainter iconPainter(&tinted);
        iconPainter.drawPixmap(0, 0, pixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(tinted.rect(),
                             ThemeManager::instance().color(ThemeColor::PrimaryText));
        iconPainter.end();

        painter.drawPixmap(iconRect, tinted);
    }

private:
    QIcon m_icon;
};

} // namespace

class AvatarCropZoomSlider final : public QWidget
{
public:
    explicit AvatarCropZoomSlider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(kAvatarCropSliderWidth, kAvatarCropSliderHeight);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
    }

    void setRange(int minimum, int maximum)
    {
        if (minimum > maximum) {
            qSwap(minimum, maximum);
        }
        m_minimum = minimum;
        m_maximum = maximum;
        setValue(m_value);
        update();
    }

    void setValue(int value)
    {
        const int nextValue = qBound(m_minimum, value, m_maximum);
        if (m_value == nextValue) {
            return;
        }

        m_value = nextValue;
        update();
        if (valueChanged) {
            valueChanged(m_value);
        }
    }

    std::function<void(int)> valueChanged;

protected:
    bool event(QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            m_hovered = true;
            update();
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            if (!m_dragging) {
                m_hovered = false;
                update();
            }
            break;
        default:
            break;
        }
        return QWidget::event(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

        const QRect track = trackRect();
        const QColor border = ThemeManager::instance().isDark()
                ? QColor(0x18, 0x18, 0x18)
                : QColor(0x31, 0x31, 0x31);
        const QColor base = ThemeManager::instance().isDark()
                ? QColor(0x50, 0x50, 0x50)
                : QColor(0xB5, 0xB5, 0xB5);
        const QColor fill = ThemeManager::instance().color(ThemeColor::Accent);

        painter.setPen(Qt::NoPen);
        painter.setBrush(border);
        painter.drawRect(track);
        painter.setBrush(base);
        painter.drawRect(track.adjusted(2, 2, -2, -2));

        QRect progress = track.adjusted(2, 2, -2, -2);
        progress.setRight(handleRect().center().x());
        if (progress.isValid()) {
            painter.setBrush(fill);
            painter.drawRect(progress);
        }

        const QString handleSource = (m_hovered || m_dragging)
                ? kAvatarCropSliderHandleHighlightedSource
                : kAvatarCropSliderHandleSource;
        const QPixmap handlePixmap = ImageService::instance().pixmap(handleSource);
        const QRect handle = handleRect();
        if (handlePixmap.isNull()) {
            painter.setBrush(m_hovered || m_dragging ? QColor(0xEE, 0xEE, 0xEE)
                                                     : QColor(0xC6, 0xC6, 0xC6));
            painter.setPen(border);
            painter.drawRect(handle.adjusted(0, 0, -1, -1));
            return;
        }

        painter.drawPixmap(handle, handlePixmap, handlePixmap.rect());
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        m_dragging = true;
        m_hovered = true;
        setValue(valueFromPosition(event->position().toPoint().x()));
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_dragging) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        setValue(valueFromPosition(event->position().toPoint().x()));
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_dragging && event->button() == Qt::LeftButton) {
            m_dragging = false;
            m_hovered = rect().contains(event->position().toPoint());
            AudioService::instance().playButtonClick();
            update();
            event->accept();
            return;
        }

        QWidget::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const int delta = event->pixelDelta().isNull()
                ? event->angleDelta().y()
                : event->pixelDelta().y();
        if (delta == 0) {
            QWidget::wheelEvent(event);
            return;
        }

        setValue(m_value + (delta > 0 ? kAvatarCropSliderWheelStep : -kAvatarCropSliderWheelStep));
        event->accept();
    }

private:
    QRect trackRect() const
    {
        const int handleWidth = this->handleWidth();
        const int x = handleWidth / 2;
        const int y = (height() - kAvatarCropSliderTrackHeight) / 2;
        return QRect(x,
                     y,
                     qMax(1, width() - handleWidth),
                     kAvatarCropSliderTrackHeight);
    }

    QRect handleRect() const
    {
        const int widthForHandle = handleWidth();
        const int travel = qMax(0, width() - widthForHandle);
        const int range = qMax(1, m_maximum - m_minimum);
        const qreal progress = qreal(m_value - m_minimum) / qreal(range);
        return QRect(qRound(progress * travel),
                     (height() - kAvatarCropSliderHandleHeight) / 2,
                     widthForHandle,
                     kAvatarCropSliderHandleHeight);
    }

    int handleWidth() const
    {
        const QPixmap handlePixmap = ImageService::instance().pixmap(kAvatarCropSliderHandleSource);
        if (handlePixmap.isNull() || handlePixmap.height() <= 0) {
            return qMax(kAvatarCropSliderHandleMinWidth, kAvatarCropSliderHandleHeight / 2);
        }

        const int scaledWidth = qRound(qreal(handlePixmap.width())
                                       * kAvatarCropSliderHandleHeight
                                       / qreal(handlePixmap.height()));
        return qBound(kAvatarCropSliderHandleMinWidth, scaledWidth, width());
    }

    int valueFromPosition(int x) const
    {
        const int widthForHandle = handleWidth();
        const int travel = qMax(1, width() - widthForHandle);
        const int range = qMax(0, m_maximum - m_minimum);
        if (range == 0) {
            return m_minimum;
        }

        const int handleX = qBound(0, x - widthForHandle / 2, travel);
        const qreal progress = qreal(handleX) / qreal(travel);
        return m_minimum + qBound(0, qRound(progress * range), range);
    }

    int m_minimum = 0;
    int m_maximum = 1000;
    int m_value = 0;
    bool m_hovered = false;
    bool m_dragging = false;
};

AvatarCropPopupContent::AvatarCropPopupContent(const QImage& image, QWidget* parent)
    : QWidget(parent)
    , m_canvas(new AvatarCropCanvas(image, this))
    , m_zoomSlider(new AvatarCropZoomSlider(this))
    , m_cancelButton(new StatefulPushButton(QStringLiteral("取消"), this))
    , m_confirmButton(new StatefulPushButton(QStringLiteral("使用"), this))
{
    setMinimumSize(420, 492);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 22);
    layout->setSpacing(12);

    auto* title = makeLabel(QStringLiteral("调整头像"), ThemeColor::PrimaryText, 20, this);
    title->setFont(AppFonts::applicationPixelSizedFont(20, true));
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    layout->addWidget(title);
    layout->addWidget(m_canvas, 0, Qt::AlignHCenter);

    auto* zoomBar = new QWidget(this);
    zoomBar->setFixedHeight(kAvatarCropBottomBarHeight);
    auto* zoomLayout = new QHBoxLayout(zoomBar);
    zoomLayout->setContentsMargins(14, 6, 14, 6);
    zoomLayout->setSpacing(8);
    zoomLayout->addStretch(1);

    auto* zoomOutButton = new AvatarCropToolButton(
            QStringLiteral(":/resources/icon/image_viewer_zoom_out.svg"),
            QStringLiteral("缩小"),
            zoomBar);
    auto* zoomInButton = new AvatarCropToolButton(
            QStringLiteral(":/resources/icon/image_viewer_zoom_in.svg"),
            QStringLiteral("放大"),
            zoomBar);

    m_zoomSlider->setRange(0, 1000);
    m_zoomSlider->setValue(m_canvas->zoomValue());

    zoomLayout->addWidget(zoomOutButton);
    zoomLayout->addWidget(m_zoomSlider);
    zoomLayout->addWidget(zoomInButton);
    zoomLayout->addStretch(1);
    layout->addWidget(zoomBar);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 2, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch(1);
    m_cancelButton->setFixedSize(92, 34);
    m_confirmButton->setFixedSize(92, 34);
    applyDefaultButtonStyle(m_cancelButton);
    m_confirmButton->setPrimaryStyle();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    layout->addLayout(buttonLayout);

    connect(zoomOutButton, &QPushButton::clicked, m_canvas, &AvatarCropCanvas::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, m_canvas, &AvatarCropCanvas::zoomIn);
    m_zoomSlider->valueChanged = [this](int value) {
        if (m_syncingZoom) {
            return;
        }
        m_canvas->setZoomValue(value);
    };
    m_canvas->zoomValueChanged = [this](int value) {
        m_syncingZoom = true;
        m_zoomSlider->setValue(value);
        m_syncingZoom = false;
    };
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (cancelRequested) {
            cancelRequested();
        }
    });
    connect(m_confirmButton, &QPushButton::clicked, this, [this]() {
        if (accepted) {
            accepted(m_canvas->croppedImage());
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyDefaultButtonStyle(m_cancelButton);
        m_confirmButton->setPrimaryStyle();
        m_canvas->update();
        m_zoomSlider->update();
        update();
    });
}
