#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/ui/FriendProfilePopup.h"
#include "shared/services/AppFonts.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/ui/InWindowPopupOverlay.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"
#include "app/state/CurrentUser.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>
#include <cmath>

namespace {

constexpr int kAvatarStatusCutoutSize = 18;
constexpr int kAvatarStatusIconSize = 13;
constexpr int kAvatarStatusPopupGap = 8;
constexpr int kAvatarStatusCenterOffset = -2;
constexpr int kStatusChoiceRadius = 8;
constexpr int kStatusIconChoiceCount = 4;
constexpr int kStatusChoiceIconSize = 72;
constexpr int kProfileEditAvatarSize = 74;
constexpr int kProfileEditInputHeight = 36;
constexpr int kProfileEditLabelWidth = 62;
constexpr int kAvatarHoverAnimationDurationMs = 160;
constexpr int kAvatarCropPreviewSize = 320;
constexpr int kAvatarCropOutputSize = 512;
constexpr int kAvatarCropBottomBarHeight = 50;
constexpr int kAvatarCropToolButtonSize = 34;
constexpr int kAvatarCropSliderWidth = 198;
constexpr int kAvatarCropSliderHeight = 32;
constexpr int kAvatarCropSliderTrackHeight = 8;
constexpr int kAvatarCropSliderHandleHeight = 30;
constexpr int kAvatarCropSliderHandleMinWidth = 10;
constexpr int kAvatarCropSliderWheelStep = 42;
constexpr qreal kAvatarCropMaxScaleMultiplier = 4.5;
const QString kAvatarCropSliderHandleSource(
        QStringLiteral(":/resources/icon/mc_slider_handle.png"));
const QString kAvatarCropSliderHandleHighlightedSource(
        QStringLiteral(":/resources/icon/mc_slider_handle_highlighted.png"));

struct StatusChoice {
    QString title;
    QString iconPath;
    int index = 0;
};

QColor colorCompositedOver(const QColor& foreground, const QColor& background)
{
    const QColor fg = foreground.toRgb();
    const QColor bg = background.toRgb();
    const qreal alpha = fg.alphaF();
    return QColor(qRound(fg.red() * alpha + bg.red() * (1.0 - alpha)),
                  qRound(fg.green() * alpha + bg.green() * (1.0 - alpha)),
                  qRound(fg.blue() * alpha + bg.blue() * (1.0 - alpha)));
}

PaintedLabel* makeProfileEditLabel(const QString& text,
                                   ThemeColor role,
                                   int pixelSize,
                                   QWidget* parent)
{
    auto* label = new PaintedLabel(text, parent);
    label->setProperty("themeTextRole", static_cast<int>(role));
    label->setTextColor(ThemeManager::instance().color(role));
    label->setFont(AppFonts::applicationPixelSizedFont(pixelSize, false));
    return label;
}

void applyProfileEditInputStyle(InlineEditableText* edit)
{
    if (!edit) {
        return;
    }

    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor selectionBackground =
            ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedText = ThemeManager::textColorOn(
            colorCompositedOver(selectionBackground, inputBackground));

    edit->setFont(AppFonts::applicationPixelSizedFont(14, false));
    edit->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    edit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    edit->setNormalBackgroundColor(inputBackground);
    edit->setHoverBackgroundColor(inputBackground);
    edit->setFocusBackgroundColor(inputBackground);
    edit->setNormalBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Accent));
    edit->setSelectionBackgroundColor(selectionBackground);
    edit->setSelectedTextColor(selectedText);
    edit->setBorderWidth(1);
    edit->setRadius(8);
    edit->setHorizontalPadding(12);
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void applyPopupDefaultButtonStyle(StatefulPushButton* button)
{
    if (!button) {
        return;
    }

    button->setDefaultStyle();
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

QString saveAvatarImageToAppData(const QImage& image, const QString& userId)
{
    if (image.isNull()) {
        return {};
    }

    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + QStringLiteral("/NetherLink-static");
    }

    const QString avatarDirPath = root + QStringLiteral("/avatars");
    QDir avatarDir;
    if (!avatarDir.mkpath(avatarDirPath)) {
        return {};
    }

    const QString safeUserId = userId.isEmpty() ? QStringLiteral("user") : userId;
    const QString fileName = QStringLiteral("%1_%2.png")
            .arg(safeUserId,
                 QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString filePath = QDir(avatarDirPath).filePath(fileName);
    return image.save(filePath, "PNG") ? filePath : QString();
}

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
        iconPainter.fillRect(tinted.rect(), ThemeManager::instance().color(ThemeColor::PrimaryText));
        iconPainter.end();

        painter.drawPixmap(iconRect, tinted);
    }

private:
    QIcon m_icon;
};

class AvatarCropCanvas final : public QWidget
{
public:
    explicit AvatarCropCanvas(const QImage& image, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_image(image)
    {
        setFixedSize(kAvatarCropPreviewSize, kAvatarCropPreviewSize);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::SizeAllCursor);
        resetTransform();
    }

