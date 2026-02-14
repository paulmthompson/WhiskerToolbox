#include "PythonSyntaxHighlighter.hpp"

#include <QRegularExpression>

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument * parent)
    : QSyntaxHighlighter(parent) {
    _setupFormats();
    _setupRules();
}

void PythonSyntaxHighlighter::_setupFormats() {
    // Keywords — bold blue
    _keyword_format.setForeground(QColor(86, 156, 214));   // VS Code blue
    _keyword_format.setFontWeight(QFont::Bold);

    // Built-in functions — cyan
    _builtin_format.setForeground(QColor(78, 201, 176));    // teal/cyan

    // Strings — orange
    _string_format.setForeground(QColor(206, 145, 120));    // VS Code string orange

    // Comments — green
    _comment_format.setForeground(QColor(106, 153, 85));    // VS Code comment green
    _comment_format.setFontItalic(true);

    // Numbers — light green
    _number_format.setForeground(QColor(181, 206, 168));    // VS Code number green

    // Decorators — yellow
    _decorator_format.setForeground(QColor(220, 220, 170)); // VS Code decorator yellow

    // Special constants (self, True, False, None) — blue
    _special_format.setForeground(QColor(86, 156, 214));
    _special_format.setFontItalic(true);

    // Function/class definitions — light yellow
    _function_def_format.setForeground(QColor(220, 220, 170));
}

void PythonSyntaxHighlighter::_setupRules() {
    // === Keywords ===
    QStringList const keywords = {
        QStringLiteral("and"),      QStringLiteral("as"),       QStringLiteral("assert"),
        QStringLiteral("async"),    QStringLiteral("await"),    QStringLiteral("break"),
        QStringLiteral("class"),    QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"),      QStringLiteral("elif"),     QStringLiteral("else"),
        QStringLiteral("except"),   QStringLiteral("finally"),  QStringLiteral("for"),
        QStringLiteral("from"),     QStringLiteral("global"),   QStringLiteral("if"),
        QStringLiteral("import"),   QStringLiteral("in"),       QStringLiteral("is"),
        QStringLiteral("lambda"),   QStringLiteral("nonlocal"), QStringLiteral("not"),
        QStringLiteral("or"),       QStringLiteral("pass"),     QStringLiteral("raise"),
        QStringLiteral("return"),   QStringLiteral("try"),      QStringLiteral("while"),
        QStringLiteral("with"),     QStringLiteral("yield"),
    };

    for (auto const & kw : keywords) {
        _rules.push_back({
            QRegularExpression(QStringLiteral("\\b") + kw + QStringLiteral("\\b")),
            _keyword_format
        });
    }

    // === Special constants ===
    QStringList const specials = {
        QStringLiteral("True"), QStringLiteral("False"), QStringLiteral("None"), QStringLiteral("self"),
    };
    for (auto const & sp : specials) {
        _rules.push_back({
            QRegularExpression(QStringLiteral("\\b") + sp + QStringLiteral("\\b")),
            _special_format
        });
    }

    // === Built-in functions ===
    QStringList const builtins = {
        QStringLiteral("abs"),     QStringLiteral("all"),       QStringLiteral("any"),
        QStringLiteral("bin"),     QStringLiteral("bool"),      QStringLiteral("bytes"),
        QStringLiteral("callable"),QStringLiteral("chr"),       QStringLiteral("classmethod"),
        QStringLiteral("dict"),    QStringLiteral("dir"),       QStringLiteral("divmod"),
        QStringLiteral("enumerate"),QStringLiteral("eval"),     QStringLiteral("exec"),
        QStringLiteral("filter"),  QStringLiteral("float"),     QStringLiteral("format"),
        QStringLiteral("frozenset"),QStringLiteral("getattr"),  QStringLiteral("globals"),
        QStringLiteral("hasattr"), QStringLiteral("hash"),      QStringLiteral("hex"),
        QStringLiteral("id"),      QStringLiteral("input"),     QStringLiteral("int"),
        QStringLiteral("isinstance"),QStringLiteral("issubclass"),QStringLiteral("iter"),
        QStringLiteral("len"),     QStringLiteral("list"),      QStringLiteral("locals"),
        QStringLiteral("map"),     QStringLiteral("max"),       QStringLiteral("min"),
        QStringLiteral("next"),    QStringLiteral("object"),    QStringLiteral("oct"),
        QStringLiteral("open"),    QStringLiteral("ord"),       QStringLiteral("pow"),
        QStringLiteral("print"),   QStringLiteral("property"),  QStringLiteral("range"),
        QStringLiteral("repr"),    QStringLiteral("reversed"),  QStringLiteral("round"),
        QStringLiteral("set"),     QStringLiteral("setattr"),   QStringLiteral("slice"),
        QStringLiteral("sorted"),  QStringLiteral("staticmethod"),QStringLiteral("str"),
        QStringLiteral("sum"),     QStringLiteral("super"),     QStringLiteral("tuple"),
        QStringLiteral("type"),    QStringLiteral("vars"),      QStringLiteral("zip"),
    };
    for (auto const & bi : builtins) {
        _rules.push_back({
            QRegularExpression(QStringLiteral("\\b") + bi + QStringLiteral("\\b")),
            _builtin_format
        });
    }

    // === Function/class definition names ===
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\bdef\\s+(\\w+)")),
        _function_def_format
    });
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\bclass\\s+(\\w+)")),
        _function_def_format
    });

    // === Decorators ===
    _rules.push_back({
        QRegularExpression(QStringLiteral("@\\w+(\\.\\w+)*")),
        _decorator_format
    });

    // === Numbers (int, float, hex, octal, binary, scientific) ===
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\b0[xX][0-9a-fA-F]+\\b")),
        _number_format
    });
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\b0[oO][0-7]+\\b")),
        _number_format
    });
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\b0[bB][01]+\\b")),
        _number_format
    });
    _rules.push_back({
        QRegularExpression(QStringLiteral("\\b[0-9]+\\.?[0-9]*([eE][+-]?[0-9]+)?\\b")),
        _number_format
    });

    // === Single-line strings (must come before comments) ===
    // f-strings, r-strings, b-strings etc. prefixes
    QString const str_prefix = QStringLiteral("[fFrRbBuU]{0,2}");

    // Double-quoted strings (not triple)
    _rules.push_back({
        QRegularExpression(str_prefix + QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\"")),
        _string_format
    });
    // Single-quoted strings (not triple)
    _rules.push_back({
        QRegularExpression(str_prefix + QStringLiteral("'(?:[^'\\\\]|\\\\.)*'")),
        _string_format
    });

    // === Comments (must be last single-line rule) ===
    _rules.push_back({
        QRegularExpression(QStringLiteral("#[^\n]*")),
        _comment_format
    });

    // === Multi-line string delimiters (handled separately) ===
    _triple_single = QRegularExpression(QStringLiteral("'''"));
    _triple_double = QRegularExpression(QStringLiteral("\"\"\""));
}

