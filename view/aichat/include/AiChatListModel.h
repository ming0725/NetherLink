#ifndef AICHATLISTMODEL_H
#define AICHATLISTMODEL_H

#include "AiChatMessage.h"
#include <QAbstractListModel>
#include <QVector>

// 底部空白项
struct AiBottomSpace {
    static constexpr int DEFAULT_HEIGHT = 200;  // 默认底部空白高度（像素）
};

class AiChatListModel : public QAbstractListModel {
    Q_OBJECT

    public:
        explicit AiChatListModel(QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

        void appendMessage(AiChatMessage* message);

        void appendContentToLastMessage(const QString& content);

        void clearSelection();

        AiChatMessage* messageAt(int row) const;

        bool isBottomSpace(int index) const;

        void ensureBottomSpace();

        void setBottomSpaceHeight(int height);

        // 新增：设置单个选中项
        void setSingleSelection(const QModelIndex& index);

    private:

        struct ListItem {
            AiChatMessage* message = nullptr;
            bool isBottomSpace = false;
            int bottomSpaceHeight = AiBottomSpace::DEFAULT_HEIGHT;
        };

        QVector <ListItem> m_items;
        QModelIndex m_currentSelectedIndex; // 当前选中的项
};

#endif // AICHATLISTMODEL_H