    QImage croppedImage() const
    {
        if (m_image.isNull()) {
            return {};
        }

        QImage output(QSize(kAvatarCropOutputSize, kAvatarCropOutputSize),
                      QImage::Format_ARGB32_Premultiplied);
        output.fill(Qt::transparent);

        const QRectF sourceRect(-m_offset.x() / m_scale,
                                -m_offset.y() / m_scale,
                                width() / m_scale,
                                height() / m_scale);
        QPainter painter(&output);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(QRectF(QPointF(0, 0), QSizeF(output.size())),
                          m_image,
                          sourceRect);
        return output;
    }

    int zoomValue() const
    {
        if (qFuzzyCompare(m_minScale, m_maxScale)) {
            return 0;
        }
        const qreal normalized = std::log(m_scale / m_minScale) / std::log(m_maxScale / m_minScale);
        return qBound(0, qRound(normalized * 1000.0), 1000);
    }

    void setZoomValue(int value)
    {
        const qreal normalized = qBound(0.0, value / 1000.0, 1.0);
        setScale(m_minScale * std::pow(m_maxScale / m_minScale, normalized),
                 QRectF(rect()).center());
    }

    void zoomIn()
    {
        setScale(m_scale * 1.15, QRectF(rect()).center());
    }

    void zoomOut()
    {
        setScale(m_scale / 1.15, QRectF(rect()).center());
    }

    std::function<void(int)> zoomValueChanged;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

        if (!m_image.isNull()) {
            painter.drawImage(imageTargetRect(), m_image);
        }

        QPainterPath outer;
        outer.addRect(rect());
        QPainterPath cropCircle;
        cropCircle.addEllipse(cropRect());

