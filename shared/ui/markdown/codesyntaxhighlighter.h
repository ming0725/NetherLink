#ifndef CODESYNTAXHIGHLIGHTER_H
#define CODESYNTAXHIGHLIGHTER_H

#include <QString>
#include <QVector>

class CodeSyntaxHighlighter
{
public:
    enum class TokenType {
        Keyword,
        TypeKeyword,
        Function,
        Comment,
        String,
        Preprocessor,
        Number,
        Operator
    };

    struct TokenSpan {
        int start = 0;
        int length = 0;
        TokenType type = TokenType::Keyword;
    };

    static QVector<TokenSpan> highlight(const QString &code, const QString &language);
    static QString displayName(const QString &language);
    static bool hasRules(const QString &language);

private:
    CodeSyntaxHighlighter() = delete;
};

#endif // CODESYNTAXHIGHLIGHTER_H
