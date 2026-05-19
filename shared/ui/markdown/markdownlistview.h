#ifndef MARKDOWNLISTVIEW_H
#define MARKDOWNLISTVIEW_H

#include <QListView>
#include <QPersistentModelIndex>
#include <QStyleOptionViewItem>

class MarkdownDocumentModel;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QTimer;
class QWheelEvent;

class MarkdownListView : public QListView
{
    Q_OBJECT

public:
    explicit MarkdownListView(QWidget *parent = nullptr);

    void setMarkdown(const QString &markdown);
    MarkdownDocumentModel *markdownModel() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool handleZoomWheelEvent(QWheelEvent *event);
    void setMarkdownFontPixelSize(int pixelSize);
    int cursorForViewportPosition(const QPoint &position, QModelIndex *hitIndex = nullptr) const;
    bool hasTextAtViewportPosition(const QPoint &position, QModelIndex *hitIndex = nullptr) const;
    QString linkAtViewportPosition(const QPoint &position, QModelIndex *hitIndex = nullptr) const;
    void updateCursorForViewportPosition(const QPoint &position);
    QStyleOptionViewItem optionForIndex(const QModelIndex &index) const;
    void setCopiedCodeIndex(const QModelIndex &index);
    void clearCopiedCodeIndex();

    MarkdownDocumentModel *m_model = nullptr;
    QTimer *m_copyResetTimer = nullptr;
    QPersistentModelIndex m_copiedCodeIndex;
    int m_anchorRow = -1;
    int m_anchorCursor = 0;
    int m_zoomWheelDelta = 0;
    bool m_selecting = false;
};

#endif // MARKDOWNLISTVIEW_H
