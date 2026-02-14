// Python/pybind11 headers MUST come before Qt headers to avoid 'slots' macro conflict
#include "PythonBridge.hpp"
#include "PythonResult.hpp"

#include "PythonEditorWidget.hpp"
#include "PythonSyntaxHighlighter.hpp"
#include "Python_Widget/Core/PythonWidgetState.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// =============================================================================
// LineNumberArea — helper widget painted by LineNumberEditor
// =============================================================================

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(LineNumberEditor * editor)
        : QWidget(editor), _editor(editor) {}

    [[nodiscard]] QSize sizeHint() const override {
        return {_editor->lineNumberAreaWidth(), 0};
    }

protected:
    void paintEvent(QPaintEvent * event) override {
        _editor->lineNumberAreaPaintEvent(event);
    }

private:
    LineNumberEditor * _editor;
};

// =============================================================================
// LineNumberEditor implementation
// =============================================================================

LineNumberEditor::LineNumberEditor(QWidget * parent)
    : QPlainTextEdit(parent)
    , _line_number_area(new LineNumberArea(this)) {

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &LineNumberEditor::_updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &LineNumberEditor::_updateLineNumberArea);

    _updateLineNumberAreaWidth(0);
}

int LineNumberEditor::lineNumberAreaWidth() const {
    if (!_show_line_numbers) {
        return 0;
    }

    int digits = 1;
    int max_val = qMax(1, blockCount());
    while (max_val >= 10) {
        max_val /= 10;
        ++digits;
    }

    int const space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 6;
    return space;
}

void LineNumberEditor::setShowLineNumbers(bool show) {
    _show_line_numbers = show;
    _updateLineNumberAreaWidth(0);
}

