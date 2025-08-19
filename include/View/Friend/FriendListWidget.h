/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET
#define INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class FriendListModel;
class FriendItemDelegate;
class FriendListView;
class User;

class FriendListWidget : public QWidget {
    Q_OBJECT

    public:
        explicit FriendListWidget(QWidget* parent = nullptr);

        ~FriendListWidget();

        void addItem(const User& user);

        void removeItemAt(int index);

    public slots:
        void reloadFriendList();

    private slots:
        void onItemClicked(int index);

    private:
        FriendListModel* model = nullptr;
        FriendItemDelegate* delegate = nullptr;
        FriendListView* listView = nullptr;
};

#endif /* INCLUDE_VIEW_FRIEND_FRIEND_LIST_WIDGET */
