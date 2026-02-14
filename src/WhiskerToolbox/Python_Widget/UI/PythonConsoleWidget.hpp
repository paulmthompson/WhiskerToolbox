#ifndef PYTHON_CONSOLE_WIDGET_HPP
#define PYTHON_CONSOLE_WIDGET_HPP

/**
 * @file PythonConsoleWidget.hpp
 * @brief Interactive Python REPL console widget
 *
 * Provides a split-pane console with:
 * - Read-only output area (top) showing color-coded stdout/stderr
 * - Multi-line input area (bottom) with syntax highlighting
 * - Command history navigation (Up/Down arrows)
 * - Shift+Enter to execute, Ctrl+L to clear output
 *
 * @see PythonBridge for the execution backend
 * @see PythonSyntaxHighlighter for syntax colorization
 */

#include <QWidget>

#include <memory>
#include <vector>

class PythonBridge;
class PythonSyntaxHighlighter;
class PythonWidgetState;
class QPlainTextEdit;
class QPushButton;
class QSplitter;

class PythonConsoleWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonConsoleWidget(std::shared_ptr<PythonWidgetState> state,
                                PythonBridge * bridge,
                                QWidget * parent = nullptr);
    ~PythonConsoleWidget() override;

    /// Execute the current input text
    void executeInput();

    /// Clear the output area
    void clearOutput();

    /// Get the input text edit (for inserting text from properties panel)
    [[nodiscard]] QPlainTextEdit * inputEdit() const { return _input_edit; }

    /// Apply current font size from state
    void updateFontSize(int size);

    /// Apply auto-scroll setting
    void setAutoScroll(bool enabled);

signals:
    /// Emitted after each execution (for namespace refresh)
    void executionFinished();

protected:
    bool eventFilter(QObject * obj, QEvent * event) override;

private:
    void _setupUI();
    void _appendOutput(QString const & text, QColor const & color);
    void _appendPrompt(QString const & input);
    void _navigateHistory(int direction);  // -1 = older, +1 = newer

    std::shared_ptr<PythonWidgetState> _state;
    PythonBridge * _bridge;

    // UI elements
    QSplitter * _splitter{nullptr};
    QPlainTextEdit * _output_edit{nullptr};
    QPlainTextEdit * _input_edit{nullptr};
    QPushButton * _run_button{nullptr};
    QPushButton * _clear_button{nullptr};

    // Syntax highlighter for input
    PythonSyntaxHighlighter * _input_highlighter{nullptr};

    // Command history
    std::vector<QString> _history;
    int _history_index{-1};
    QString _history_buffer;  // saves current input when navigating

    bool _auto_scroll{true};

    static QColor const _stdout_color;
    static QColor const _stderr_color;
    static QColor const _prompt_color;
};

#endif // PYTHON_CONSOLE_WIDGET_HPP
