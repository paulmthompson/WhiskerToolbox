// Python/pybind11 headers MUST come before Qt headers to avoid 'slots' macro conflict
#include "PythonBridge.hpp"
#include "PythonEngine.hpp"
#include "PythonResult.hpp"

#include "PythonConsoleWidget.hpp"
#include "PythonSyntaxHighlighter.hpp"
#include "Python_Widget/Core/PythonWidgetState.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

QColor const PythonConsoleWidget::_stdout_color{212, 212, 212};   // light grey
QColor const PythonConsoleWidget::_stderr_color{244, 71, 71};     // red
QColor const PythonConsoleWidget::_prompt_color{86, 156, 214};    // blue

PythonConsoleWidget::PythonConsoleWidget(std::shared_ptr<PythonWidgetState> state,
                                         PythonBridge * bridge,
                                         QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state))
    , _bridge(bridge) {
    _setupUI();

    // Restore state
    if (_state) {
        _auto_scroll = _state->autoScroll();
        updateFontSize(_state->fontSize());

        // Restore command history from state
        auto const & history = _state->commandHistory();
        _history.assign(history.begin(), history.end());
    }
}

PythonConsoleWidget::~PythonConsoleWidget() {
    // Save command history to state
    if (_state) {
        std::vector<QString> history(_history.begin(), _history.end());
        _state->setCommandHistory(history);
    }
}

void PythonConsoleWidget::_setupUI() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(2);

    // Splitter: output (top) / input (bottom)
    _splitter = new QSplitter(Qt::Vertical, this);

    // --- Output area (read-only) ---
    _output_edit = new QPlainTextEdit(this);
    _output_edit->setReadOnly(true);
    _output_edit->setUndoRedoEnabled(false);

    QFont mono_font(QStringLiteral("Courier New"));
    mono_font.setStyleHint(QFont::Monospace);
    mono_font.setPointSize(_state ? _state->fontSize() : 10);
    _output_edit->setFont(mono_font);

    // Dark background for output
    QPalette output_pal = _output_edit->palette();
    output_pal.setColor(QPalette::Base, QColor(30, 30, 30));
    output_pal.setColor(QPalette::Text, QColor(212, 212, 212));
    _output_edit->setPalette(output_pal);

    // Show welcome message
    _appendOutput(QStringLiteral("WhiskerToolbox Python Console\n"), _prompt_color);
    if (_bridge) {
        auto const & engine = _bridge->engine();
        _appendOutput(
            QStringLiteral("Python %1\n").arg(QString::fromStdString(engine.pythonVersion())),
            _prompt_color);
    }
    _appendOutput(QStringLiteral("Type code below and press Shift+Enter to execute.\n\n"), _prompt_color);

    _splitter->addWidget(_output_edit);

    // --- Input area ---
    _input_edit = new QPlainTextEdit(this);
    _input_edit->setFont(mono_font);
    _input_edit->setPlaceholderText(QStringLiteral(">>> Enter Python code here..."));
    _input_edit->setTabStopDistance(
        QFontMetricsF(mono_font).horizontalAdvance(QLatin1Char(' ')) * 4);

    // Dark background for input
    QPalette input_pal = _input_edit->palette();
    input_pal.setColor(QPalette::Base, QColor(30, 30, 30));
    input_pal.setColor(QPalette::Text, QColor(212, 212, 212));
    _input_edit->setPalette(input_pal);

    // Install syntax highlighter on input
    _input_highlighter = new PythonSyntaxHighlighter(_input_edit->document());

    // Install event filter for keyboard shortcuts
    _input_edit->installEventFilter(this);

    _splitter->addWidget(_input_edit);

    // Set splitter proportions (3:1 output:input)
    _splitter->setStretchFactor(0, 3);
    _splitter->setStretchFactor(1, 1);

    main_layout->addWidget(_splitter);

    // --- Button bar ---
    auto * button_layout = new QHBoxLayout();
    button_layout->setContentsMargins(4, 0, 4, 4);

    button_layout->addStretch();

    _clear_button = new QPushButton(QStringLiteral("Clear"), this);
    _clear_button->setToolTip(QStringLiteral("Clear output (Ctrl+L)"));
    connect(_clear_button, &QPushButton::clicked, this, &PythonConsoleWidget::clearOutput);
    button_layout->addWidget(_clear_button);

    _run_button = new QPushButton(QStringLiteral("Run"), this);
    _run_button->setToolTip(QStringLiteral("Execute code (Shift+Enter)"));
    _run_button->setDefault(true);
    connect(_run_button, &QPushButton::clicked, this, &PythonConsoleWidget::executeInput);
    button_layout->addWidget(_run_button);

    main_layout->addLayout(button_layout);
}

