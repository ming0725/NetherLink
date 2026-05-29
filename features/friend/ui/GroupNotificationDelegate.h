#pragma once

#include <QStyledItemDelegate>

class FriendSessionController;

class GroupNotificationDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GroupNotificationDelegate(QObject* parent = nullptr);
    void setController(FriendSessionController* controller);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    int buttonAt(const QStyleOptionViewItem& option,
                 const QModelIndex& index,
                 const QPoint& point) const;

    static constexpr int kItemHeight = 104;

private:
    FriendSessionController* m_controller = nullptr;
};
