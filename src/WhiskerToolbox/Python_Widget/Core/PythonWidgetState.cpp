#include "PythonWidgetState.hpp"

#include <algorithm>

PythonWidgetState::PythonWidgetState(QObject * parent)
    : EditorState(parent) {
    _data.instance_id = getInstanceId().toStdString();
}

// === Type Identification ===

QString PythonWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void PythonWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// === Serialization ===

std::string PythonWidgetState::toJson() const {
    PythonWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool PythonWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<PythonWidgetStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        emit stateChanged();
        emit lastScriptPathChanged(QString::fromStdString(_data.last_script_path));
        emit autoScrollChanged(_data.auto_scroll);
        emit fontSizeChanged(_data.font_size);
        emit showLineNumbersChanged(_data.show_line_numbers);
        return true;
    }
    return false;
}

// === Existing Setters ===

void PythonWidgetState::setLastScriptPath(QString const & path) {
    std::string const path_std = path.toStdString();
    if (_data.last_script_path != path_std) {
        _data.last_script_path = path_std;
        markDirty();
        emit lastScriptPathChanged(path);
    }
}

void PythonWidgetState::setAutoScroll(bool enabled) {
    if (_data.auto_scroll != enabled) {
        _data.auto_scroll = enabled;
        markDirty();
        emit autoScrollChanged(enabled);
    }
}

void PythonWidgetState::setFontSize(int size) {
    int clamped = std::clamp(size, 8, 24);
    if (_data.font_size != clamped) {
        _data.font_size = clamped;
        markDirty();
        emit fontSizeChanged(clamped);
    }
}

// === New Setters (Phase 4) ===

std::vector<QString> PythonWidgetState::commandHistory() const {
    std::vector<QString> result;
    result.reserve(_data.command_history.size());
    for (auto const & cmd : _data.command_history) {
        result.push_back(QString::fromStdString(cmd));
    }
    return result;
}

void PythonWidgetState::setCommandHistory(std::vector<QString> const & history) {
    _data.command_history.clear();
    _data.command_history.reserve(history.size());

    // Keep last 500 entries max
    auto const start = history.size() > 500 ? history.end() - 500 : history.begin();
    for (auto it = start; it != history.end(); ++it) {
        _data.command_history.push_back(it->toStdString());
    }
    markDirty();
}

std::vector<QString> PythonWidgetState::recentScripts() const {
    std::vector<QString> result;
    result.reserve(_data.recent_scripts.size());
    for (auto const & path : _data.recent_scripts) {
        result.push_back(QString::fromStdString(path));
    }
    return result;
}

void PythonWidgetState::addRecentScript(QString const & path) {
    std::string const path_std = path.toStdString();

    // Remove if already present (move to front)
    auto & scripts = _data.recent_scripts;
    scripts.erase(
        std::remove(scripts.begin(), scripts.end(), path_std),
        scripts.end());

    // Insert at front, keep max 10
    scripts.insert(scripts.begin(), path_std);
    if (scripts.size() > 10) {
        scripts.resize(10);
    }
    markDirty();
}

void PythonWidgetState::setShowLineNumbers(bool show) {
    if (_data.show_line_numbers != show) {
        _data.show_line_numbers = show;
        markDirty();
        emit showLineNumbersChanged(show);
    }
}

void PythonWidgetState::setEditorContent(QString const & content) {
    _data.editor_content = content.toStdString();
    // Don't markDirty for editor content changes — too frequent
}

// === Phase 5 Setters ===

void PythonWidgetState::setScriptArguments(QString const & args) {
    std::string const args_std = args.toStdString();
    if (_data.script_arguments != args_std) {
        _data.script_arguments = args_std;
        markDirty();
        emit scriptArgumentsChanged(args);
    }
}

void PythonWidgetState::setAutoImportPrelude(QString const & prelude) {
    std::string const prelude_std = prelude.toStdString();
    if (_data.auto_import_prelude != prelude_std) {
        _data.auto_import_prelude = prelude_std;
        markDirty();
        emit preludeChanged(prelude);
    }
}

void PythonWidgetState::setPreludeEnabled(bool enabled) {
    if (_data.prelude_enabled != enabled) {
        _data.prelude_enabled = enabled;
        markDirty();
        emit preludeEnabledChanged(enabled);
    }
}

void PythonWidgetState::setLastWorkingDirectory(QString const & dir) {
    std::string const dir_std = dir.toStdString();
    if (_data.last_working_directory != dir_std) {
        _data.last_working_directory = dir_std;
        markDirty();
        emit workingDirectoryChanged(dir);
    }
}

// === Phase 6 Setters ===

void PythonWidgetState::setVenvPath(QString const & path) {
    std::string const path_std = path.toStdString();
    if (_data.venv_path != path_std) {
        _data.venv_path = path_std;
        markDirty();
        emit venvPathChanged(path);
    }
}
