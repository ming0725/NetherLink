#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QString>
#include <QVector>

class MarkdownRenderer
{
public:
    enum InlineStyle {
        NoInlineStyle = 0,
        Bold = 1 << 0,
        Italic = 1 << 1,
        Code = 1 << 2,
        Strike = 1 << 3,
        Link = 1 << 4,
        Math = 1 << 5
    };

    struct InlineSpan {
        int start = 0;
        int length = 0;
        int style = NoInlineStyle;
        QString href;
    };

    enum class BlockType {
        Paragraph,
        Heading,
        BlockQuote,
        UnorderedList,
        OrderedList,
        HorizontalRule,
        CodeBlock,
        MathBlock,
        Table,
        Blank
    };

    enum class TableAlignment {
        None,
        Left,
        Center,
        Right
    };

    struct TableCell {
        QString text;
        QVector<InlineSpan> spans;
    };

    struct TableRow {
        QVector<TableCell> cells;
        bool header = false;
    };

    struct Block {
        BlockType type = BlockType::Paragraph;
        QString text;
        QString language;
        QVector<InlineSpan> spans;
        QVector<TableRow> tableRows;
        QVector<TableAlignment> tableAlignments;
        int level = 0;
        int number = 0;
    };

    static QList<Block> parseBlocks(const QString &markdown);
    static Block parseInlineText(Block block, const QString &markdownText);
};

#endif // MARKDOWNRENDERER_H