        QColor mask(0, 0, 0, ThemeManager::instance().isDark() ? 118 : 92);
        painter.fillPath(outer.subtracted(cropCircle), mask);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 230), 2));
        painter.drawEllipse(cropRect().adjusted(1, 1, -1, -1));

        painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const int delta = event->pixelDelta().isNull()
                ? event->angleDelta().y()
                : event->pixelDelta().y();
        if (delta != 0) {
            const qreal factor = std::pow(1.0018, delta);
            setScale(m_scale * factor, event->position());
            event->accept();
            return;
        }
        QWidget::wheelEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && !m_image.isNull()) {
            m_dragging = true;
            m_dragStartPos = event->pos();
            m_dragStartOffset = m_offset;
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        setCursor(Qt::SizeAllCursor);
        if (m_dragging) {
            const QPoint delta = event->pos() - m_dragStartPos;
            m_offset = m_dragStartOffset + QPointF(delta);
            clampOffset();
            update();
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void enterEvent(QEnterEvent* event) override
    {
        setCursor(Qt::SizeAllCursor);
        QWidget::enterEvent(event);
    }

private:
    QRectF cropRect() const
    {
        const int inset = 0;
        return QRectF(rect()).adjusted(inset, inset, -inset, -inset);
    }

    QRectF imageTargetRect() const
    {
        return QRectF(m_offset,
                      QSizeF(m_image.width() * m_scale,
                             m_image.height() * m_scale));
    }

    void resetTransform()
    {
        if (m_image.isNull()) {
            return;
        }

        m_minScale = qMax(width() / qreal(m_image.width()),
                          height() / qreal(m_image.height()));
        m_maxScale = qMax(m_minScale, m_minScale * kAvatarCropMaxScaleMultiplier);
        m_scale = m_minScale;
        m_offset = QPointF((width() - m_image.width() * m_scale) / 2.0,
                           (height() - m_image.height() * m_scale) / 2.0);
        clampOffset();
        notifyZoomChanged();
    }

    void setScale(qreal scale, const QPointF& anchor)
    {
        if (m_image.isNull()) {
            return;
        }

        const qreal nextScale = qBound(m_minScale, scale, m_maxScale);
        if (qFuzzyCompare(m_scale, nextScale)) {
            return;
        }

        const QPointF imageAnchor = (anchor - m_offset) / m_scale;
        m_scale = nextScale;
        m_offset = anchor - imageAnchor * m_scale;
        clampOffset();
        notifyZoomChanged();
        update();
    }

    void clampOffset()
    {
        const QSizeF scaledSize(m_image.width() * m_scale, m_image.height() * m_scale);
        if (scaledSize.width() <= width()) {
            m_offset.setX((width() - scaledSize.width()) / 2.0);
        } else {
            m_offset.setX(qBound(width() - scaledSize.width(), m_offset.x(), 0.0));
        }

        if (scaledSize.height() <= height()) {
            m_offset.setY((height() - scaledSize.height()) / 2.0);
        } else {
            m_offset.setY(qBound(height() - scaledSize.height(), m_offset.y(), 0.0));
        }
    }

    void notifyZoomChanged()
    {
        if (zoomValueChanged) {
            zoomValueChanged(zoomValue());
        }
    }

    QImage m_image;
    qreal m_minScale = 1.0;
    qreal m_maxScale = 1.0;
    qreal m_scale = 1.0;
    QPointF m_offset;
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QPointF m_dragStartOffset;
};

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

    int value() const
    {
        return m_value;
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
            painter.setBrush(m_hovered || m_dragging ? QColor(0xEE, 0xEE, 0xEE) : QColor(0xC6, 0xC6, 0xC6));
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

class AvatarCropPopupContent final : public QWidget
{
public:
    explicit AvatarCropPopupContent(const QImage& image, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_title(makeProfileEditLabel(QStringLiteral("调整头像"),
                                       ThemeColor::PrimaryText,
                                       20,
                                       this))
        , m_canvas(new AvatarCropCanvas(image, this))
        , m_zoomSlider(new AvatarCropZoomSlider(this))
        , m_cancelButton(new StatefulPushButton(QStringLiteral("取消"), this))
        , m_confirmButton(new StatefulPushButton(QStringLiteral("使用"), this))
    {
        setMinimumSize(420, 492);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(26, 22, 26, 22);
        layout->setSpacing(12);

        m_title->setFont(AppFonts::applicationPixelSizedFont(20, true));
        m_title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        layout->addWidget(m_title);
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
        applyPopupDefaultButtonStyle(m_cancelButton);
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
            applyTheme();
        });

        applyTheme();
    }

    std::function<void(const QImage&)> accepted;
    std::function<void()> cancelRequested;

private:
    void applyTheme()
    {
        m_title->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
        applyPopupDefaultButtonStyle(m_cancelButton);
        m_confirmButton->setPrimaryStyle();
        m_canvas->update();
        m_zoomSlider->update();
        update();
    }

    PaintedLabel* m_title = nullptr;
    AvatarCropCanvas* m_canvas = nullptr;
    AvatarCropZoomSlider* m_zoomSlider = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_confirmButton = nullptr;
    bool m_syncingZoom = false;
};

class ProfileAvatarPreview final : public QWidget
{
public:
    explicit ProfileAvatarPreview(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_hoverAnimation(new QVariantAnimation(this))
    {
        setFixedSize(kProfileEditAvatarSize, kProfileEditAvatarSize);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        m_hoverAnimation->setDuration(kAvatarHoverAnimationDurationMs);
        m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_hoverProgress = value.toReal();
            update();
        });
    }

    void setAvatarSource(const QString& source)
    {
        if (m_avatarSource == source) {
            return;
        }
        m_avatarSource = source;
        update();
    }

    std::function<void(const QImage&)> avatarImageSelected;

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            animateHover(1.0);
        } else if (event->type() == QEvent::Leave) {
            animateHover(0.0);
        }
        return QWidget::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRect avatarRect((width() - kProfileEditAvatarSize) / 2,
                               (height() - kProfileEditAvatarSize) / 2,
                               kProfileEditAvatarSize,
                               kProfileEditAvatarSize);
        const QPixmap avatar = ImageService::instance().circularAvatar(m_avatarSource,
                                                                       kProfileEditAvatarSize,
                                                                       devicePixelRatioF());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0));
        painter.drawEllipse(avatarRect);

        if (!avatar.isNull()) {
            painter.save();
            painter.setOpacity(1.0 - m_hoverProgress * 0.48);
            painter.drawPixmap(avatarRect, avatar);
            painter.restore();
        } else {
            painter.setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
            painter.drawEllipse(avatarRect);
        }

        if (m_hoverProgress > 0.01) {
            painter.save();
            painter.setOpacity(m_hoverProgress);
            const int iconSize = 30;
            const QRect iconRect(avatarRect.center().x() - iconSize / 2,
                                 avatarRect.center().y() - iconSize / 2,
                                 iconSize,
                                 iconSize);
            QPixmap icon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/painting.png"),
                                                           iconRect.size(),
                                                           Qt::KeepAspectRatio,
                                                           devicePixelRatioF());
            if (!icon.isNull()) {
                painter.drawPixmap(iconRect, icon);
            }
            painter.restore();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            AudioService::instance().playButtonClick();
            QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            if (defaultPath.isEmpty()) {
                defaultPath = QDir::homePath();
            }
            const QString filePath = QFileDialog::getOpenFileName(
                    this,
                    QStringLiteral("选择头像"),
                    defaultPath,
                    QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
            if (!filePath.isEmpty()) {
                QImageReader reader(filePath);
                reader.setAutoTransform(true);
                const QImage image = reader.read();
                if (!image.isNull() && avatarImageSelected) {
                    avatarImageSelected(image);
                }
            }
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    void animateHover(qreal target)
    {
        if (qFuzzyCompare(m_hoverProgress, target)) {
            return;
        }
        m_hoverAnimation->stop();
        m_hoverAnimation->setStartValue(m_hoverProgress);
        m_hoverAnimation->setEndValue(target);
        m_hoverAnimation->start();
    }

    QString m_avatarSource;
    qreal m_hoverProgress = 0.0;
    QVariantAnimation* m_hoverAnimation = nullptr;
};

