#ifndef AICHATLISTVIEW_H
#define AICHATLISTVIEW_H

#include "SmoothScrollBar.h"
#include <QListView>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QScrollBar>

class AiChatListView : public QListView {
    Q_OBJECT Q_PROPERTY(int smoothScrollValue READ smoothScrollValue WRITE setSmoothScrollValue)

    public:
        explicit AiChatListView(QWidget*parent = nullptr);

        void setModel(QAbstractItemModel*model) override;

        void scrollToBottom();

    protected:
        void mousePressEvent(QMouseEvent* event) override;

        void resizeEvent(QResizeEvent*event) override;

        void wheelEvent(QWheelEvent*event) override;

        void enterEvent(QEnterEvent*event) override;

        void leaveEvent(QEvent*event) override;

    private slots:
        void onCustomScrollValueChanged(int value);

        void onAnimationFinished();

        void onModelRowsChanged();

        void checkScrollBarVisibility();

    private:
        SmoothScrollBar*customScrollBar;
        QPropertyAnimation*scrollAnimation;
        int m_smoothScrollValue;
        bool hovered = false;

        int smoothScrollValue() const {
            return (m_smoothScrollValue);
        }

        void setSmoothScrollValue(int value);

        void updateCustomScrollBar();

        void startScrollAnimation(int targetValue);

};

#endif // AICHATLISTVIEW_H