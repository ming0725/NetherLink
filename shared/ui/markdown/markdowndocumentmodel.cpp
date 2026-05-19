#include "markdowndocumentmodel.h"

#include <QFutureWatcher>
#include <QStringList>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace {

struct MarkdownParseResult {
    int generation = 0;
    QString markdown;
    QList<MarkdownRenderer::Block> blocks;
};

} // namespace

MarkdownDocumentModel::MarkdownDocumentModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_parseTimer(new QTimer(this))
{
    m_parseTimer->setSingleShot(true);
    m_parseTimer->setInterval(16);
    connect(m_parseTimer, &QTimer::timeout, this, &MarkdownDocumentModel::startPendingParse);
}

int MarkdownDocumentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_blocks.size();
}

QVariant MarkdownDocumentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_blocks.size()) {
        return {};
    }

    const MarkdownRenderer::Block &block = m_blocks.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return block.text;
    case TypeRole:
        return static_cast<int>(block.type);
    case LevelRole:
        return block.level;
    case NumberRole:
        return block.number;
    case BlockRole:
        return QVariant::fromValue(block);
    case SelectionStartRole:
        return index.row() < m_selection.size() ? m_selection.at(index.row()).start : -1;
    case SelectionEndRole:
        return index.row() < m_selection.size() ? m_selection.at(index.row()).end : -1;
    default:
        return {};
    }
}

Qt::ItemFlags MarkdownDocumentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled;
}

QHash<int, QByteArray> MarkdownDocumentModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[TypeRole] = "type";
    roles[TextRole] = "text";
    roles[LevelRole] = "level";
    roles[NumberRole] = "number";
    roles[BlockRole] = "block";
    roles[SelectionStartRole] = "selectionStart";
    roles[SelectionEndRole] = "selectionEnd";
    return roles;
}

void MarkdownDocumentModel::setMarkdown(const QString &markdown)
{
    ++m_parseGeneration;
    m_markdown = markdown;
    m_pendingMarkdown = markdown;

    scheduleParse();
}

void MarkdownDocumentModel::scheduleParse()
{
    if (m_parseRunning) {
        m_parseRequestedWhileRunning = true;
        return;
    }

    m_parseTimer->start();
}

void MarkdownDocumentModel::startPendingParse()
{
    if (m_parseRunning) {
        m_parseRequestedWhileRunning = true;
        return;
    }

    const int generation = m_parseGeneration;
    const QString markdown = m_pendingMarkdown;
    m_parseRunning = true;
    m_parseRequestedWhileRunning = false;

    auto *watcher = new QFutureWatcher<MarkdownParseResult>(this);
    connect(watcher, &QFutureWatcher<MarkdownParseResult>::finished, this, [this, watcher] {
        const MarkdownParseResult result = watcher->result();
        watcher->deleteLater();
        m_parseRunning = false;

        if (result.generation == m_parseGeneration) {
            beginResetModel();
            m_markdown = result.markdown;
            m_blocks = result.blocks;
            m_selection = QVector<RowSelection>(m_blocks.size());
            endResetModel();
        }

        if (m_parseRequestedWhileRunning || result.generation != m_parseGeneration) {
            m_parseRequestedWhileRunning = false;
            scheduleParse();
        }
    });

    watcher->setFuture(QtConcurrent::run([markdown, generation] {
        MarkdownParseResult result;
        result.generation = generation;
        result.markdown = markdown;
        result.blocks = MarkdownRenderer::parseBlocks(markdown);
        return result;
    }));
}

QString MarkdownDocumentModel::markdown() const
{
    return m_markdown;
}

