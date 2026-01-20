#include "DataManagerWidgetState.hpp"

DataManagerWidgetState::DataManagerWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString DataManagerWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataManagerWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string DataManagerWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    DataManagerWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataManagerWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataManagerWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit selectedDataKeyChanged(QString::fromStdString(_data.selected_data_key));
        return true;
    }
    return false;
}

void DataManagerWidgetState::setSelectedDataKey(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.selected_data_key != key_std) {
        _data.selected_data_key = key_std;
        markDirty();
        emit selectedDataKeyChanged(key);
    }
}

QString DataManagerWidgetState::selectedDataKey() const {
    return QString::fromStdString(_data.selected_data_key);
}