void LineNumberEditor::_updateLineNumberAreaWidth(int /*new_block_count*/) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LineNumberEditor::_updateLineNumberArea(QRect const & rect, int dy) {
    if (dy) {
        _line_number_area->scroll(0, dy);
    } else {
        _line_number_area->update(0, rect.y(), _line_number_area->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        _updateLineNumberAreaWidth(0);
    }
}

void LineNumberEditor::resizeEvent(QResizeEvent * event) {
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    _line_number_area->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void LineNumberEditor::lineNumberAreaPaintEvent(QPaintEvent * event) {
    if (!_show_line_numbers) {
        return;
    }

    QPainter painter(_line_number_area);
    painter.fillRect(event->rect(), QColor(45, 45, 45));

    QTextBlock block = firstVisibleBlock();
    int block_number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(block_number + 1);
            painter.setPen(QColor(133, 133, 133));
            painter.drawText(0, top, _line_number_area->width() - 4,
                             fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++block_number;
    }
}

// =============================================================================
// PythonEditorWidget implementation
// =============================================================================

PythonEditorWidget::PythonEditorWidget(std::shared_ptr<PythonWidgetState> state,
                                       PythonBridge * bridge,
                                       QWidget * parent)
    : QWidget(parent)
    , _state(std::move(state))
    , _bridge(bridge) {
    _setupUI();

    // Restore state
    if (_state) {
        _current_file_path = _state->lastScriptPath();
        updateFontSize(_state->fontSize());

        // Restore editor content from state if we have unsaved content
        auto const & content = _state->editorContent();
        if (!content.isEmpty()) {
            _editor->setPlainText(content);
            _modified = false;
        } else if (!_current_file_path.isEmpty()) {
            // Try to load from file path
            QFile file(_current_file_path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                _editor->setPlainText(QString::fromUtf8(file.readAll()));
                _modified = false;
            }
        }
    }
}

PythonEditorWidget::~PythonEditorWidget() {
    // Save editor content to state for persistence
    if (_state) {
        _state->setEditorContent(_editor->toPlainText());
    }
}

void PythonEditorWidget::_setupUI() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(2);

    // Accept drag-and-drop
    setAcceptDrops(true);

    // Button toolbar
    auto * toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(4, 4, 4, 0);

    _open_button = new QPushButton(QStringLiteral("Open"), this);
    _open_button->setToolTip(QStringLiteral("Open a Python script file"));
    connect(_open_button, &QPushButton::clicked, this, &PythonEditorWidget::openFile);
    toolbar->addWidget(_open_button);

    _save_button = new QPushButton(QStringLiteral("Save"), this);
    _save_button->setToolTip(QStringLiteral("Save the current script (Ctrl+S)"));
    connect(_save_button, &QPushButton::clicked, this, &PythonEditorWidget::saveFile);
    toolbar->addWidget(_save_button);

    // Recent files menu button
    _recent_button = new QToolButton(this);
    _recent_button->setText(QStringLiteral("Recent"));
    _recent_button->setToolTip(QStringLiteral("Open a recently used script"));
    _recent_button->setPopupMode(QToolButton::InstantPopup);
    _recent_menu = new QMenu(this);
    _recent_button->setMenu(_recent_menu);
    _buildRecentFilesMenu();
    toolbar->addWidget(_recent_button);

    // Templates menu button
    _templates_button = new QToolButton(this);
    _templates_button->setText(QStringLiteral("Templates"));
    _templates_button->setToolTip(QStringLiteral("Create new script from template"));
    _templates_button->setPopupMode(QToolButton::InstantPopup);
    _templates_menu = new QMenu(this);
    _templates_button->setMenu(_templates_menu);
    _buildTemplatesMenu();
    toolbar->addWidget(_templates_button);

    toolbar->addStretch();

    _run_sel_button = new QPushButton(QStringLiteral("Run Selection"), this);
    _run_sel_button->setToolTip(QStringLiteral("Execute selected text"));
    connect(_run_sel_button, &QPushButton::clicked, this, &PythonEditorWidget::runSelection);
    toolbar->addWidget(_run_sel_button);

    _run_button = new QPushButton(QStringLiteral("Run Script"), this);
    _run_button->setToolTip(QStringLiteral("Execute entire script (Ctrl+Shift+Enter)"));
    connect(_run_button, &QPushButton::clicked, this, &PythonEditorWidget::runScript);
    toolbar->addWidget(_run_button);

    main_layout->addLayout(toolbar);

    // Code editor with line numbers
    _editor = new LineNumberEditor(this);

    QFont mono_font(QStringLiteral("Courier New"));
    mono_font.setStyleHint(QFont::Monospace);
    mono_font.setPointSize(_state ? _state->fontSize() : 10);
    _editor->setFont(mono_font);
    _editor->setTabStopDistance(
        QFontMetricsF(mono_font).horizontalAdvance(QLatin1Char(' ')) * 4);

    // Dark background
    QPalette pal = _editor->palette();
    pal.setColor(QPalette::Base, QColor(30, 30, 30));
    pal.setColor(QPalette::Text, QColor(212, 212, 212));
    _editor->setPalette(pal);

    _highlighter = new PythonSyntaxHighlighter(_editor->document());

    // Track modifications
    connect(_editor, &QPlainTextEdit::textChanged,
            this, &PythonEditorWidget::_onTextChanged);

    main_layout->addWidget(_editor);
}

void PythonEditorWidget::runScript() {
    if (!_bridge) {
        return;
    }

    QString const code = _editor->toPlainText();
    if (code.trimmed().isEmpty()) {
        return;
    }

    // If file is saved, execute via file for better tracebacks
    if (!_current_file_path.isEmpty() && !_modified) {
        auto result = _bridge->executeFile(_current_file_path.toStdString());
        emit outputGenerated(
            QString::fromStdString(result.stdout_text),
            QString::fromStdString(result.stderr_text),
            result.success);
    } else {
        // Execute from text buffer
        auto result = _bridge->execute(code.toStdString());
        emit outputGenerated(
            QString::fromStdString(result.stdout_text),
            QString::fromStdString(result.stderr_text),
            result.success);
    }

    emit executionFinished();
}

void PythonEditorWidget::runSelection() {
    if (!_bridge) {
        return;
    }

    QTextCursor cursor = _editor->textCursor();
    QString const selected = cursor.selectedText();
    if (selected.trimmed().isEmpty()) {
        return;
    }

    // QPlainTextEdit uses Unicode paragraph separator; convert to \n
    QString code = selected;
    code.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

    auto result = _bridge->execute(code.toStdString());
    emit outputGenerated(
        QString::fromStdString(result.stdout_text),
        QString::fromStdString(result.stderr_text),
        result.success);

    emit executionFinished();
}

void PythonEditorWidget::openFile() {
    QString const path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Python Script"),
        _current_file_path.isEmpty() ? QDir::homePath() : QFileInfo(_current_file_path).absolutePath(),
        QStringLiteral("Python Files (*.py);;All Files (*)"));

    if (path.isEmpty()) {
        return;
    }

    openFilePath(path);
}

