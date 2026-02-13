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
        return true;
    }
    return false;
}

// === State Setters ===

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
