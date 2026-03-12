/**
 * @file DataSynthesizerState.cpp
 * @brief Implementation of the DataSynthesizerState EditorState subclass
 */

#include "DataSynthesizerState.hpp"

DataSynthesizerState::DataSynthesizerState(QObject * parent)
    : EditorState(parent) {
    _data.instance_id = getInstanceId().toStdString();
}

QString DataSynthesizerState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataSynthesizerState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string DataSynthesizerState::toJson() const {
    DataSynthesizerStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataSynthesizerState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataSynthesizerStateData>(json);
    if (result) {
        _data = *result;

        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        return true;
    }
    return false;
}