class CurrentUserProfileEditContent final : public QWidget
{
public:
    explicit CurrentUserProfileEditContent(const CurrentUserProfile& profile,
                                           QWidget* parent = nullptr)
        : QWidget(parent)
        , m_profile(profile)
        , m_title(makeProfileEditLabel(QStringLiteral("编辑资料"),
                                       ThemeColor::PrimaryText,
                                       20,
                                       this))
        , m_avatar(new ProfileAvatarPreview(this))
        , m_idLabel(makeProfileEditLabel(QString(), ThemeColor::TertiaryText, 12, this))
        , m_nameEdit(new InlineEditableText(this))
        , m_regionEdit(new InlineEditableText(this))
        , m_signatureEdit(new InlineEditableText(this))
        , m_cancelButton(new StatefulPushButton(QStringLiteral("取消"), this))
        , m_saveButton(new StatefulPushButton(QStringLiteral("保存"), this))
    {
        setMinimumSize(420, 420);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(26, 24, 26, 24);
        layout->setSpacing(12);

        m_title->setFont(AppFonts::applicationPixelSizedFont(20, true));
        m_title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        layout->addWidget(m_title);

        m_avatar->setAvatarSource(m_profile.avatarPath);
        m_avatar->avatarImageSelected = [this](const QImage& image) {
            showAvatarCropPopup(image);
        };
        layout->addWidget(m_avatar, 0, Qt::AlignHCenter);

        m_idLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_idLabel->setText(QStringLiteral("ID %1").arg(m_profile.userId));
        layout->addWidget(m_idLabel);
        layout->addSpacing(2);

        configureEdit(m_nameEdit, QStringLiteral("昵称"), m_profile.nickName);
        configureEdit(m_regionEdit, QStringLiteral("地区"), m_profile.region);
        configureEdit(m_signatureEdit, QStringLiteral("个性签名"), m_profile.signature);

        layout->addWidget(createInputRow(QStringLiteral("昵称"), m_nameEdit));
        layout->addWidget(createInputRow(QStringLiteral("地区"), m_regionEdit));
        layout->addWidget(createInputRow(QStringLiteral("签名"), m_signatureEdit));
        layout->addStretch(1);

        auto* buttonLayout = new QHBoxLayout;
        buttonLayout->setContentsMargins(0, 4, 0, 0);
        buttonLayout->setSpacing(8);
        buttonLayout->addStretch(1);

        m_cancelButton->setFixedSize(86, 32);
        m_saveButton->setFixedSize(86, 32);
        applyPopupDefaultButtonStyle(m_cancelButton);
        m_saveButton->setPrimaryStyle();

        buttonLayout->addWidget(m_cancelButton);
        buttonLayout->addWidget(m_saveButton);
        layout->addLayout(buttonLayout);

        connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
            if (cancelRequested) {
                cancelRequested();
            }
        });
        connect(m_saveButton, &QPushButton::clicked, this, [this]() {
            if (saveRequested) {
                saveRequested(editedProfile());
            }
        });
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            applyTheme();
        });

        applyTheme();
    }

    std::function<void(const CurrentUserProfile&)> saveRequested;
    std::function<void()> cancelRequested;

private:
    void configureEdit(InlineEditableText* edit, const QString& placeholder, const QString& text)
    {
        edit->setMinimumHeight(kProfileEditInputHeight);
        edit->setPlaceholderText(placeholder);
        edit->setText(text);
        applyProfileEditInputStyle(edit);
    }

    QWidget* createInputRow(const QString& title, InlineEditableText* edit)
    {
        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto* titleLabel = makeProfileEditLabel(title, ThemeColor::SecondaryText, 13, row);
        titleLabel->setFixedWidth(kProfileEditLabelWidth);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        edit->setParent(row);
        rowLayout->addWidget(titleLabel);
        rowLayout->addWidget(edit, 1);
        return row;
    }

    CurrentUserProfile editedProfile() const
    {
        CurrentUserProfile next = m_profile;
        next.nickName = m_nameEdit->text().trimmed();
        if (next.nickName.isEmpty()) {
            next.nickName = next.userId;
        }
        next.avatarPath = m_profile.avatarPath;
        next.region = m_regionEdit->text().trimmed();
        next.signature = m_signatureEdit->text().trimmed();
        return next;
    }

    void showAvatarCropPopup(const QImage& image)
    {
        if (image.isNull()) {
            return;
        }
        if (m_avatarCropPopup) {
            m_avatarCropPopup->raise();
            return;
        }

        auto* content = new AvatarCropPopupContent(image);
        InWindowPopupOverlay::Options options;
        options.maximumPopupSize = QSize(480, 560);
        options.dismissOnOutsideClick = true;
        options.dismissOnEscape = true;

        m_avatarCropPopup = InWindowPopupOverlay::showPopup(this, content, options);
        if (!m_avatarCropPopup) {
            return;
        }

        content->accepted = [this](const QImage& croppedImage) {
            const QString savedPath = saveAvatarImageToAppData(croppedImage, m_profile.userId);
            if (!savedPath.isEmpty()) {
                m_profile.avatarPath = savedPath;
                m_avatar->setAvatarSource(savedPath);
            }
            if (m_avatarCropPopup) {
                m_avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
            }
        };
        content->cancelRequested = [this]() {
            if (m_avatarCropPopup) {
                m_avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
            }
        };

        connect(m_avatarCropPopup,
                &InWindowPopupOverlay::dismissed,
                this,
                [this]() {
                    m_avatarCropPopup = nullptr;
                });
    }

    void applyTheme()
    {
        const QList<PaintedLabel*> labels = findChildren<PaintedLabel*>();
        for (PaintedLabel* label : labels) {
            const QVariant roleValue = label->property("themeTextRole");
            if (!roleValue.isValid()) {
                continue;
            }
            label->setTextColor(ThemeManager::instance().color(
                    static_cast<ThemeColor>(roleValue.toInt())));
        }

        applyProfileEditInputStyle(m_nameEdit);
        applyProfileEditInputStyle(m_regionEdit);
        applyProfileEditInputStyle(m_signatureEdit);
        applyPopupDefaultButtonStyle(m_cancelButton);
        m_saveButton->setPrimaryStyle();
        if (m_avatar) {
            m_avatar->update();
        }
        update();
    }

    CurrentUserProfile m_profile;
    PaintedLabel* m_title = nullptr;
    ProfileAvatarPreview* m_avatar = nullptr;
    PaintedLabel* m_idLabel = nullptr;
    InlineEditableText* m_nameEdit = nullptr;
    InlineEditableText* m_regionEdit = nullptr;
    InlineEditableText* m_signatureEdit = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_saveButton = nullptr;
    QPointer<InWindowPopupOverlay> m_avatarCropPopup;
};

