#ifndef HISTORYUNREADNOTIFIER_H
#define HISTORYUNREADNOTIFIER_H

#include <QWidget>

class HistoryUnreadNotifier : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryUnreadNotifier(QWidget* parent = nullptr);
    void setUnreadCount(int count);
    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;

private:
    QString text() const;
    void updateSize();

    int m_count = 0;
    bool m_hovered = false;
    bool m_pressed = false;
};

#endif // HISTORYUNREADNOTIFIER_H