void PythonSyntaxHighlighter::highlightBlock(QString const & text) {
    // Apply single-line rules
    for (auto const & rule : _rules) {
        auto match_iter = rule.pattern.globalMatch(text);
        while (match_iter.hasNext()) {
            auto match = match_iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Handle multi-line strings
    _highlightMultilineStrings(text);
}

void PythonSyntaxHighlighter::_highlightMultilineStrings(QString const & text) {
    // State encoding:
    // 0 = not in multiline string
    // 1 = inside ''' string
    // 2 = inside """ string

    int const prev_state = previousBlockState();
    int start = 0;

    // Determine which delimiter we're looking for
    QRegularExpression const * delimiter = nullptr;
    int current_state = 0;

    if (prev_state == 1) {
        delimiter = &_triple_single;
        current_state = 1;
    } else if (prev_state == 2) {
        delimiter = &_triple_double;
        current_state = 2;
    }

    // If continuing a multi-line string from previous block
    if (delimiter) {
        auto match = delimiter->match(text, start);
        if (match.hasMatch()) {
            // Found closing delimiter
            int length = match.capturedEnd() - start;
            setFormat(start, length, _string_format);
            start = match.capturedEnd();
            current_state = 0;
            delimiter = nullptr;
        } else {
            // Entire line is part of the multi-line string
            setFormat(start, text.length() - start, _string_format);
            setCurrentBlockState(current_state);
            return;
        }
    }

    // Scan for opening triple-quote delimiters
    while (start < text.length()) {
        // Look for ''' or """ whichever comes first
        auto match_single = _triple_single.match(text, start);
        auto match_double = _triple_double.match(text, start);

        bool has_single = match_single.hasMatch();
        bool has_double = match_double.hasMatch();

        if (!has_single && !has_double) {
            break;
        }

        QRegularExpressionMatch opening_match;
        QRegularExpression const * closing_delim = nullptr;
        int new_state = 0;

        if (has_single && (!has_double || match_single.capturedStart() <= match_double.capturedStart())) {
            opening_match = match_single;
            closing_delim = &_triple_single;
            new_state = 1;
        } else {
            opening_match = match_double;
            closing_delim = &_triple_double;
            new_state = 2;
        }

        int open_start = opening_match.capturedStart();

        // Look for closing delimiter after the opening one
        auto closing_match = closing_delim->match(text, opening_match.capturedEnd());
        if (closing_match.hasMatch()) {
            // Entire string is on one line (or finishes on this line)
            int length = closing_match.capturedEnd() - open_start;
            setFormat(open_start, length, _string_format);
            start = closing_match.capturedEnd();
        } else {
            // Multi-line string starts here, doesn't end on this line
            setFormat(open_start, text.length() - open_start, _string_format);
            setCurrentBlockState(new_state);
            return;
        }
    }

    if (currentBlockState() == -1) {
        setCurrentBlockState(0);
    }
}
