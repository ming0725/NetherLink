#pragma once

#include <QAbstractButton>
#include <QPointF>

class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;

class WindowsWindowControlButton final : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Kind {
        Minimize,
        Maximize,
        Close
    };

    explicit WindowsWindowControlButton(Kind kind, QWidget* parent = nullptr);
    void setNativeState(bool hovered, bool pressed);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void paintMinimizeIcon(QPainter& painter, const QPointF& center) const;
    void paintMaximizeIcon(QPainter& painter, const QPointF& center) const;
    void paintRestoreIcon(QPainter& painter, const QPointF& center) const;
    void paintCloseIcon(QPainter& painter, const QPointF& center) const;

    Kind m_kind;
    bool m_hovered = false;
};
