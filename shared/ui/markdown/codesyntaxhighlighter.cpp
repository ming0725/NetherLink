#include "codesyntaxhighlighter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <utility>

namespace {

struct BlockCommentRule {
    QString start;
    QString end;
};

struct StringRule {
    QString start;
    QString end;
    QString escape;
    bool multiline = false;
};

struct LanguageRule {
    QString id;
    QString displayName;
    QStringList aliases;
    QStringList keywords;
    QStringList lineComments;
    QVector<BlockCommentRule> blockComments;
    QVector<StringRule> strings;
    QString functionPattern;
    bool raw = false;
};

struct RuleRepository {
    QVector<LanguageRule> rules;
    QHash<QString, int> aliasToRuleIndex;
    QSet<QString> loadedRuleFiles;
};

QString normalizedLanguage(QString language)
{
    language = language.trimmed().toLower();
    if (language.startsWith(QLatin1Char('.'))) {
        language.remove(0, 1);
    }
    return language;
}

QStringList syntaxRuleDirectories()
{
    QStringList paths;
    const QString applicationDir = QCoreApplication::applicationDirPath();
    if (!applicationDir.isEmpty()) {
        paths.append(QDir(applicationDir).absoluteFilePath(QStringLiteral("syntax")));
    }
    paths.append(QDir::current().absoluteFilePath(QStringLiteral("syntax")));
#ifdef QMARKDOWN_SOURCE_DIR
    paths.append(QDir(QStringLiteral(QMARKDOWN_SOURCE_DIR)).absoluteFilePath(QStringLiteral("syntax")));
#endif
    paths.removeDuplicates();
    return paths;
}

bool isSafeRuleFileStem(const QString &language)
{
    static const QRegularExpression safeStemExpression(QStringLiteral(R"(^[A-Za-z0-9_+.-]+$)"));
    return safeStemExpression.match(language).hasMatch();
}

QStringList stringsFromArray(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    values.removeDuplicates();
    return values;
}

LanguageRule parseRuleObject(const QJsonObject &object)
{
    LanguageRule rule;
    rule.id = normalizedLanguage(object.value(QStringLiteral("id")).toString());
    rule.displayName = object.value(QStringLiteral("displayName")).toString().trimmed();
    rule.aliases = stringsFromArray(object.value(QStringLiteral("aliases")).toArray());
    rule.keywords = stringsFromArray(object.value(QStringLiteral("keywords")).toArray());
    rule.lineComments = stringsFromArray(object.value(QStringLiteral("lineComments")).toArray());
    rule.functionPattern = object.value(QStringLiteral("functionPattern")).toString();
    rule.raw = object.value(QStringLiteral("raw")).toBool(false);

    for (const QJsonValue &commentValue : object.value(QStringLiteral("blockComments")).toArray()) {
        const QJsonObject commentObject = commentValue.toObject();
        BlockCommentRule comment;
        comment.start = commentObject.value(QStringLiteral("start")).toString();
        comment.end = commentObject.value(QStringLiteral("end")).toString();
        if (!comment.start.isEmpty() && !comment.end.isEmpty()) {
            rule.blockComments.append(comment);
        }
    }

    for (const QJsonValue &stringValue : object.value(QStringLiteral("strings")).toArray()) {
        const QJsonObject stringObject = stringValue.toObject();
        StringRule string;
        string.start = stringObject.value(QStringLiteral("start")).toString();
        string.end = stringObject.value(QStringLiteral("end")).toString();
        string.escape = stringObject.value(QStringLiteral("escape")).toString();
        string.multiline = stringObject.value(QStringLiteral("multiline")).toBool(false);
        if (!string.start.isEmpty() && !string.end.isEmpty()) {
            rule.strings.append(string);
        }
    }

    if (rule.displayName.isEmpty()) {
        rule.displayName = rule.id;
    }
    if (!rule.id.isEmpty() && !rule.aliases.contains(rule.id)) {
        rule.aliases.prepend(rule.id);
    }

    return rule;
}

QVector<LanguageRule> parseRules(const QByteArray &json)
{
    QVector<LanguageRule> rules;

    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return rules;
    }

    const QJsonObject root = document.object();
    const QJsonArray languages = root.value(QStringLiteral("languages")).toArray();
    if (!languages.isEmpty()) {
        rules.reserve(languages.size());
        for (const QJsonValue &value : languages) {
            LanguageRule rule = parseRuleObject(value.toObject());
            if (!rule.id.isEmpty()) {
                rules.append(rule);
            }
        }
        return rules;
    }

    LanguageRule rule = parseRuleObject(root);
    if (!rule.id.isEmpty()) {
        rules.append(rule);
    }

