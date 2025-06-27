#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "AvatarLoader.h"
#include "User.h"

class SearchResultItem : public QWidget {
    Q_OBJECT
public:
    enum ItemType {
        UserType,
        GroupType
    };

    SearchResultItem(const User& user, QString& uid, QWidget* parent = nullptr);  // 用户构造函数
    SearchResultItem(const QString& groupId, const QString& groupName, int memberCount, QString& avatarUrl, QWidget* parent = nullptr);  // 群组构造函数
    QSize sizeHint() const override;

protected:
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private slots:
    void onActionButtonClicked();

private:
    void setupUI();

    AvatarLabel* avatarLabel;
    QLabel* nameLabel;
    QLabel* statusIconLabel;  // 用户状态图标或群组成员图标
    QLabel* statusTextLabel;  // 用户状态文本或群组成员数
    QPushButton* actionButton;

    QString fullNameText;
    QString fullStatusAndSignText;
    ItemType type;
    UserStatus status;
    bool hovered = false;

    // 存储用户/群组信息
    QString id;
    QString name;
    QString avatarUrl;

    // 布局常量
    const int AVATAR_SIZE = 48;
    const int LEFT_PADDING = 16;
    const int SPACING = 12;
    const int BETWEEN = 4;
    const int STATUS_ICON_SIZE = 13;
    const int BUTTON_WIDTH = 60;
    const int BUTTON_HEIGHT = 28;
}; 