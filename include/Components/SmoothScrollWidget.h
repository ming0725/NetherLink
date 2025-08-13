/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_SMOOTH_SCROLL_WIDGET

#define INCLUDE_COMPONENTS_SMOOTH_SCROLL_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QTimeLine>
#include <QWidget>

#include "Components/ScrollAreaNoWheel.h"
#include "Components/ScrollBarThumb.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ScrollAreaNoWheel;
class ScrollBarThumb;

class SmoothScrollWidget : public QWidget {
    Q_OBJECT

    protected:
        void wheelEvent(QWheelEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

        bool eventFilter(QObject* obj, QEvent* event) override;

        virtual void animateTo(int targetOffset);

        virtual void updateScrollBar();

        virtual void relayoutContent();

        virtual void setContentMinimumHeight(int height); // 提供给子类设置内容高度（用于 setGeometry 布局）

        // 子类通过该 widget 添加内容即可，基类会处理滚动逻辑
        ScrollAreaNoWheel* scrollArea;
        ScrollBarThumb* scrollBarThumb;
        QWidget* contentWidget;
        QTimeLine* scrollAnimation;
        int contentOffset;
        int thumbOffset;
        bool dragging;
        int dragStartY;
        int thumbOffsetAtDragStart;

    public:
        explicit SmoothScrollWidget(QWidget* parent = nullptr);

        virtual ~SmoothScrollWidget();
};

#endif /* INCLUDE_COMPONENTS_SMOOTH_SCROLL_WIDGET */
