#ifndef PYTHON_SYNTAX_HIGHLIGHTER_HPP
#define PYTHON_SYNTAX_HIGHLIGHTER_HPP

/**
 * @file PythonSyntaxHighlighter.hpp
 * @brief QSyntaxHighlighter for Python code
 *
 * Highlights Python keywords, built-in functions, strings (single/double/triple-quoted),
 * comments, numbers, decorators, and special constants (self, True, False, None).
 */

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <vector>

class QTextDocument;

class PythonSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit PythonSyntaxHighlighter(QTextDocument * parent = nullptr);

protected:
    void highlightBlock(QString const & text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void _setupFormats();
    void _setupRules();
    void _highlightMultilineStrings(QString const & text);

    std::vector<HighlightRule> _rules;

    // Formats
    QTextCharFormat _keyword_format;
    QTextCharFormat _builtin_format;
    QTextCharFormat _string_format;
    QTextCharFormat _comment_format;
    QTextCharFormat _number_format;
    QTextCharFormat _decorator_format;
    QTextCharFormat _special_format;  // self, True, False, None
    QTextCharFormat _function_def_format;

    // Multi-line string delimiters
    QRegularExpression _triple_single;
    QRegularExpression _triple_double;
};

#endif // PYTHON_SYNTAX_HIGHLIGHTER_HPP
