#pragma once

#include <QVariantAnimation>
#include <QWidget>

class QtFallbackLiquidGlassController;

class PostFloatingActionButton : public QWidget {
    Q_OBJECT
public:
    explicit PostFloatingActionButton(QWidget* parent = nullptr);
    ~PostFloatingActionButton() override;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void setVisualOpacity(qreal opacity);
    void refreshPlatformAppearance();
    void setLiquidGlassSourceWidget(QWidget* widget);
    void scheduleLiquidGlassUpdate(int delayMs = 0);
    void scheduleLiquidGlassInteractiveUpdate();
    qreal visualOpacity() const { return m_visualOpacity; }
    bool usesNativeButton() const { return m_usesNativeButton; }
    bool usesQtFallbackLiquidGlass() const;

signals:
    void clicked();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onNativeActionTriggered();

private:
    void syncPlatformButton();
    void updatePanelShadow();
    bool shouldUseQtFallbackLiquidGlass() const;
    void updateQtFallbackLiquidGlassState();
    void releaseQtFallbackLiquidGlassResources(bool updateWidget = true);

    QtFallbackLiquidGlassController* m_liquidGlass = nullptr;
    qreal m_visualOpacity = 1.0;
    bool m_usesNativeButton = false;
    bool m_hovered = false;
};