void PythonEditorWidget::openFilePath(QString const & path) {
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("Could not open file: %1").arg(path));
        return;
    }

    _editor->setPlainText(QString::fromUtf8(file.readAll()));
    _current_file_path = path;
    _modified = false;

    if (_state) {
        _state->setLastScriptPath(path);
        _state->addRecentScript(path);
        _buildRecentFilesMenu();
    }
}

void PythonEditorWidget::saveFile() {
    if (_current_file_path.isEmpty()) {
        saveFileAs();
        return;
    }

    QFile file(_current_file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("Could not save file: %1").arg(_current_file_path));
        return;
    }

    file.write(_editor->toPlainText().toUtf8());
    _modified = false;
    _updateTitle();

    if (_state) {
        _state->setLastScriptPath(_current_file_path);
    }
}

void PythonEditorWidget::saveFileAs() {
    QString const path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Python Script"),
        _current_file_path.isEmpty() ? QDir::homePath() : _current_file_path,
        QStringLiteral("Python Files (*.py);;All Files (*)"));

    if (path.isEmpty()) {
        return;
    }

    _current_file_path = path;
    saveFile();

    if (_state) {
        _state->addRecentScript(path);
        _buildRecentFilesMenu();
    }
}

void PythonEditorWidget::updateFontSize(int size) {
    QFont font = _editor->font();
    font.setPointSize(size);
    _editor->setFont(font);
    _editor->setTabStopDistance(
        QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 4);
}

void PythonEditorWidget::setShowLineNumbers(bool show) {
    _editor->setShowLineNumbers(show);
}

void PythonEditorWidget::_onTextChanged() {
    if (!_modified) {
        _modified = true;
        _updateTitle();
    }
}

void PythonEditorWidget::_updateTitle() {
    // This could be used to update a tab title with modified indicator
    // For now, we just track the modified state
    Q_UNUSED(_modified);
}

// =============================================================================
// Templates (Phase 5)
// =============================================================================

void PythonEditorWidget::_buildTemplatesMenu() {
    _templates_menu->clear();

    auto addTemplate = [this](QString const & name, QString const & description) {
        auto * action = _templates_menu->addAction(name);
        action->setToolTip(description);
        connect(action, &QAction::triggered, this, [this, name]() {
            loadTemplate(name);
        });
    };

    addTemplate(QStringLiteral("Blank Script"),
                QStringLiteral("Empty Python script"));
    addTemplate(QStringLiteral("AnalogTimeSeries Filter"),
                QStringLiteral("Bandpass filter an AnalogTimeSeries"));
    addTemplate(QStringLiteral("Batch Processing"),
                QStringLiteral("Process all keys of a given type"));
    addTemplate(QStringLiteral("Data Export"),
                QStringLiteral("Export data to CSV/NumPy file"));
    addTemplate(QStringLiteral("Event Detection"),
                QStringLiteral("Detect events from analog data"));
    addTemplate(QStringLiteral("Statistics Summary"),
                QStringLiteral("Print summary statistics for all data"));
}

void PythonEditorWidget::loadTemplate(QString const & name) {
    // Check if there's unsaved content
    if (_modified && !_editor->toPlainText().trimmed().isEmpty()) {
        auto const answer = QMessageBox::question(
            this,
            QStringLiteral("Unsaved Changes"),
            QStringLiteral("The editor has unsaved changes. Replace with template?"),
            QMessageBox::Yes | QMessageBox::No);
        if (answer == QMessageBox::No) {
            return;
        }
    }

    QString const content = _getTemplateContent(name);
    _editor->setPlainText(content);
    _current_file_path.clear();
    _modified = false;
    _updateTitle();
}