    return rules;
}

void addRule(RuleRepository &repository, LanguageRule rule)
{
    if (rule.id.isEmpty() || repository.aliasToRuleIndex.contains(rule.id)) {
        return;
    }

    const int ruleIndex = repository.rules.size();
    for (const QString &alias : std::as_const(rule.aliases)) {
        const QString normalized = normalizedLanguage(alias);
        if (!normalized.isEmpty() && !repository.aliasToRuleIndex.contains(normalized)) {
            repository.aliasToRuleIndex.insert(normalized, ruleIndex);
        }
    }
    repository.rules.append(std::move(rule));
}

void loadRuleFile(RuleRepository &repository, const QString &path)
{
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    const QString rulePath = canonicalPath.isEmpty() ? QDir::cleanPath(path) : canonicalPath;
    if (repository.loadedRuleFiles.contains(rulePath)) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    repository.loadedRuleFiles.insert(rulePath);

    const QVector<LanguageRule> rules = parseRules(file.readAll());
    for (LanguageRule rule : rules) {
        addRule(repository, std::move(rule));
    }
}

const LanguageRule *ruleByAlias(const RuleRepository &repository, const QString &language)
{
    const int ruleIndex = repository.aliasToRuleIndex.value(language, -1);
    if (ruleIndex < 0 || ruleIndex >= repository.rules.size()) {
        return nullptr;
    }
    return &repository.rules.at(ruleIndex);
}

void loadRuleFileForLanguage(RuleRepository &repository, const QString &language)
{
    if (!isSafeRuleFileStem(language)) {
        return;
    }

    for (const QString &directoryPath : syntaxRuleDirectories()) {
        const QString path = QDir(directoryPath).absoluteFilePath(language + QStringLiteral(".json"));
        if (QFileInfo::exists(path)) {
            loadRuleFile(repository, path);
            if (ruleByAlias(repository, language)) {
                return;
            }
        }
    }
}

void loadAvailableRuleFiles(RuleRepository &repository)
{
    for (const QString &directoryPath : syntaxRuleDirectories()) {
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        const QFileInfoList entries = directory.entryInfoList(QStringList{QStringLiteral("*.json")},
                                                              QDir::Files | QDir::Readable,
                                                              QDir::Name);
        for (const QFileInfo &entry : entries) {
            loadRuleFile(repository, entry.absoluteFilePath());
        }
    }
}

RuleRepository &repository()
{
    static RuleRepository rules;
    return rules;
}

const LanguageRule *ruleForLanguage(const QString &language)
{
    RuleRepository &rules = repository();
    const QString normalized = normalizedLanguage(language);
    if (normalized.isEmpty()) {
        return nullptr;
    }

    if (const LanguageRule *rule = ruleByAlias(rules, normalized)) {
        return rule;
    }

    loadRuleFileForLanguage(rules, normalized);
    if (const LanguageRule *rule = ruleByAlias(rules, normalized)) {
        return rule;
    }

    loadAvailableRuleFiles(rules);
    return ruleByAlias(rules, normalized);
}

bool startsWithAt(const QString &text, int index, const QString &needle)
{
    if (needle.isEmpty() || index < 0 || index + needle.size() > text.size()) {
        return false;
    }
    return text.mid(index, needle.size()) == needle;
}

void addSpan(QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
             QVector<bool> &occupied,
             int start,
             int length,
             CodeSyntaxHighlighter::TokenType type)
{
    if (start < 0 || length <= 0 || start >= occupied.size()) {
        return;
    }

    length = qMin(length, occupied.size() - start);
    for (int index = start; index < start + length; ++index) {
        if (occupied.at(index)) {
            return;
        }
    }

    CodeSyntaxHighlighter::TokenSpan span;
    span.start = start;
    span.length = length;
    span.type = type;
    spans.append(span);

    for (int index = start; index < start + length; ++index) {
        occupied[index] = true;
    }
}

struct DelimiterMatch {
    enum class Kind {
        None,
        LineComment,
        BlockComment,
        String
    };

    Kind kind = Kind::None;
    int startLength = 0;
    QString end;
    QString escape;
    bool multiline = false;
};

