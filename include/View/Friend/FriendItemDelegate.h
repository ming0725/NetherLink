/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_FRIEND_ITEM_DELEGATE
#define INCLUDE_VIEW_FRIEND_FRIEND_ITEM_DELEGATE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QStyledItemDelegate>

#include "Entity/User.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class FriendItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

    public:
        explicit FriendItemDelegate(QObject* parent = nullptr);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    signals:
        void itemClicked(int index);

    private:
        static constexpr int AVATAR_SIZE = 48;
        static constexpr int LEFT_PAD = 12;
        static constexpr int SPACING = 6;
        static constexpr int BETWEEN = 6;
        static constexpr int ICON_SIZE = 12;
        static constexpr int ICO_TEXT_DIST = 4;
        static constexpr int STATUS_ICON_SIZE = 13;

        void drawAvatar(QPainter* painter, const QRect& rect, const QString& userID, const QString& avatarPath, UserStatus status) const;

        void drawName(QPainter* painter, const QRect& rect, const QString& name, bool isSelected) const;

        void drawStatus(QPainter* painter, const QRect& rect, UserStatus status, const QString& signature, bool isSelected) const;

        void drawStatusIcon(QPainter* painter, const QRect& rect, UserStatus status) const;

        QString statusText(UserStatus status) const;

        QString statusIconPath(UserStatus status) const;

        QRect calculateAvatarRect(const QRect& contentRect) const;

        QRect calculateNameRect(const QRect& contentRect) const;

        QRect calculateStatusRect(const QRect& contentRect) const;

        QRect calculateStatusIconRect(const QRect& contentRect) const;

};

#endif /* INCLUDE_VIEW_FRIEND_FRIEND_ITEM_DELEGATE */ 