/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_FRIEND_LIST_MODEL
#define INCLUDE_VIEW_FRIEND_FRIEND_LIST_MODEL

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QAbstractListModel>

#include "Entity/User.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class FriendListModel : public QAbstractListModel {
    Q_OBJECT

    public:
        explicit FriendListModel(QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

        Qt::ItemFlags flags(const QModelIndex& index) const override;

        void addUser(const User& user);

        void removeUser(int index);

        void clear();

        void reloadUsers();

        const User* userAt(int index) const;

        void setSelectedIndex(int index);

        int getSelectedIndex() const;

        void clearSelection();

    private:
        QVector<User> users;
        int selectedIndex = -1;

        void sortUsers();
};

#endif /* INCLUDE_VIEW_FRIEND_FRIEND_LIST_MODEL */ 