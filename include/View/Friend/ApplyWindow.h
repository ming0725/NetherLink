/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_FRIEND_APPLY_WINDOW

#define INCLUDE_VIEW_FRIEND_APPLY_WINDOW

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPushButton>
#include <QTextEdit>

#include "Data/AvatarLoader.h"
#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class ApplyWindow : public QWidget {
    Q_OBJECT

    public:
        enum Type {
            User,
            Group
        };

        explicit ApplyWindow(Type type, const QString& id, const QString& name, const QString& avatarUrl, QWidget* parent = nullptr);

        ~ApplyWindow() override;

    protected:
        void resizeEvent(QResizeEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

    // bool eventFilter(QObject* watched, QEvent* ev) override;

    private slots:
        void onCancelClicked();

        void onSendClicked();

    private:
        void initUI();

        void setupConnections();

        Type type;
        QString targetId;
        QString targetName;
        QString targetAvatar;
        QLabel* titleLabel;
        AvatarLabel* avatarLabel;
        QLabel* nameLabel;
        QTextEdit* messageEdit;
        QPushButton* btnClose;
        QPushButton* btnCancel;
        QPushButton* btnSend;
        QIcon iconClose;
        QIcon iconCloseHover;

        // 窗口尺寸相关常量
        const int WINDOW_MIN_WIDTH = 360;
        const int WINDOW_MIN_HEIGHT = 280;
        const int TITLE_HEIGHT = 40;
        const int BTN_SIZE = 32;
        const int AVATAR_SIZE = 60;
        const int MARGIN = 20;
        const int BTN_HEIGHT = 32;
        const int BTN_WIDTH = 80;
        const int MESSAGE_HEIGHT = 80;
};

#endif /* INCLUDE_VIEW_FRIEND_APPLY_WINDOW */