DelimiterMatch delimiterAt(const LanguageRule &rule, const QString &code, int index)
{
    DelimiterMatch best;

    for (const StringRule &string : rule.strings) {
        if (startsWithAt(code, index, string.start) && string.start.size() > best.startLength) {
            best.kind = DelimiterMatch::Kind::String;
            best.startLength = string.start.size();
            best.end = string.end;
            best.escape = string.escape;
            best.multiline = string.multiline;
        }
    }

    for (const BlockCommentRule &comment : rule.blockComments) {
        if (startsWithAt(code, index, comment.start) && comment.start.size() > best.startLength) {
            best.kind = DelimiterMatch::Kind::BlockComment;
            best.startLength = comment.start.size();
            best.end = comment.end;
            best.escape.clear();
            best.multiline = true;
        }
    }

    for (const QString &commentStart : rule.lineComments) {
        if (startsWithAt(code, index, commentStart) && commentStart.size() > best.startLength) {
            best.kind = DelimiterMatch::Kind::LineComment;
            best.startLength = commentStart.size();
            best.end.clear();
            best.escape.clear();
            best.multiline = false;
        }
    }

    return best;
}

int delimitedEnd(const QString &code, int start, const DelimiterMatch &match)
{
    if (match.kind == DelimiterMatch::Kind::LineComment) {
        const int newline = code.indexOf(QLatin1Char('\n'), start + match.startLength);
        return newline < 0 ? code.size() : newline;
    }

    int cursor = start + match.startLength;
    while (cursor < code.size()) {
        if (!match.escape.isEmpty() && startsWithAt(code, cursor, match.escape)) {
            cursor += match.escape.size();
            if (cursor < code.size()) {
                ++cursor;
            }
            continue;
        }

        if (startsWithAt(code, cursor, match.end)) {
            return cursor + match.end.size();
        }

        if (!match.multiline && code.at(cursor) == QLatin1Char('\n')) {
            return cursor;
        }

        ++cursor;
    }

    return code.size();
}

void addDelimitedSpans(const LanguageRule &rule,
                       const QString &code,
                       QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                       QVector<bool> &occupied)
{
    int cursor = 0;
    while (cursor < code.size()) {
        const DelimiterMatch match = delimiterAt(rule, code, cursor);
        if (match.kind == DelimiterMatch::Kind::None) {
            ++cursor;
            continue;
        }

        const int end = delimitedEnd(code, cursor, match);
        const CodeSyntaxHighlighter::TokenType type =
            match.kind == DelimiterMatch::Kind::String
                ? CodeSyntaxHighlighter::TokenType::String
                : CodeSyntaxHighlighter::TokenType::Comment;
        addSpan(spans, occupied, cursor, end - cursor, type);
        cursor = qMax(cursor + 1, end);
    }
}

bool rangeIsFree(const QVector<bool> &occupied, int start, int length)
{
    if (start < 0 || length <= 0 || start + length > occupied.size()) {
        return false;
    }
    for (int index = start; index < start + length; ++index) {
        if (occupied.at(index)) {
            return false;
        }
    }
    return true;
}

const QSet<QString> &typeKeywords()
{
    static const QSet<QString> keywords = {
        QStringLiteral("any"),
        QStringLiteral("bool"),
        QStringLiteral("boolean"),
        QStringLiteral("char"),
        QStringLiteral("char8_t"),
        QStringLiteral("char16_t"),
        QStringLiteral("char32_t"),
        QStringLiteral("double"),
        QStringLiteral("float"),
        QStringLiteral("int"),
        QStringLiteral("long"),
        QStringLiteral("never"),
        QStringLiteral("number"),
        QStringLiteral("object"),
        QStringLiteral("short"),
        QStringLiteral("signed"),
        QStringLiteral("string"),
        QStringLiteral("symbol"),
        QStringLiteral("unsigned"),
        QStringLiteral("void"),
        QStringLiteral("wchar_t")
    };
    return keywords;
}

void addPreprocessorSpans(const QString &code,
                          QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                          QVector<bool> &occupied)
{
    static const QRegularExpression directiveExpression(
        QStringLiteral(R"((^|\n)[ \t]*(#[A-Za-z_][A-Za-z0-9_]*))"));
    QRegularExpressionMatchIterator matches = directiveExpression.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const int directiveStart = match.capturedStart(2);
        const int directiveLength = match.capturedLength(2);
        addSpan(spans,
                occupied,
                directiveStart,
                directiveLength,
                CodeSyntaxHighlighter::TokenType::Preprocessor);

        if (match.captured(2) != QStringLiteral("#include")) {
            continue;
        }

        int cursor = directiveStart + directiveLength;
        while (cursor < code.size() && (code.at(cursor) == QLatin1Char(' ') ||
                                        code.at(cursor) == QLatin1Char('\t'))) {
            ++cursor;
        }

        if (cursor < code.size() && code.at(cursor) == QLatin1Char('<')) {
            const int end = code.indexOf(QLatin1Char('>'), cursor + 1);
            if (end > cursor) {
                addSpan(spans,
                        occupied,
                        cursor,
                        end - cursor + 1,
                        CodeSyntaxHighlighter::TokenType::String);
            }
        }
    }
}