QString PythonEditorWidget::_getTemplateContent(QString const & name) {
    if (name == QStringLiteral("Blank Script")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"New script.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "from whiskertoolbox_python import *\n"
            "\n"
            "# dm is the DataManager — already available\n"
            "# print(dm.getAllKeys())\n"
        );
    }

    if (name == QStringLiteral("AnalogTimeSeries Filter")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"Apply a bandpass filter to an AnalogTimeSeries.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "from whiskertoolbox_python import AnalogTimeSeries\n"
            "\n"
            "# --- Configuration ---\n"
            "INPUT_KEY  = \"whisker_angle\"        # Key of the input signal\n"
            "OUTPUT_KEY = \"whisker_angle_filtered\" # Key for the filtered result\n"
            "LOW_CUT    = 1.0                      # Low cutoff frequency (Hz)\n"
            "HIGH_CUT   = 50.0                     # High cutoff frequency (Hz)\n"
            "SAMPLE_RATE = 500.0                   # Sampling rate (Hz)\n"
            "FILTER_ORDER = 4\n"
            "\n"
            "# --- Load data ---\n"
            "analog = dm.getData(INPUT_KEY)\n"
            "if analog is None:\n"
            "    raise ValueError(f\"Key '{INPUT_KEY}' not found in DataManager\")\n"
            "\n"
            "values = np.array(analog.values, copy=True)  # copy to get writeable array\n"
            "times  = analog.getTimeSeries()\n"
            "\n"
            "# --- Filter ---\n"
            "from scipy.signal import butter, filtfilt\n"
            "\n"
            "nyq = 0.5 * SAMPLE_RATE\n"
            "b, a = butter(FILTER_ORDER, [LOW_CUT / nyq, HIGH_CUT / nyq], btype='band')\n"
            "filtered = filtfilt(b, a, values).astype(np.float32)\n"
            "\n"
            "# --- Store result ---\n"
            "ts = AnalogTimeSeries(filtered.tolist(), times)\n"
            "dm.setData(OUTPUT_KEY, ts, dm.getTimeKey(INPUT_KEY))\n"
            "print(f\"Filtered {len(filtered)} samples → '{OUTPUT_KEY}'\")\n"
        );
    }

    if (name == QStringLiteral("Batch Processing")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"Process all AnalogTimeSeries keys in the DataManager.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "from whiskertoolbox_python import DataType\n"
            "\n"
            "# Get all AnalogTimeSeries keys\n"
            "keys = dm.getKeys(DataType.AnalogTimeSeries)\n"
            "print(f\"Found {len(keys)} AnalogTimeSeries:\")\n"
            "\n"
            "for key in keys:\n"
            "    data = dm.getData(key)\n"
            "    values = np.array(data.values, copy=False)\n"
            "    print(f\"  {key}: {len(values)} samples, \"\n"
            "          f\"mean={values.mean():.3f}, std={values.std():.3f}\")\n"
            "\n"
            "# ---- Add your processing below ----\n"
            "# for key in keys:\n"
            "#     data = dm.getData(key)\n"
            "#     # ... process ...\n"
            "#     dm.setData(key + \"_processed\", result, dm.getTimeKey(key))\n"
        );
    }

    if (name == QStringLiteral("Data Export")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"Export data to file.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "\n"
            "# --- Configuration ---\n"
            "KEY = \"whisker_angle\"\n"
            "OUTPUT_PATH = \"exported_data.csv\"  # or .npy\n"
            "\n"
            "data = dm.getData(KEY)\n"
            "if data is None:\n"
            "    raise ValueError(f\"Key '{KEY}' not found\")\n"
            "\n"
            "values = np.array(data.values, copy=False)\n"
            "\n"
            "# CSV export\n"
            "np.savetxt(OUTPUT_PATH, values, delimiter=',',\n"
            "           header=KEY, comments='')\n"
            "print(f\"Exported {len(values)} values to {OUTPUT_PATH}\")\n"
            "\n"
            "# Alternative: NumPy binary\n"
            "# np.save(KEY + '.npy', values)\n"
        );
    }

    if (name == QStringLiteral("Event Detection")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"Detect threshold-crossing events from analog data.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "from whiskertoolbox_python import DigitalEventSeries\n"
            "\n"
            "# --- Configuration ---\n"
            "INPUT_KEY  = \"whisker_angle\"\n"
            "OUTPUT_KEY = \"threshold_events\"\n"
            "THRESHOLD  = 10.0\n"
            "DIRECTION  = \"rising\"    # 'rising', 'falling', or 'both'\n"
            "\n"
            "data = dm.getData(INPUT_KEY)\n"
            "if data is None:\n"
            "    raise ValueError(f\"Key '{INPUT_KEY}' not found\")\n"
            "\n"
            "values = np.array(data.values, copy=False)\n"
            "\n"
            "# Find crossings\n"
            "above = values > THRESHOLD\n"
            "if DIRECTION == \"rising\":\n"
            "    crossings = np.where(np.diff(above.astype(int)) == 1)[0] + 1\n"
            "elif DIRECTION == \"falling\":\n"
            "    crossings = np.where(np.diff(above.astype(int)) == -1)[0] + 1\n"
            "else:\n"
            "    crossings = np.where(np.diff(above.astype(int)) != 0)[0] + 1\n"
            "\n"
            "# Create event series\n"
            "events = DigitalEventSeries(crossings.tolist())\n"
            "dm.setData(OUTPUT_KEY, events, dm.getTimeKey(INPUT_KEY))\n"
            "print(f\"Detected {len(crossings)} {DIRECTION} crossings → '{OUTPUT_KEY}'\")\n"
        );
    }

    if (name == QStringLiteral("Statistics Summary")) {
        return QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\"\"\"Print summary statistics for all data in the DataManager.\"\"\"\n"
            "\n"
            "import numpy as np\n"
            "from whiskertoolbox_python import DataType\n"
            "\n"
            "all_keys = dm.getAllKeys()\n"
            "print(f\"DataManager contains {len(all_keys)} data objects:\\n\")\n"
            "\n"
            "for key in all_keys:\n"
            "    dtype = dm.getType(key)\n"
            "    print(f\"  [{dtype.name}] {key}\")\n"
            "\n"
            "    data = dm.getData(key)\n"
            "    if data is None:\n"
            "        print(\"    (type not accessible from Python)\")\n"
            "        continue\n"
            "\n"
            "    if hasattr(data, 'values'):\n"
            "        vals = np.array(data.values, copy=False)\n"
            "        print(f\"    samples: {len(vals)}, \"\n"
            "              f\"min: {vals.min():.4f}, max: {vals.max():.4f}, \"\n"
            "              f\"mean: {vals.mean():.4f}, std: {vals.std():.4f}\")\n"
            "    elif hasattr(data, 'size'):\n"
            "        print(f\"    size: {data.size()}\")\n"
            "\n"
            "print(\"\\nDone.\")\n"
        );
    }

    // Fallback: empty
    return {};
}

