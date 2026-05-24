#include "app/frame/current_user/AvatarCropPopupContent.h"

#include "app/frame/current_user/AvatarCropCanvas.h"
#include "app/frame/current_user/CurrentUserPopupStyle.h"
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

using namespace CurrentUserPopupStyle;

AvatarCropPopupContent::AvatarCropPopupContent(const QImage& image, QWidget* parent)
    : QWidget(parent)
    , m_canvas(new AvatarCropCanvas(image, this))
    , m_zoomSlider(new QSlider(Qt::Horizontal, this))
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

    m_zoomSlider->setRange(0, 1000);
    m_zoomSlider->setValue(m_canvas->zoomValue());
    layout->addWidget(m_zoomSlider);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    m_cancelButton->setFixedSize(92, 34);
    m_confirmButton->setFixedSize(92, 34);
    applyDefaultButtonStyle(m_cancelButton);
    m_confirmButton->setPrimaryStyle();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    layout->addLayout(buttonLayout);

    connect(m_zoomSlider, &QSlider::valueChanged, m_canvas, &AvatarCropCanvas::setZoomValue);
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
        update();
    });
}
