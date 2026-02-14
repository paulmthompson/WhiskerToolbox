#ifndef PYTHON_WIDGET_STATE_HPP
#define PYTHON_WIDGET_STATE_HPP

/**
 * @file PythonWidgetState.hpp
 * @brief State class for Python_Widget
 *
 * PythonWidgetState manages the serializable state for the Python integration widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * @see EditorState for base class documentation
 * @see PythonWidgetStateData for the complete state structure
 */

#include "EditorState/EditorState.hpp"
#include "PythonWidgetStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

class PythonWidgetState : public EditorState {
    Q_OBJECT

public:
    explicit PythonWidgetState(QObject * parent = nullptr);
    ~PythonWidgetState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("PythonWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Direct Data Access ===

    [[nodiscard]] PythonWidgetStateData const & data() const { return _data; }

    // === State Accessors (existing) ===

    [[nodiscard]] QString lastScriptPath() const { return QString::fromStdString(_data.last_script_path); }
    void setLastScriptPath(QString const & path);

    [[nodiscard]] bool autoScroll() const { return _data.auto_scroll; }
    void setAutoScroll(bool enabled);

    [[nodiscard]] int fontSize() const { return _data.font_size; }
    void setFontSize(int size);

    // === New State Accessors (Phase 4) ===

    [[nodiscard]] std::vector<QString> commandHistory() const;
    void setCommandHistory(std::vector<QString> const & history);

    [[nodiscard]] std::vector<QString> recentScripts() const;
    void addRecentScript(QString const & path);

    [[nodiscard]] bool showLineNumbers() const { return _data.show_line_numbers; }
    void setShowLineNumbers(bool show);

    [[nodiscard]] QString editorContent() const { return QString::fromStdString(_data.editor_content); }
    void setEditorContent(QString const & content);

    // === Phase 5 Accessors ===

    [[nodiscard]] QString scriptArguments() const { return QString::fromStdString(_data.script_arguments); }
    void setScriptArguments(QString const & args);

    [[nodiscard]] QString autoImportPrelude() const { return QString::fromStdString(_data.auto_import_prelude); }
    void setAutoImportPrelude(QString const & prelude);

    [[nodiscard]] bool preludeEnabled() const { return _data.prelude_enabled; }
    void setPreludeEnabled(bool enabled);

    [[nodiscard]] QString lastWorkingDirectory() const { return QString::fromStdString(_data.last_working_directory); }
    void setLastWorkingDirectory(QString const & dir);

    // === Phase 6 Accessors ===

    [[nodiscard]] QString venvPath() const { return QString::fromStdString(_data.venv_path); }
    void setVenvPath(QString const & path);

signals:
    void lastScriptPathChanged(QString const & path);
    void autoScrollChanged(bool enabled);
    void fontSizeChanged(int size);
    void showLineNumbersChanged(bool show);
    void scriptArgumentsChanged(QString const & args);
    void preludeChanged(QString const & prelude);
    void preludeEnabledChanged(bool enabled);
    void workingDirectoryChanged(QString const & dir);
    void venvPathChanged(QString const & path);

private:
    PythonWidgetStateData _data;
};

#endif // PYTHON_WIDGET_STATE_HPP