void PythonConsoleWidget::executeInput() {
    QString const code = _input_edit->toPlainText().trimmed();
    if (code.isEmpty()) {
        return;
    }

    // Add to history
    if (_history.empty() || _history.back() != code) {
        _history.push_back(code);
    }
    _history_index = -1;
    _history_buffer.clear();

    // Show the prompt + code in output
    _appendPrompt(code);

    // Execute via bridge
    if (_bridge) {
        auto result = _bridge->execute(code.toStdString());

        if (!result.stdout_text.empty()) {
            _appendOutput(QString::fromStdString(result.stdout_text), _stdout_color);
        }
        if (!result.stderr_text.empty()) {
            _appendOutput(QString::fromStdString(result.stderr_text), _stderr_color);
        }
    }

    // Clear input
    _input_edit->clear();

    // Notify listeners (e.g. properties panel to refresh namespace)
    emit executionFinished();
}

void PythonConsoleWidget::clearOutput() {
    _output_edit->clear();
}

void PythonConsoleWidget::updateFontSize(int size) {
    QFont font = _output_edit->font();
    font.setPointSize(size);
    _output_edit->setFont(font);
    _input_edit->setFont(font);

    // Update tab stop
    _input_edit->setTabStopDistance(
        QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 4);
}

void PythonConsoleWidget::setAutoScroll(bool enabled) {
    _auto_scroll = enabled;
}

void PythonConsoleWidget::_appendOutput(QString const & text, QColor const & color) {
    QTextCharFormat fmt;
    fmt.setForeground(color);

    QTextCursor cursor = _output_edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, fmt);

    if (_auto_scroll) {
        _output_edit->verticalScrollBar()->setValue(
            _output_edit->verticalScrollBar()->maximum());
    }
}

void PythonConsoleWidget::_appendPrompt(QString const & input) {
    QStringList lines = input.split(QLatin1Char('\n'));
    QString formatted;
    for (int i = 0; i < lines.size(); ++i) {
        if (i == 0) {
            formatted += QStringLiteral(">>> ") + lines[i] + QStringLiteral("\n");
        } else {
            formatted += QStringLiteral("... ") + lines[i] + QStringLiteral("\n");
        }
    }
    _appendOutput(formatted, _prompt_color);
}

void PythonConsoleWidget::_navigateHistory(int direction) {
    if (_history.empty()) {
        return;
    }

    if (_history_index == -1) {
        // Save current input before navigating
        _history_buffer = _input_edit->toPlainText();
        _history_index = static_cast<int>(_history.size());
    }

    _history_index += direction;
    _history_index = std::clamp(_history_index, 0, static_cast<int>(_history.size()));

    if (_history_index == static_cast<int>(_history.size())) {
        // Back to current buffer
        _input_edit->setPlainText(_history_buffer);
        _history_index = -1;
    } else {
        _input_edit->setPlainText(_history[static_cast<size_t>(_history_index)]);
    }

    // Move cursor to end
    QTextCursor cursor = _input_edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    _input_edit->setTextCursor(cursor);
}

bool PythonConsoleWidget::eventFilter(QObject * obj, QEvent * event) {
    if (obj != _input_edit || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(obj, event);
    }

    auto * key_event = static_cast<QKeyEvent *>(event);

    // Shift+Enter or Shift+Return → execute
    if ((key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter)
        && (key_event->modifiers() & Qt::ShiftModifier)) {
        executeInput();
        return true;
    }

    // Ctrl+L → clear output
    if (key_event->key() == Qt::Key_L && (key_event->modifiers() & Qt::ControlModifier)) {
        clearOutput();
        return true;
    }

    // Up arrow (no modifiers, cursor on first line) → history older
    if (key_event->key() == Qt::Key_Up && key_event->modifiers() == Qt::NoModifier) {
        QTextCursor cursor = _input_edit->textCursor();
        if (cursor.blockNumber() == 0) {
            _navigateHistory(-1);
            return true;
        }
    }

    // Down arrow (no modifiers, cursor on last line) → history newer
    if (key_event->key() == Qt::Key_Down && key_event->modifiers() == Qt::NoModifier) {
        QTextCursor cursor = _input_edit->textCursor();
        if (cursor.blockNumber() == _input_edit->document()->blockCount() - 1) {
            _navigateHistory(1);
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}
