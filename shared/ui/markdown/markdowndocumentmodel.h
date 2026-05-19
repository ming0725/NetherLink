#ifndef MARKDOWNDOCUMENTMODEL_H
#define MARKDOWNDOCUMENTMODEL_H

#include "markdownrenderer.h"

#include <QAbstractListModel>

class QTimer;

class MarkdownDocumentModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        TextRole,
        LevelRole,
        NumberRole,
        BlockRole,
        SelectionStartRole,
        SelectionEndRole
    };

    explicit MarkdownDocumentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMarkdown(const QString &markdown);
    QString markdown() const;
    void setSelectionRange(int anchorRow, int anchorCursor, int activeRow, int activeCursor);
    void clearSelection();
    void selectAll();
    bool hasSelection() const;
    QString selectedText() const;

private:
    struct RowSelection {
        int start = -1;
        int end = -1;
    };

    void emitSelectionChanges(const QVector<RowSelection> &oldSelection);
    void scheduleParse();
    void startPendingParse();

    QString m_markdown;
    QString m_pendingMarkdown;
    QList<MarkdownRenderer::Block> m_blocks;
    QVector<RowSelection> m_selection;
    QTimer *m_parseTimer = nullptr;
    int m_parseGeneration = 0;
    bool m_parseRunning = false;
    bool m_parseRequestedWhileRunning = false;
};

Q_DECLARE_METATYPE(MarkdownRenderer::Block)

#endif // MARKDOWNDOCUMENTMODEL_H
