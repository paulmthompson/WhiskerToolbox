#include "DataTransformWidgetState.hpp"

DataTransformWidgetState::DataTransformWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString DataTransformWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataTransformWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string DataTransformWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    DataTransformWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataTransformWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataTransformWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit selectedInputDataKeyChanged(QString::fromStdString(_data.selected_input_data_key));
        emit selectedOperationChanged(QString::fromStdString(_data.selected_operation));
        emit lastOutputNameChanged(QString::fromStdString(_data.last_output_name));
        return true;
    }
    return false;
}

void DataTransformWidgetState::setSelectedInputDataKey(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.selected_input_data_key != key_std) {
        _data.selected_input_data_key = key_std;
        markDirty();
        emit selectedInputDataKeyChanged(key);
    }
}

QString DataTransformWidgetState::selectedInputDataKey() const {
    return QString::fromStdString(_data.selected_input_data_key);
}

void DataTransformWidgetState::setSelectedOperation(QString const & operation) {
    std::string const op_std = operation.toStdString();
    if (_data.selected_operation != op_std) {
        _data.selected_operation = op_std;
        markDirty();
        emit selectedOperationChanged(operation);
    }
}

QString DataTransformWidgetState::selectedOperation() const {
    return QString::fromStdString(_data.selected_operation);
}

void DataTransformWidgetState::setLastOutputName(QString const & name) {
    std::string const name_std = name.toStdString();
    if (_data.last_output_name != name_std) {
        _data.last_output_name = name_std;
        markDirty();
        emit lastOutputNameChanged(name);
    }
}

QString DataTransformWidgetState::lastOutputName() const {
    return QString::fromStdString(_data.last_output_name);
}
