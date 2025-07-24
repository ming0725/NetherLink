/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "ScrollBarThumb.h"
#include <QAbstractScrollArea>
#include <QTimeLine>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class CustomScrollArea : public QAbstractScrollArea {
    Q_OBJECT

    public:
        explicit CustomScrollArea(QWidget* parent = nullptr);

        ~CustomScrollArea() override;

        QWidget* getContentWidget() {
            return (contentWidget);
        }

        virtual void layoutContent() = 0;

    protected:
        void resizeEvent(QResizeEvent* ev) override;

        bool eventFilter(QObject* obj, QEvent* ev) override;

        void wheelEvent(QWheelEvent* ev) override;

    signals:
        void reachedTop();

        void reachedBottom();

    private:
        void animateTo(int targetY);

        void updateScrollBar();

        ScrollBarThumb* thumb;
        QTimeLine* scrollAnimation;
        int currentOffset = 0;
        int thumbOffset = 0;
        bool dragging = false;
        int dragStartY = 0;
        int thumbOffsetAtStart = 0;

    protected:
        QWidget* contentWidget;
};
