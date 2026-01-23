#include "TerminalWidgetState.hpp"

TerminalWidgetState::TerminalWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString TerminalWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TerminalWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TerminalWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    TerminalWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TerminalWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<TerminalWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Emit all signals to update any connected views
        emit stateChanged();
        emit autoScrollChanged(_data.auto_scroll);
        emit showTimestampsChanged(_data.show_timestamps);
        emit wordWrapChanged(_data.word_wrap);
        emit maxLinesChanged(_data.max_lines);
        emit fontSizeChanged(_data.font_size);
        emit fontFamilyChanged(QString::fromStdString(_data.font_family));
        emit backgroundColorChanged(QString::fromStdString(_data.background_color));
        emit textColorChanged(QString::fromStdString(_data.text_color));
        emit errorColorChanged(QString::fromStdString(_data.error_color));
        emit systemColorChanged(QString::fromStdString(_data.system_color));
        
        return true;
    }
    return false;
}

// === Display Preferences ===

void TerminalWidgetState::setAutoScroll(bool enabled) {
    if (_data.auto_scroll != enabled) {
        _data.auto_scroll = enabled;
        markDirty();
        emit autoScrollChanged(enabled);
    }
}

void TerminalWidgetState::setShowTimestamps(bool show) {
    if (_data.show_timestamps != show) {
        _data.show_timestamps = show;
        markDirty();
        emit showTimestampsChanged(show);
    }
}

void TerminalWidgetState::setWordWrap(bool enabled) {
    if (_data.word_wrap != enabled) {
        _data.word_wrap = enabled;
        markDirty();
        emit wordWrapChanged(enabled);
    }
}

// === Buffer Settings ===

void TerminalWidgetState::setMaxLines(int lines) {
    if (_data.max_lines != lines) {
        _data.max_lines = lines;
        markDirty();
        emit maxLinesChanged(lines);
    }
}

// === Visual Settings ===

void TerminalWidgetState::setFontSize(int size) {
    // Clamp to valid range
    int clamped = std::clamp(size, 8, 24);
    if (_data.font_size != clamped) {
        _data.font_size = clamped;
        markDirty();
        emit fontSizeChanged(clamped);
    }
}

void TerminalWidgetState::setFontFamily(QString const & family) {
    std::string const family_std = family.toStdString();
    if (_data.font_family != family_std) {
        _data.font_family = family_std;
        markDirty();
        emit fontFamilyChanged(family);
    }
}

// === Color Settings ===

void TerminalWidgetState::setBackgroundColor(QString const & color) {
    std::string const color_std = color.toStdString();
    if (_data.background_color != color_std) {
        _data.background_color = color_std;
        markDirty();
        emit backgroundColorChanged(color);
    }
}

void TerminalWidgetState::setTextColor(QString const & color) {
    std::string const color_std = color.toStdString();
    if (_data.text_color != color_std) {
        _data.text_color = color_std;
        markDirty();
        emit textColorChanged(color);
    }
}

void TerminalWidgetState::setErrorColor(QString const & color) {
    std::string const color_std = color.toStdString();
    if (_data.error_color != color_std) {
        _data.error_color = color_std;
        markDirty();
        emit errorColorChanged(color);
    }
}

void TerminalWidgetState::setSystemColor(QString const & color) {
    std::string const color_std = color.toStdString();
    if (_data.system_color != color_std) {
        _data.system_color = color_std;
        markDirty();
        emit systemColorChanged(color);
    }
}
