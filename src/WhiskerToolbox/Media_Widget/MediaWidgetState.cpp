#include "MediaWidgetState.hpp"

MediaWidgetState::MediaWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString MediaWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void MediaWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string MediaWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    MediaWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool MediaWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MediaWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit displayedDataKeyChanged(QString::fromStdString(_data.displayed_data_key));
        return true;
    }
    return false;
}

void MediaWidgetState::setDisplayedDataKey(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.displayed_data_key != key_std) {
        _data.displayed_data_key = key_std;
        markDirty();
        emit displayedDataKeyChanged(key);
    }
}

QString MediaWidgetState::displayedDataKey() const {
    return QString::fromStdString(_data.displayed_data_key);
}
