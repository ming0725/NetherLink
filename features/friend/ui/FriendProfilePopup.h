#pragma once

#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include "shared/types/Group.h"
#include "shared/types/User.h"

class FriendSessionController;
class ImageViewer;
class QLabel;
class PaintedLabel;
class QCloseEvent;
class QHideEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QToolButton;
class StatefulPushButton;

class FriendProfilePopup : public QWidget
{
    Q_OBJECT

public:
    explicit FriendProfilePopup(QWidget* parent = nullptr);
    ~FriendProfilePopup() override;

    void setController(FriendSessionController* controller);
    void setGroupContext(const Group& group, bool canEditMemberNickname);
    void clearGroupContext();
    void popupAt(const QPoint& globalPos, const QString& userId);

public slots:
    void setUserId(const QString& userId);
    void clear();

signals:
    void requestMessage(const QString& userId);
    void requestAddFriend(const QString& userId);
    void requestGroupNicknameChange(const QString& userId, const QString& nickname);
    void requestEditProfile();

protected:
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QRect avatarRect() const;
    void setUser(const User& user, bool isCurrentUser);
    void openAvatarViewer();
    void updateAvatar();
    void updateActionButton();
    void updateInfoRows();
    void promptGroupNicknameChange();
    void updatePopupHeight();
    void layoutPopup();
    void updateElidedTexts();
    void releaseProfileState();
    void copyCurrentId();
    QWidget* notificationHost() const;
    void applyTheme();
    User currentUserAsUser() const;
    QPoint constrainedPopupPos(const QPoint& globalPos) const;
    QString currentGroupNickname() const;
    bool hasGroupContextForCurrentUser() const;

    FriendSessionController* m_controller = nullptr;
    QPointer<ImageViewer> m_avatarViewer;
    QString m_avatarImageRequestId;
    QString m_avatarSource;
    User m_user;
    Group m_groupContext;
    bool m_hasUser = false;
    bool m_isCurrentUser = false;
    bool m_canEditGroupNickname = false;

    QWidget* m_contentWidget = nullptr;
    PaintedLabel* m_nameLabel = nullptr;
    PaintedLabel* m_idPrefixLabel = nullptr;
    PaintedLabel* m_idLabel = nullptr;
    QToolButton* m_copyIdButton = nullptr;
    PaintedLabel* m_statusLabel = nullptr;
    PaintedLabel* m_regionTitleLabel = nullptr;
    PaintedLabel* m_remarkTitleLabel = nullptr;
    PaintedLabel* m_groupNicknameTitleLabel = nullptr;
    PaintedLabel* m_signatureTitleLabel = nullptr;
    PaintedLabel* m_regionLabel = nullptr;
    PaintedLabel* m_remarkLabel = nullptr;
    PaintedLabel* m_groupNicknameLabel = nullptr;
    PaintedLabel* m_signatureLabel = nullptr;
    QWidget* m_regionRow = nullptr;
    QWidget* m_remarkRow = nullptr;
    QWidget* m_groupNicknameRow = nullptr;
    QWidget* m_signatureRow = nullptr;
    QLabel* m_statusIcon = nullptr;
    StatefulPushButton* m_actionButton = nullptr;
};