void MarkdownDocumentModel::emitSelectionChanges(const QVector<RowSelection> &oldSelection)
{
    if (m_blocks.isEmpty()) {
        if (!oldSelection.isEmpty()) {
            Q_EMIT layoutChanged();
        }
        return;
    }

    int firstChanged = -1;
    int lastChanged = -1;
    const int rowLimit = qMax(oldSelection.size(), m_selection.size());
    for (int row = 0; row < rowLimit; ++row) {
        const RowSelection oldValue = row < oldSelection.size() ? oldSelection.at(row) : RowSelection();
        const RowSelection newValue = row < m_selection.size() ? m_selection.at(row) : RowSelection();
        if (oldValue.start == newValue.start && oldValue.end == newValue.end) {
            continue;
        }
        if (firstChanged < 0) {
            firstChanged = row;
        }
        lastChanged = row;
    }

    if (firstChanged >= 0) {
        firstChanged = qBound(0, firstChanged, m_blocks.size() - 1);
        lastChanged = qBound(0, lastChanged, m_blocks.size() - 1);
        Q_EMIT dataChanged(index(firstChanged, 0),
                           index(lastChanged, 0),
                           {SelectionStartRole, SelectionEndRole});
    }
}

void MarkdownDocumentModel::setSelectionRange(int anchorRow,
                                              int anchorCursor,
                                              int activeRow,
                                              int activeCursor)
{
    if (m_blocks.isEmpty()) {
        return;
    }

    anchorRow = qBound(0, anchorRow, m_blocks.size() - 1);
    activeRow = qBound(0, activeRow, m_blocks.size() - 1);
    anchorCursor = qBound(0, anchorCursor, m_blocks.at(anchorRow).text.size());
    activeCursor = qBound(0, activeCursor, m_blocks.at(activeRow).text.size());

    const QVector<RowSelection> oldSelection = m_selection;
    m_selection = QVector<RowSelection>(m_blocks.size());

    int startRow = anchorRow;
    int endRow = activeRow;
    int startCursor = anchorCursor;
    int endCursor = activeCursor;
    if (startRow > endRow || (startRow == endRow && startCursor > endCursor)) {
        std::swap(startRow, endRow);
        std::swap(startCursor, endCursor);
    }

    for (int row = startRow; row <= endRow; ++row) {
        const int rowLength = m_blocks.at(row).text.size();
        RowSelection selection;
        if (startRow == endRow) {
            selection.start = startCursor;
            selection.end = endCursor;
        } else if (row == startRow) {
            selection.start = startCursor;
            selection.end = rowLength;
        } else if (row == endRow) {
            selection.start = 0;
            selection.end = endCursor;
        } else {
            selection.start = 0;
            selection.end = rowLength;
        }

        if (selection.end > selection.start) {
            m_selection[row] = selection;
        }
    }

    emitSelectionChanges(oldSelection);
}

void MarkdownDocumentModel::clearSelection()
{
    if (m_selection.isEmpty()) {
        return;
    }

    bool hadSelection = false;
    for (const RowSelection &selection : std::as_const(m_selection)) {
        if (selection.end > selection.start) {
            hadSelection = true;
            break;
        }
    }

    if (hadSelection) {
        const QVector<RowSelection> oldSelection = m_selection;
        m_selection = QVector<RowSelection>(m_blocks.size());
        emitSelectionChanges(oldSelection);
    }
}

void MarkdownDocumentModel::selectAll()
{
    const QVector<RowSelection> oldSelection = m_selection;
    m_selection = QVector<RowSelection>(m_blocks.size());
    for (int row = 0; row < m_blocks.size(); ++row) {
        if (m_blocks.at(row).text.isEmpty()) {
            continue;
        }
        m_selection[row] = {0, static_cast<int>(m_blocks.at(row).text.size())};
    }

    emitSelectionChanges(oldSelection);
}

bool MarkdownDocumentModel::hasSelection() const
{
    for (const RowSelection &selection : m_selection) {
        if (selection.end > selection.start) {
            return true;
        }
    }
    return false;
}

QString MarkdownDocumentModel::selectedText() const
{
    QStringList lines;
    for (int row = 0; row < m_blocks.size() && row < m_selection.size(); ++row) {
        const RowSelection &selection = m_selection.at(row);
        if (selection.end > selection.start) {
            lines.append(m_blocks.at(row).text.mid(selection.start, selection.end - selection.start));
        }
    }
    return lines.join(QLatin1Char('\n'));
}
