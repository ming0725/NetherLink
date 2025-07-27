/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET
#define INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QTimeLine>

#include "FriendListItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ScrollAreaNoWheel;
class ScrollBarThumb;

class FriendListWidget : public QWidget {
    Q_OBJECT

    public:
        explicit FriendListWidget(QWidget*parent = nullptr);

        ~FriendListWidget();

        void addItem(const User& user);

        void removeItemAt(int index);

    public slots:
        void reloadFriendList();

    protected:
        void resizeEvent(QResizeEvent*event) override;

        void wheelEvent(QWheelEvent*event) override;

        bool eventFilter(QObject*obj, QEvent*event) override;

    private slots:
        void onItemClicked(FriendListItem*);

    private:
        void animateTo(int targetOffset);

        void updateScrollBar();

        void relayoutItems();

        ScrollAreaNoWheel*scrollArea = nullptr;
        ScrollBarThumb*scrollBarThumb = nullptr;
        QWidget*contentWidget = nullptr;
        QList <FriendListItem*> itemList;
        int contentOffset; // 内容区域的偏移
        int thumbOffset; // 滑块的偏移
        bool dragging;
        int dragStartY;
        int thumbOffsetAtDragStart;
        QTimeLine*scrollAnimation = nullptr;
        FriendListItem* selectItem = nullptr;
};

#endif /* INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET */
