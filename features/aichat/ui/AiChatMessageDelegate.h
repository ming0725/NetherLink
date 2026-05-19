#pragma once

#include <QCache>
#include <QHash>
#include <QPair>
#include <QPersistentModelIndex>
#include <QString>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QVector>

#include "shared/ui/markdown/markdowndelegate.h"
#include "shared/ui/markdown/markdownrenderer.h"
#include "shared/ui/SelectableText.h"

class QColor;

class AiChatMessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum class MessageAction {
        None,
        Copy,
        Refresh,
        Like,
        Dislike,
        ToggleExpansion
    };

    enum class MessageFeedback {
        None,
        Liked,
        Disliked
    };

    explicit AiChatMessageDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    bool bubbleHitTest(const QStyleOptionViewItem& option,
                       const QModelIndex& index,
                       const QPoint& viewportPos) const;
    int characterIndexAt(const QStyleOptionViewItem& option,
                         const QModelIndex& index,
                         const QPoint& viewportPos,
                         bool allowLineWhitespace = false) const;
    QString urlAt(const QStyleOptionViewItem& option,
                  const QModelIndex& index,
                  const QPoint& viewportPos) const;
    bool isCodeCopyButtonAt(const QStyleOptionViewItem& option,
                            const QModelIndex& index,
                            const QPoint& viewportPos) const;
    int codeCopyBlockRowAt(const QStyleOptionViewItem& option,
                           const QModelIndex& index,
                           const QPoint& viewportPos) const;
    QString codeBlockTextAt(const QStyleOptionViewItem& option,
                            const QModelIndex& index,
                            const QPoint& viewportPos) const;
    MessageAction messageActionAt(const QStyleOptionViewItem& option,
                                  const QModelIndex& index,
                                  const QPoint& viewportPos) const;

    bool selectWordAt(const QStyleOptionViewItem& option,
                      const QModelIndex& index,
                      const QPoint& viewportPos);
    void setMessageFeedback(const QString& messageId, MessageFeedback feedback);
    MessageFeedback messageFeedback(const QString& messageId) const;
    void setStreamingMessageId(const QString& messageId);
    QString streamingMessageId() const;
    void setCopiedCodeBlock(const QModelIndex& index, int blockRow);
    void clearCopiedCodeBlock();
    void setUserMessageExpanded(const QString& messageId, bool expanded);
    bool isUserMessageExpanded(const QString& messageId) const;
    void setUserMessageExpansionProgress(const QString& messageId, qreal progress);
    qreal userMessageExpansionProgress(const QString& messageId) const;
    void notifySizeHintChanged(const QModelIndex& index);
    void setUserCopyButtonOpacity(const QString& messageId, qreal opacity);
    qreal userCopyButtonOpacity(const QString& messageId) const;
    void setSelection(const QModelIndex& index, int anchor, int cursor);
    void clearSelection();
    void setBubbleSelection(const QModelIndex& index);
    void clearBubbleSelection();
    bool hasSelection() const;
    bool hasBubbleSelection() const;
    bool selectionContains(const QModelIndex& index, int cursor) const;
    QString selectedText() const;
    QString renderedText(const QModelIndex& index) const;
    QPersistentModelIndex selectionIndex() const;

private:
    struct LayoutMetrics {
        QRect bubbleRect;
        QRect textRect;
        QSize textSize;
        QRect expandRect;
        QRect copyButtonRect;
        bool userCanExpand = false;
    };

    struct TextRange {
        int start = -1;
        int length = 0;
        QString text;
    };

    struct TextDocumentCacheEntry {
        QTextDocument document;
    };

    struct TextSizeCacheEntry {
        QSize fullSize;
        int collapsedHeight = 1;
        bool canExpand = false;
    };

    struct UrlRangesCacheEntry {
        QVector<TextRange> ranges;
    };

    struct MarkdownCacheEntry {
        QList<MarkdownRenderer::Block> blocks;
        QVector<int> blockStartOffsets;
        QString plainText;
        QString sourceText;
    };

    struct MarkdownLayoutCacheEntry {
        QVector<int> blockHeights;
        QVector<int> blockOffsets;
        QSize size;
    };

    struct MarkdownBlockHit {
        bool valid = false;
        int row = -1;
        MarkdownRenderer::Block block;
        QStyleOptionViewItem option;
    };

    LayoutMetrics layoutMetrics(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const;
    void configureTextDocument(QTextDocument& document,
                               const QString& text,
                               const QFont& font,
                               int textWidth,
                               const QColor& textColor,
                               bool isFromUser,
                               bool dark) const;
    QSize textDocumentSize(const QString& text,
                           const QFont& font,
                           int maxTextWidth,
                           bool isFromUser) const;
    const TextSizeCacheEntry& cachedTextSize(const QString& text,
                                             const QFont& font,
                                             int maxTextWidth,
                                             bool isFromUser) const;
    const QTextDocument& cachedTextDocument(const QString& text,
                                            const QFont& font,
                                            int textWidth,
                                            const QColor& textColor,
                                            bool isFromUser,
                                            bool dark) const;
    QVector<TextRange> cachedUrlRanges(const QString& text) const;
    QVector<TextRange> urlRanges(const QString& text) const;
    const MarkdownCacheEntry& cachedMarkdown(const QString& text) const;
    const MarkdownLayoutCacheEntry& cachedMarkdownLayout(const MarkdownCacheEntry& entry,
                                                         const QStyleOptionViewItem& option,
                                                         const QFont& font,
                                                         int maxTextWidth) const;
    QSize markdownDocumentSize(const MarkdownCacheEntry& entry,
                               const QStyleOptionViewItem& option,
                               const QFont& font,
                               int maxTextWidth) const;
    MarkdownBlockHit markdownBlockAt(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const;
    void paintMarkdownMessage(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index,
                              const LayoutMetrics& metrics,
                              const MarkdownCacheEntry& entry,
                              const QColor& textColor) const;
    QString renderedPlainText(const QString& text, bool isFromUser) const;
    QFont messageFont() const;
    int maxBubbleWidth(int itemWidth) const;
    bool isAiReplyActionVisible(const QModelIndex& index, MessageAction action) const;
    QVector<QPair<MessageAction, QRect>> aiReplyActionRects(const LayoutMetrics& metrics,
                                                            const QModelIndex& index) const;
    void paintAiReplyActions(QPainter* painter,
                             const LayoutMetrics& metrics,
                             const QModelIndex& index) const;
    void paintUserMessageChrome(QPainter* painter,
                                const LayoutMetrics& metrics,
                                const QModelIndex& index,
                                const QColor& textColor) const;

    QPersistentModelIndex m_selectionIndex;
    QPersistentModelIndex m_bubbleSelectionIndex;
    QPersistentModelIndex m_copiedCodeMessageIndex;
    int m_copiedCodeBlockRow = -1;
    SelectableText::Selection m_selection;
    QHash<QString, MessageFeedback> m_messageFeedback;
    QHash<QString, qreal> m_userMessageExpansionProgress;
    QHash<QString, qreal> m_userCopyButtonOpacity;
    QString m_streamingMessageId;
    mutable QCache<QString, TextDocumentCacheEntry> m_textDocumentCache;
    mutable QCache<QString, TextSizeCacheEntry> m_textSizeCache;
    mutable QCache<QString, UrlRangesCacheEntry> m_urlRangesCache;
    mutable QCache<QString, MarkdownCacheEntry> m_markdownCache;
    mutable QCache<QString, MarkdownLayoutCacheEntry> m_markdownLayoutCache;
    MarkdownDelegate m_markdownDelegate;

    static constexpr int kVerticalMargin = 8;
    static constexpr int kHorizontalMargin = 24;
    static constexpr int kBubblePadding = 12;
    static constexpr int kBubbleRadius = 12;
    static constexpr int kMarkdownHorizontalInset = 18;
    static constexpr int kActionButtonSize = 24;
    static constexpr int kActionIconSize = 15;
    static constexpr int kActionGap = 6;
    static constexpr int kActionTopMargin = 4;
    static constexpr int kUserCollapsedMaxLines = 10;
    static constexpr int kUserExpandTopGap = 6;
    static constexpr int kUserExpandHeight = 22;
    static constexpr int kUserCopyButtonSize = 24;
    static constexpr int kUserCopyButtonTopMargin = 4;
};