void addNumberSpans(const QString &code,
                    QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                    QVector<bool> &occupied)
{
    static const QRegularExpression numberExpression(
        QStringLiteral(R"((?<![A-Za-z0-9_])(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)[uUlLfF]*(?![A-Za-z0-9_]))"));
    QRegularExpressionMatchIterator matches = numberExpression.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        addSpan(spans,
                occupied,
                match.capturedStart(),
                match.capturedLength(),
                CodeSyntaxHighlighter::TokenType::Number);
    }
}

void addOperatorSpans(const QString &code,
                      QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                      QVector<bool> &occupied)
{
    static const QRegularExpression operatorExpression(QStringLiteral(R"([+\-*/%=!<>|&^~]+)"));
    QRegularExpressionMatchIterator matches = operatorExpression.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        addSpan(spans,
                occupied,
                match.capturedStart(),
                match.capturedLength(),
                CodeSyntaxHighlighter::TokenType::Operator);
    }
}

void addKeywordSpans(const LanguageRule &rule,
                     const QString &code,
                     QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                     QVector<bool> &occupied)
{
    if (rule.keywords.isEmpty()) {
        return;
    }

    QStringList escapedKeywords;
    escapedKeywords.reserve(rule.keywords.size());
    for (const QString &keyword : rule.keywords) {
        escapedKeywords.append(QRegularExpression::escape(keyword));
    }
    escapedKeywords.sort(Qt::CaseInsensitive);

    const QString pattern = QStringLiteral(R"((?<![A-Za-z0-9_])(?:%1)(?![A-Za-z0-9_]))")
                                .arg(escapedKeywords.join(QLatin1Char('|')));
    QRegularExpression expression(pattern);
    QRegularExpressionMatchIterator matches = expression.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString keyword = match.captured();
        addSpan(spans,
                occupied,
                match.capturedStart(),
                match.capturedLength(),
                typeKeywords().contains(keyword)
                    ? CodeSyntaxHighlighter::TokenType::TypeKeyword
                    : CodeSyntaxHighlighter::TokenType::Keyword);
    }
}

void addFunctionSpans(const LanguageRule &rule,
                      const QString &code,
                      QVector<CodeSyntaxHighlighter::TokenSpan> &spans,
                      QVector<bool> &occupied)
{
    if (rule.functionPattern.isEmpty()) {
        return;
    }

    const QSet<QString> keywordSet(rule.keywords.cbegin(), rule.keywords.cend());
    QRegularExpression expression(rule.functionPattern);
    QRegularExpressionMatchIterator matches = expression.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const int group = match.lastCapturedIndex() >= 1 ? 1 : 0;
        const QString functionName = match.captured(group);
        const int start = match.capturedStart(group);
        const int length = match.capturedLength(group);
        if (keywordSet.contains(functionName) || !rangeIsFree(occupied, start, length)) {
            continue;
        }
        addSpan(spans,
                occupied,
                start,
                length,
                CodeSyntaxHighlighter::TokenType::Function);
    }
}

} // namespace

QVector<CodeSyntaxHighlighter::TokenSpan> CodeSyntaxHighlighter::highlight(const QString &code,
                                                                           const QString &language)
{
    QVector<TokenSpan> spans;
    const LanguageRule *rule = ruleForLanguage(language);
    if (!rule || rule->raw || code.isEmpty()) {
        return spans;
    }

    QVector<bool> occupied(code.size(), false);
    addDelimitedSpans(*rule, code, spans, occupied);
    addPreprocessorSpans(code, spans, occupied);
    addNumberSpans(code, spans, occupied);
    addKeywordSpans(*rule, code, spans, occupied);
    addFunctionSpans(*rule, code, spans, occupied);
    addOperatorSpans(code, spans, occupied);

    std::sort(spans.begin(), spans.end(), [](const TokenSpan &left, const TokenSpan &right) {
        return left.start < right.start;
    });
    return spans;
}

QString CodeSyntaxHighlighter::displayName(const QString &language)
{
    const QString normalized = normalizedLanguage(language);
    if (normalized.isEmpty()) {
        return QStringLiteral("Plain Text");
    }

    const LanguageRule *rule = ruleForLanguage(language);
    if (rule) {
        return rule->displayName;
    }

    return language.trimmed();
}

bool CodeSyntaxHighlighter::hasRules(const QString &language)
{
    const LanguageRule *rule = ruleForLanguage(language);
    return rule && !rule->raw;
}
