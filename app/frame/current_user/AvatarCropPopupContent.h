#pragma once

#include <QImage>
#include <QWidget>

#include <functional>

class AvatarCropCanvas;
class AvatarCropZoomSlider;
class StatefulPushButton;

class AvatarCropPopupContent final : public QWidget
{
public:
    explicit AvatarCropPopupContent(const QImage& image, QWidget* parent = nullptr);

    std::function<void(const QImage&)> accepted;
    std::function<void()> cancelRequested;

private:
    AvatarCropCanvas* m_canvas = nullptr;
    AvatarCropZoomSlider* m_zoomSlider = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_confirmButton = nullptr;
    bool m_syncingZoom = false;
};
