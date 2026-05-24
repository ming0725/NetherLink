#pragma once

#include <QColor>
#include <QFutureWatcher>
#include <QImage>
#include <QMargins>
#include <QPointer>
#include <QSize>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QFrame;
class QGraphicsOpacityEffect;
class QShowEvent;

class InWindowPopupOverlay : public QWidget
{
    Q_OBJECT

public:
    enum class DismissReason {
        Accepted,
        Rejected,
        OutsideClick,
        Escape,
        Programmatic
    };
    Q_ENUM(DismissReason)

    struct Options {
        Options();

        QMargins popupMargins;
        QSize maximumPopupSize;
        qreal blurRadius;
        qreal blurRenderScale;
        QColor overlayTint;
        bool dismissOnOutsideClick;
        bool dismissOnEscape;
        bool deleteContentOnClose;
    };

    explicit InWindowPopupOverlay(QWidget* host, QWidget* content, const Options& options = Options());
    ~InWindowPopupOverlay() override;

    static InWindowPopupOverlay* showPopup(QWidget* parent,
                                           QWidget* content,
                                           const Options& options = Options());

    QWidget* contentWidget() const;
    DismissReason dismissReason() const;

public slots:
    void closePopup(DismissReason reason = DismissReason::Programmatic);

signals:
    void dismissed(InWindowPopupOverlay::DismissReason reason);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void captureBlurredSnapshot();
    void discardBlurredSnapshot();
    void startOpenAnimation();
    void setOpenProgress(qreal progress);
    void syncToHostGeometry();
    void updatePopupGeometry();
    void suppressSystemFloatingBars();
    void updateSuppressedSystemFloatingBars(qreal openProgress);
    void restoreSystemFloatingBars();
    QSize boundedPopupSize() const;
    bool isInsidePopup(const QPoint& position) const;
    bool isOverlayChild(QObject* object) const;

    struct SuppressedWidgetState {
        QPointer<QWidget> widget;
        QVariant previousSuppressedProperty;
        QVariant previousSuppressedOpacityProperty;
        bool wasVisible = false;
        bool hadSuppressedProperty = false;
        bool hadSuppressedOpacityProperty = false;
    };

    Options m_options;
    QPointer<QWidget> m_host;
    QPointer<QWidget> m_previousFocus;
    QPointer<QWidget> m_content;
    QWidget* m_popupContainer = nullptr;
    QFrame* m_popupFrame = nullptr;
    QGraphicsOpacityEffect* m_popupOpacityEffect = nullptr;
    QFutureWatcher<QImage>* m_blurWatcher = nullptr;
    QImage m_snapshot;
    QImage m_blurredSnapshot;
    QVector<SuppressedWidgetState> m_suppressedWidgets;
    qreal m_blurProgress = 1.0;
    qreal m_popupProgress = 1.0;
    DismissReason m_dismissReason = DismissReason::Programmatic;
    bool m_closing = false;
    bool m_blurInFlight = false;
    bool m_openAnimationStarted = false;
    int m_snapshotGeneration = 0;
};