class StatusChoiceCard final : public QWidget
{
public:
    StatusChoiceCard(const StatusChoice& choice, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_choiceIndex(choice.index)
        , m_icon(new QLabel(this))
        , m_title(new PaintedLabel(choice.title, this))
        , m_iconPath(choice.iconPath)
    {
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setMinimumSize(132, 178);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_icon->setFixedSize(kStatusChoiceIconSize, kStatusChoiceIconSize);
        m_icon->setAlignment(Qt::AlignCenter);

        m_title->setFont(AppFonts::applicationPixelSizedFont(16, true));
        m_title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 22, 18, 18);
        layout->setSpacing(18);
        layout->addStretch(1);
        layout->addWidget(m_icon, 0, Qt::AlignHCenter);
        layout->addWidget(m_title);
        layout->addStretch(1);

        refreshColors();
        refreshIcon();
    }

    int choiceIndex() const { return m_choiceIndex; }

    void setSelected(bool selected)
    {
        if (m_selected == selected) {
            return;
        }
        m_selected = selected;
        refreshColors();
        update();
    }

    std::function<void(int)> clicked;

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            update();
        }
        return QWidget::event(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            AudioService::instance().playButtonClick();
            if (clicked) {
                clicked(m_choiceIndex);
            }
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
        QColor fill = Qt::transparent;
        QColor stroke = ThemeManager::instance().color(ThemeColor::Divider);
        int strokeWidth = 1;

        if (m_hovered) {
            stroke = QColor(0xac, 0xac, 0xac, 128);
        }
        if (m_selected) {
            stroke = accent;
            strokeWidth = 2;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(cardRect, kStatusChoiceRadius, kStatusChoiceRadius);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(stroke, strokeWidth));
        painter.drawRoundedRect(cardRect.adjusted(strokeWidth / 2.0,
                                                  strokeWidth / 2.0,
                                                  -strokeWidth / 2.0,
                                                  -strokeWidth / 2.0),
                                kStatusChoiceRadius,
                                kStatusChoiceRadius);
    }

private:
    void refreshColors()
    {
        const QColor titleColor = m_selected
                ? ThemeManager::instance().color(ThemeColor::Accent)
                : ThemeManager::instance().color(ThemeColor::PrimaryText);
        m_title->setTextColor(titleColor);
    }

    void refreshIcon()
    {
        const QPixmap pixmap = ImageService::instance().scaled(m_iconPath,
                                                              m_icon->size(),
                                                              Qt::KeepAspectRatio,
                                                              devicePixelRatioF());
        m_icon->setPixmap(pixmap);
    }

    int m_choiceIndex = 0;
    QLabel* m_icon = nullptr;
    PaintedLabel* m_title = nullptr;
    QString m_iconPath;
    bool m_selected = false;
    bool m_hovered = false;
};

class CurrentUserStatusPopupContent final : public QWidget
{
public:
    CurrentUserStatusPopupContent(int selectedIndex, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(660, 280);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(34, 28, 34, 34);
        layout->setSpacing(26);

        auto* title = new PaintedLabel(QStringLiteral("选择在线状态"), this);
        title->setFont(AppFonts::applicationPixelSizedFont(20, true));
        title->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
        layout->addWidget(title);

        auto* choicesLayout = new QHBoxLayout;
        choicesLayout->setContentsMargins(0, 0, 0, 0);
        choicesLayout->setSpacing(18);
        layout->addLayout(choicesLayout, 1);

        const QVector<StatusChoice> choices = {
                {QStringLiteral("在线"), statusIconPath(Online), 0},
                {QStringLiteral("挖矿中"), statusIconPath(Mining), 1},
                {QStringLiteral("飞行模式"), statusIconPath(Flying), 2},
                {QStringLiteral("隐身"), QStringLiteral(":/resources/icon/invisible.png"), 3}
        };

        for (int i = 0; i < choices.size(); ++i) {
            auto* card = new StatusChoiceCard(choices.at(i), this);
            card->clicked = [this](int index) {
                selectStatus(index);
            };
            m_cards.append(card);
            choicesLayout->addWidget(card, 1);
        }

        setSelectedStatus(selectedIndex);
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            for (StatusChoiceCard* card : m_cards) {
                card->setSelected(card->choiceIndex() == m_selectedIndex);
            }
            update();
        });
    }

    std::function<void(int)> selectionChanged;

