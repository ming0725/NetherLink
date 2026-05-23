#pragma once

#include <QImage>
#include <QWidget>

class PostOverlay : public QWidget
{
public:
    explicit PostOverlay(QWidget* parent = nullptr);

    void captureBlurredBackground(QWidget* sourceWidget = nullptr);
    void clearBlurredBackground();
    void setOverlayOpacity(qreal opacity);
    qreal overlayOpacity() const { return m_opacity; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_snapshot;
    QImage m_blurredSnapshot;
    int m_blurGeneration = 0;
    qreal m_opacity = 0.0;
};
