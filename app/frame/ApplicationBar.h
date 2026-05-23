#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>

class ApplicationBarItem;
class FriendProfilePopup;
class InWindowPopupOverlay;
class QEvent;
class QMouseEvent;
class ApplicationBar : public QWidget {
    Q_OBJECT
public:
    ApplicationBar(QWidget* parent = nullptr);
    void addItem(ApplicationBarItem*);
    void addBottomItem(ApplicationBarItem*);
    void setAvatarSource(const QString& source);
    void setTopInset(int inset);
    void setCurrentTopIndex(int index);
    int topItemsCount() const { return topItems.size(); }
    int indexOfTopItem(ApplicationBarItem* item) const {
        return topItems.indexOf(item);
    }
protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent*) Q_DECL_OVERRIDE;
private slots:
    void onItemClicked(ApplicationBarItem*);
signals:
    void applicationClicked(ApplicationBarItem*);
    void settingsRequested();
    void appearanceSettingsRequested();
private:
    void layoutItems();
    void refreshChatBadge();
    void refreshFriendBadge();
    QRect avatarRect() const;
    QRect avatarStatusRect() const;
    QRect avatarCutoutRect() const;
    QRect avatarStatusIconRect() const;
    ApplicationBarItem* itemAtPosition(const QPoint& pos) const;
    void setHoveredItem(ApplicationBarItem* item);
    void showCurrentUserProfilePopup();
    void showCurrentUserEditProfilePopup();
    void showCurrentUserStatusPopup();
    QString avatarStatusIconSource() const;
    int avatarStatusChoiceIndex() const;
    void setAvatarStatusChoiceIndex(int index);
    void showMoreOptionsMenu();

    ApplicationBarItem* selectedItem = nullptr;
    ApplicationBarItem* hoveredItem = nullptr;
    ApplicationBarItem* messageItem = nullptr;
    ApplicationBarItem* friendItem = nullptr;
    ApplicationBarItem* moreOptionsItem = nullptr;
    QPointer<FriendProfilePopup> currentUserProfilePopup;
    QPointer<InWindowPopupOverlay> currentUserStatusPopup;
    QPointer<InWindowPopupOverlay> currentUserEditProfilePopup;
    QString avatarSource;
    bool privateInvisibleStatus = false;
    QVector<ApplicationBarItem*> topItems;
    QVector<ApplicationBarItem*> bottomItems;
    const int marginTop     = 20;
    const int marginBottom  = 10;
    const int iconSize      = 40;
    const int spacing       = 5;
    int avatarAndItemDist   = 22;
    int avatarSize          = 35;
    int highlightPosY       = 0;
    int topInset            = 0;
    QVariantAnimation* highlightAnim = nullptr;
};