private:
    void selectStatus(int index)
    {
        if (index < 0 || index >= kStatusIconChoiceCount) {
            return;
        }
        setSelectedStatus(index);
        if (selectionChanged) {
            selectionChanged(index);
        }
    }

    void setSelectedStatus(int index)
    {
        m_selectedIndex = index;
        for (StatusChoiceCard* card : m_cards) {
            card->setSelected(card->choiceIndex() == index);
        }
    }

    QVector<StatusChoiceCard*> m_cards;
    int m_selectedIndex = 0;
};

} // namespace

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);

#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    setAvatarSource(CurrentUser::instance().getAvatarPath());
    connect(&CurrentUser::instance(), &CurrentUser::identityChanged, this, [this]() {
        setAvatarSource(CurrentUser::instance().getAvatarPath());
        update();
    });

    messageItem = new ApplicationBarItem(
            ":/resources/icon/unselected_message.png",
            ":/resources/icon/selected_message.png");
    addItem(messageItem);
    friendItem = new ApplicationBarItem(
            ":/resources/icon/friend_unselected.png",
            ":/resources/icon/friend_selected.png");
    friendItem->setPixmapScale(0.62);
    addItem(friendItem);

    auto momentItem = new ApplicationBarItem(
            ":/resources/icon/unselected_blazer.png",
            ":/resources/icon/blazer.png");
    momentItem->setPixmapScale(0.68);
    addItem(momentItem);

    auto aiChat = new ApplicationBarItem(
            ":/resources/icon/unselected_aichat.png",
            ":/resources/icon/aichat.png");
    aiChat->setPixmapScale(0.77);
    addItem(aiChat);

    auto notItem = new ApplicationBarItem(
            ":/resources/icon/unselected_nether.png",
            ":/resources/icon/nether.png");
    notItem->setPixmapScale(0.72);
    notItem->setDarkModeInversionEnabled(false);
    addItem(notItem);

    moreOptionsItem = new ApplicationBarItem(":/resources/icon/menu.png");
    moreOptionsItem->setPixmapScale(0.57);
    addBottomItem(moreOptionsItem);

    if (!topItems.empty()) {
        selectedItem = topItems[0];
        topItems[0]->setSelected(true);
        highlightPosY = selectedItem->y();
    }
    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(250);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        highlightPosY = value.toInt();
        update();
    });

    connect(&MessageRepository::instance(), &MessageRepository::conversationListChanged,
            this, [this](const QString&) { refreshChatBadge(); });
    connect(&FriendNotificationRepository::instance(), &FriendNotificationRepository::notificationListChanged,
            this, [this]() { refreshFriendBadge(); });
    connect(&GroupNotificationRepository::instance(), &GroupNotificationRepository::notificationListChanged,
            this, [this]() { refreshFriendBadge(); });
    refreshChatBadge();
    refreshFriendBadge();
}

void ApplicationBar::resizeEvent(QResizeEvent*) {
    layoutItems();
}

void ApplicationBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
#ifdef Q_OS_WIN
    QColor windowColor = ThemeManager::instance().color(ThemeColor::PanelBackground);
    windowColor.setAlpha(128);
    painter.fillRect(rect(), windowColor);
#endif
    int w = width();
    if (selectedItem) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::AppBarItemSelectedBackground));
        int x = (width() - iconSize) / 2;
        QRect r(x, highlightPosY, iconSize, iconSize);
        painter.drawRoundedRect(r, 10, 10);
        painter.restore();
    }

    const QRect avatar = avatarRect();
    if (!avatarSource.isEmpty()) {
        painter.save();
        const qreal dpr = painter.device()->devicePixelRatioF();
        const QPixmap avatarPixmap = ImageService::instance().circularAvatar(avatarSource,
                                                                             avatarSize,
                                                                             dpr);
        QPainterPath avatarPath;
        avatarPath.addEllipse(avatar);
        QPainterPath statusCutoutPath;
        statusCutoutPath.addEllipse(avatarCutoutRect());
        painter.setClipPath(avatarPath.subtracted(statusCutoutPath));
        painter.drawPixmap(avatar, avatarPixmap);
        painter.restore();

        painter.save();
        const QRect status = avatarStatusIconRect();
        QPainterPath statusPath;
        statusPath.addEllipse(status);
        painter.setClipPath(statusPath);
        const QPixmap statusPixmap = ImageService::instance().scaled(avatarStatusIconSource(),
                                                                     status.size(),
                                                                     Qt::KeepAspectRatio,
                                                                     painter.device()->devicePixelRatioF());
        if (!statusPixmap.isNull()) {
            painter.drawPixmap(status, statusPixmap);
        }
        painter.restore();
    }

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        item->paint(painter);
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        item->paint(painter);
    }
}

