#pragma once

#include <QStyledItemDelegate>

class FriendNotificationDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FriendNotificationDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    int buttonAt(const QStyleOptionViewItem& option,
                 const QModelIndex& index,
                 const QPoint& point) const;

    static constexpr int kItemHeight = 104;
};
