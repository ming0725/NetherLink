#ifndef MARKDOWNDELEGATE_H
#define MARKDOWNDELEGATE_H

#include <QStyledItemDelegate>

class MarkdownDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MarkdownDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    int cursorForPosition(const QStyleOptionViewItem &option,
                          const QModelIndex &index,
                          const QPoint &position) const;
    bool hasTextAtPosition(const QStyleOptionViewItem &option,
                           const QModelIndex &index,
                           const QPoint &position) const;
    QString linkAtPosition(const QStyleOptionViewItem &option,
                           const QModelIndex &index,
                           const QPoint &position) const;
    bool isCodeCopyButtonAtPosition(const QStyleOptionViewItem &option,
                                    const QModelIndex &index,
                                    const QPoint &position) const;
    bool isSettingActionButtonAtPosition(const QStyleOptionViewItem &option,
                                         const QModelIndex &index,
                                         const QPoint &position) const;
};

#endif // MARKDOWNDELEGATE_H