void ApplicationBar::addItem(ApplicationBarItem* item)
{
    if (!item) {
        return;
    }

    topItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
    layoutItems();
}

void ApplicationBar::setAvatarSource(const QString& source)
{
    avatarSource = source;
    layoutItems();
}

void ApplicationBar::setTopInset(int inset)
{
    const int clampedInset = qMax(0, inset);
    if (topInset == clampedInset) {
        return;
    }

    topInset = clampedInset;
    layoutItems();
    update();
}

void ApplicationBar::setCurrentTopIndex(int index)
{
    if (index < 0 || index >= topItems.size()) {
        return;
    }

    onItemClicked(topItems.at(index));
}


void ApplicationBar::layoutItems() {
    int y = topInset + marginTop + spacing + avatarSize + avatarAndItemDist;
    int w = width();

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, y, iconSize, iconSize));
        y += iconSize + spacing;
    }
    int yb = height() - marginBottom - iconSize;
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, yb, iconSize, iconSize));
        yb -= (iconSize + spacing);
    }

    if (selectedItem && highlightAnim->state() != QAbstractAnimation::Running) {
        highlightPosY = selectedItem->y();
    }
}

void ApplicationBar::refreshChatBadge()
{
    if (!messageItem) {
        return;
    }

    int totalPromptUnreadCount = 0;
    const QVector<ConversationSummary> conversations =
            MessageRepository::instance().requestConversationList();
    for (const ConversationSummary& conversation : conversations) {
        if (conversation.isDoNotDisturb) {
            continue;
        }

        totalPromptUnreadCount += qMax(0, conversation.unreadCount);
    }
    messageItem->setBadgeCount(totalPromptUnreadCount);
}

void ApplicationBar::refreshFriendBadge()
{
    if (!friendItem) {
        return;
    }

    const int totalUnreadCount =
            qMax(0, FriendNotificationRepository::instance().unreadCount()) +
            qMax(0, GroupNotificationRepository::instance().unreadCount());
    friendItem->setBadgeCount(totalUnreadCount);
}

QRect ApplicationBar::avatarRect() const
{
    const int x = (width() - avatarSize) / 2;
    const int y = topInset + marginTop + spacing;
    return QRect(x, y, avatarSize, avatarSize);
}

QRect ApplicationBar::avatarStatusRect() const
{
    const QRect avatar = avatarRect();
    const QPoint center(avatar.right() + kAvatarStatusCenterOffset,
                        avatar.bottom() + kAvatarStatusCenterOffset);
    return QRect(center.x() - kAvatarStatusCutoutSize / 2,
                 center.y() - kAvatarStatusCutoutSize / 2,
                 kAvatarStatusCutoutSize,
                 kAvatarStatusCutoutSize);
}

QRect ApplicationBar::avatarCutoutRect() const
{
    return avatarStatusRect();
}

QRect ApplicationBar::avatarStatusIconRect() const
{
    const QPoint center = avatarStatusRect().center();
    return QRect(center.x() - kAvatarStatusIconSize / 2,
                 center.y() - kAvatarStatusIconSize / 2,
                 kAvatarStatusIconSize,
                 kAvatarStatusIconSize);
}

void ApplicationBar::onItemClicked(ApplicationBarItem* item)
{
    if (!item || !topItems.contains(item) || selectedItem == item)
        return;

    if (selectedItem) selectedItem->setSelected(false);
    item->setSelected(true);

    int startY = highlightPosY;
    int endY = item->y();
    highlightAnim->stop();
    highlightAnim->setStartValue(startY);
    highlightAnim->setEndValue(endY);
    highlightAnim->start();

    selectedItem = item;
    emit applicationClicked(item);
}

void ApplicationBar::addBottomItem(ApplicationBarItem* item)
{
    if (!item) {
        return;
    }

    bottomItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
    layoutItems();
}

void ApplicationBar::mouseMoveEvent(QMouseEvent* event)
{
    setHoveredItem(itemAtPosition(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void ApplicationBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!avatarSource.isEmpty()) {
            if (avatarStatusRect().contains(event->pos())) {
                showCurrentUserStatusPopup();
                event->accept();
                return;
            }
            if (avatarRect().contains(event->pos())) {
                showCurrentUserProfilePopup();
                event->accept();
                return;
            }
        }

        ApplicationBarItem* item = itemAtPosition(event->pos());
        if (item == moreOptionsItem) {
            showMoreOptionsMenu();
        } else {
            onItemClicked(item);
        }
    }

    QWidget::mousePressEvent(event);
}

void ApplicationBar::leaveEvent(QEvent* event)
{
    setHoveredItem(nullptr);
    QWidget::leaveEvent(event);
}

ApplicationBarItem* ApplicationBar::itemAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    return nullptr;
}

