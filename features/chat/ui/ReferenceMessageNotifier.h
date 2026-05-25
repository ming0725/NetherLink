#ifndef REFERENCEMESSAGENOTIFIER_H
#define REFERENCEMESSAGENOTIFIER_H

#include <QWidget>

#include "shared/types/ChatMessage.h"

class ReferenceMessageNotifier : public QWidget
{
    Q_OBJECT

public:
    explicit ReferenceMessageNotifier(QWidget* parent = nullptr);

    void setMessage(const ChatMessage* message);
    QSize sizeHint() const override;

signals:
    void closeRequested();

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
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QRect closeRect() const;
    QString displayTextForMessage(const ChatMessage* message) const;

    QString m_displayText;
    bool m_closeHovered = false;
    bool m_closePressed = false;
};

#endif // REFERENCEMESSAGENOTIFIER_H