// =============================================================================
// Recent Files Menu (Phase 5)
// =============================================================================

void PythonEditorWidget::_buildRecentFilesMenu() {
    _recent_menu->clear();

    if (!_state) {
        auto * empty_action = _recent_menu->addAction(QStringLiteral("(no recent files)"));
        empty_action->setEnabled(false);
        return;
    }

    auto const recent = _state->recentScripts();

    if (recent.empty()) {
        auto * empty_action = _recent_menu->addAction(QStringLiteral("(no recent files)"));
        empty_action->setEnabled(false);
        return;
    }

    for (auto const & path : recent) {
        // Show just the filename, with full path as tooltip
        QFileInfo fi(path);
        auto * action = _recent_menu->addAction(fi.fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            openFilePath(path);
        });
    }

    _recent_menu->addSeparator();
    auto * clear_action = _recent_menu->addAction(QStringLiteral("Clear Recent Files"));
    connect(clear_action, &QAction::triggered, this, [this]() {
        if (_state) {
            // Clear the recent scripts list
            auto & data = const_cast<PythonWidgetStateData &>(_state->data());
            data.recent_scripts.clear();
            _buildRecentFilesMenu();
        }
    });
}

// =============================================================================
// Drag-and-Drop (Phase 5)
// =============================================================================

void PythonEditorWidget::dragEnterEvent(QDragEnterEvent * event) {
    if (event->mimeData()->hasUrls()) {
        for (auto const & url : event->mimeData()->urls()) {
            if (url.isLocalFile() && url.toLocalFile().endsWith(QStringLiteral(".py"))) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void PythonEditorWidget::dragMoveEvent(QDragMoveEvent * event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QWidget::dragMoveEvent(event);
    }
}

void PythonEditorWidget::dropEvent(QDropEvent * event) {
    if (!event->mimeData()->hasUrls()) {
        QWidget::dropEvent(event);
        return;
    }

    for (auto const & url : event->mimeData()->urls()) {
        if (url.isLocalFile() && url.toLocalFile().endsWith(QStringLiteral(".py"))) {
            openFilePath(url.toLocalFile());
            event->acceptProposedAction();
            return;  // Only open the first .py file
        }
    }

    QWidget::dropEvent(event);
}