void ApplicationBar::showCurrentUserProfilePopup()
{
    const QString userId = CurrentUser::instance().getUserId();
    if (userId.isEmpty()) {
        return;
    }

    if (!currentUserProfilePopup) {
        currentUserProfilePopup = new FriendProfilePopup(this);
        connect(currentUserProfilePopup,
                &FriendProfilePopup::requestEditProfile,
                this,
                [this]() {
                    QTimer::singleShot(0, this, &ApplicationBar::showCurrentUserEditProfilePopup);
                });
    }

    const QRect avatar = avatarRect();
    const QPoint popupAnchor = mapToGlobal(QPoint(avatar.right()
                                                  + currentUserProfilePopup->width()
                                                  + kAvatarStatusPopupGap,
                                                  avatar.top()));
    currentUserProfilePopup->popupAt(popupAnchor, userId);
}

void ApplicationBar::showCurrentUserEditProfilePopup()
{
    if (currentUserEditProfilePopup) {
        currentUserEditProfilePopup->raise();
        return;
    }

    const CurrentUserProfile profile = CurrentUser::instance().profile();
    if (!profile.isValid()) {
        return;
    }

    auto* content = new CurrentUserProfileEditContent(profile);
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(520, 520);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    currentUserEditProfilePopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!currentUserEditProfilePopup) {
        return;
    }

    content->saveRequested = [this](const CurrentUserProfile& editedProfile) {
        CurrentUser::instance().saveProfile(editedProfile);
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    content->cancelRequested = [this]() {
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    };

    connect(currentUserEditProfilePopup,
            &InWindowPopupOverlay::dismissed,
            this,
            [this]() {
                currentUserEditProfilePopup = nullptr;
            });

    content->setFocus(Qt::PopupFocusReason);
}

void ApplicationBar::showCurrentUserStatusPopup()
{
    if (CurrentUser::instance().getUserId().isEmpty()) {
        return;
    }

    if (currentUserStatusPopup) {
        currentUserStatusPopup->raise();
        return;
    }

    auto* content = new CurrentUserStatusPopupContent(avatarStatusChoiceIndex());
    content->selectionChanged = [this](int index) {
        setAvatarStatusChoiceIndex(index);
    };
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(760, 420);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    currentUserStatusPopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!currentUserStatusPopup) {
        return;
    }

    connect(currentUserStatusPopup, &InWindowPopupOverlay::dismissed, this, [this]() {
        currentUserStatusPopup = nullptr;
    });
}

QString ApplicationBar::avatarStatusIconSource() const
{
    if (privateInvisibleStatus) {
        return QStringLiteral(":/resources/icon/invisible.png");
    }
    return statusIconPath(CurrentUser::instance().getStatus());
}

int ApplicationBar::avatarStatusChoiceIndex() const
{
    if (privateInvisibleStatus) {
        return 3;
    }

    const UserStatus status = CurrentUser::instance().getStatus();
    if (status == Mining) {
        return 1;
    }
    if (status == Flying) {
        return 2;
    }
    return 0;
}

void ApplicationBar::setAvatarStatusChoiceIndex(int index)
{
    if (index < 0 || index >= kStatusIconChoiceCount) {
        return;
    }

    if (index == 3) {
        privateInvisibleStatus = true;
        CurrentUserProfile profile = CurrentUser::instance().profile();
        if (profile.isValid() && profile.status != Offline) {
            profile.status = Offline;
            CurrentUser::instance().saveProfile(profile);
        }
        update();
        return;
    }

    privateInvisibleStatus = false;
    const UserStatus status = index == 1 ? Mining : (index == 2 ? Flying : Online);
    CurrentUserProfile profile = CurrentUser::instance().profile();
    if (profile.isValid() && profile.status != status) {
        profile.status = status;
        CurrentUser::instance().saveProfile(profile);
    }
    update();
}

void ApplicationBar::setHoveredItem(ApplicationBarItem* item)
{
    if (hoveredItem == item) {
        return;
    }

    if (hoveredItem) {
        hoveredItem->setHovered(false);
    }
    hoveredItem = item;
    if (hoveredItem) {
        hoveredItem->setHovered(true);
    }
}

void ApplicationBar::showMoreOptionsMenu()
{
    if (!moreOptionsItem) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(ThemeManager::instance().color(ThemeColor::ContextMenuHover));
    QAction* settingsAction = menu->addAction(QStringLiteral("设置"));
    QAction* themeColorAction = menu->addAction(QStringLiteral("主题颜色"));
    menu->addAction(QStringLiteral("聊天记录管理"));
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出账号"));

    connect(settingsAction, &QAction::triggered, this, &ApplicationBar::settingsRequested);
    connect(themeColorAction, &QAction::triggered, this, &ApplicationBar::appearanceSettingsRequested);

    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        if (moreOptionsItem) {
            moreOptionsItem->setSelected(false);
        }
        menu->deleteLater();
    });

    moreOptionsItem->setSelected(true);
    const QRect itemRect = moreOptionsItem->rect();
    const int horizontalOffset = menu->isUsingNativeMenu() ? 10 : 6;
    const int verticalOffset = menu->isUsingNativeMenu() ? -22 : -46;
    const QPoint popupPos = mapToGlobal(
            QPoint(itemRect.right() + horizontalOffset,
                   itemRect.y() - itemRect.height() + verticalOffset));
    menu->popupWhenMouseReleased(popupPos);
}
