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

        emit outputTypeChanged(_data.output_type);
        emit generatorNameChanged(_data.generator_name);
        emit parameterJsonChanged(_data.parameter_json);
        emit outputKeyChanged(_data.output_key);
        emit stateChanged();
        return true;
    }
    return false;
}

// --- Generator selection ---

std::string const & DataSynthesizerState::outputType() const {
    return _data.output_type;
}

void DataSynthesizerState::setOutputType(std::string const & type) {
    if (_data.output_type != type) {
        _data.output_type = type;
        markDirty();
        emit outputTypeChanged(_data.output_type);
    }
}

std::string const & DataSynthesizerState::generatorName() const {
    return _data.generator_name;
}

void DataSynthesizerState::setGeneratorName(std::string const & name) {
    if (_data.generator_name != name) {
        _data.generator_name = name;
        markDirty();
        emit generatorNameChanged(_data.generator_name);
    }
}

std::string const & DataSynthesizerState::parameterJson() const {
    return _data.parameter_json;
}

void DataSynthesizerState::setParameterJson(std::string const & json) {
    if (_data.parameter_json != json) {
        _data.parameter_json = json;
        markDirty();
        emit parameterJsonChanged(_data.parameter_json);
    }
}

// --- Output configuration ---

std::string const & DataSynthesizerState::outputKey() const {
    return _data.output_key;
}

void DataSynthesizerState::setOutputKey(std::string const & key) {
    if (_data.output_key != key) {
        _data.output_key = key;
        markDirty();
        emit outputKeyChanged(_data.output_key);
    }
}

std::string const & DataSynthesizerState::timeKey() const {
    return _data.time_key;
}

void DataSynthesizerState::setTimeKey(std::string const & key) {
    if (_data.time_key != key) {
        _data.time_key = key;
        markDirty();
    }
}

std::string const & DataSynthesizerState::timeFrameMode() const {
    return _data.time_frame_mode;
}

void DataSynthesizerState::setTimeFrameMode(std::string const & mode) {
    if (_data.time_frame_mode != mode) {
        _data.time_frame_mode = mode;
        markDirty();
        emit timeFrameModeChanged(_data.time_frame_mode);
    }
}
