
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "AvatarLoader.h"
#include "User.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class FriendListItem : public QWidget {
    Q_OBJECT

    public:
        explicit FriendListItem(const User& user, QWidget* parent = nullptr);

        QSize sizeHint() const override;

        void setSelected(bool select);

        bool isSelected();

        UserStatus getUserStatus() {
            return (status);
        }

        QString getUserName() {
            return (fullNameText);
        }

    protected:
        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

        void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;

        void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;

        void leaveEvent(QEvent*) Q_DECL_OVERRIDE;

        void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;

        void showEvent(QShowEvent*) Q_DECL_OVERRIDE;

    signals:
        void itemClicked(FriendListItem* item);

    private:
        AvatarLabel* avatarLabel;
        QLabel* nameLabel;
        QLabel* statusIconLabel;
        QLabel* statusTextLabel;
        QString fullNameText;
        QString fullStatusAndSignText;
        UserStatus status;
        QString userID;
        bool hovered = false;
        bool selected = false;

        void setupUI(const User& user);

};
