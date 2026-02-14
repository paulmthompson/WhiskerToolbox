#ifndef PYTHON_EDITOR_WIDGET_HPP
#define PYTHON_EDITOR_WIDGET_HPP

/**
 * @file PythonEditorWidget.hpp
 * @brief Script editor widget with line numbers and file operations
 *
 * Provides a Python script editor with:
 * - Line number gutter
 * - Syntax highlighting
 * - Open / Save / Save As / Run Script / Run Selection
 * - Modified indicator in state
 * - Monospace font with configurable size
 * - Script templates (New from template menu)
 * - Drag-and-drop .py file support
 * - Recent files menu
 *
 * @see PythonSyntaxHighlighter for syntax colorization
 * @see PythonBridge for script execution
 */

#include <QPlainTextEdit>
#include <QWidget>

#include <memory>

class PythonBridge;
class PythonSyntaxHighlighter;
class PythonWidgetState;
class QMenu;
class QPushButton;
class QToolButton;

/**
 * @brief Code editor with line numbers
 *
 * A QPlainTextEdit subclass that paints a line-number gutter on the left side.
 */
class LineNumberEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit LineNumberEditor(QWidget * parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent * event);
    [[nodiscard]] int lineNumberAreaWidth() const;

    void setShowLineNumbers(bool show);

protected:
    void resizeEvent(QResizeEvent * event) override;

private slots:
    void _updateLineNumberAreaWidth(int new_block_count);
    void _updateLineNumberArea(QRect const & rect, int dy);

private:
    QWidget * _line_number_area;
    bool _show_line_numbers{true};
};

/**
 * @brief Python script editor tab
 */
class PythonEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonEditorWidget(std::shared_ptr<PythonWidgetState> state,
                                PythonBridge * bridge,
                                QWidget * parent = nullptr);
    ~PythonEditorWidget() override;

    /// Run the entire script
    void runScript();

    /// Run only the selected text
    void runSelection();

    /// Open a .py file
    void openFile();

    /// Open a specific file by path
    void openFilePath(QString const & path);

    /// Save the current file
    void saveFile();

    /// Save as a new file
    void saveFileAs();

    /// Apply current font size
    void updateFontSize(int size);

    /// Show/hide line numbers
    void setShowLineNumbers(bool show);

    /// Load a template into the editor
    void loadTemplate(QString const & name);

signals:
    /// Emitted after script execution (for namespace refresh)
    void executionFinished();

    /// Emitted when output should be shown in the console
    void outputGenerated(QString const & stdout_text,
                         QString const & stderr_text,
                         bool success);

protected:
    void dragEnterEvent(QDragEnterEvent * event) override;
    void dragMoveEvent(QDragMoveEvent * event) override;
    void dropEvent(QDropEvent * event) override;

private:
    void _setupUI();
    void _onTextChanged();
    void _updateTitle();
    void _buildTemplatesMenu();
    void _buildRecentFilesMenu();

    /// Get template content by name
    [[nodiscard]] static QString _getTemplateContent(QString const & name);

    std::shared_ptr<PythonWidgetState> _state;
    PythonBridge * _bridge;

    LineNumberEditor * _editor{nullptr};
    PythonSyntaxHighlighter * _highlighter{nullptr};

    QPushButton * _open_button{nullptr};
    QPushButton * _save_button{nullptr};
    QPushButton * _run_button{nullptr};
    QPushButton * _run_sel_button{nullptr};
    QToolButton * _templates_button{nullptr};
    QToolButton * _recent_button{nullptr};

    QMenu * _templates_menu{nullptr};
    QMenu * _recent_menu{nullptr};

    QString _current_file_path;
    bool _modified{false};
};

#endif // PYTHON_EDITOR_WIDGET_HPP
