#pragma once

#include <QWidget>
#include <QLabel>
#include "NotificationBadge.h"

class NotificationItem : public QWidget {
    Q_OBJECT
public:
    explicit NotificationItem(const QString& text, QWidget* parent = nullptr);
    QSize sizeHint() const override;
    void setUnreadCount(int count);
    int getUnreadCount() const;
    void clearUnreadCount();
    void setSelected(bool selected);
    bool isSelected() const { return selected; }

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* textLabel;
    QLabel* arrowLabel;
    NotificationBadge* badge;
    bool hovered = false;
    bool pressed = false;
    bool selected = false;

signals:
    void clicked();
}; 