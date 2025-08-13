/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_CUSTOM_SCROLL_AREA

#define INCLUDE_COMPONENTS_CUSTOM_SCROLL_AREA

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QAbstractScrollArea>
#include <QTimeLine>

#include "Components/ScrollBarThumb.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class CustomScrollArea : public QAbstractScrollArea {
    Q_OBJECT

    protected:
        QWidget* contentWidget;

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
};

#endif /* INCLUDE_COMPONENTS_CUSTOM_SCROLL_AREA */
