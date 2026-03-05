/**
 * @file TriageSessionState.cpp
 * @brief Implementation of TriageSessionState
 */

#include "TriageSessionState.hpp"

#include <rfl/json.hpp>

#include <iostream>

TriageSessionState::TriageSessionState(std::shared_ptr<DataManager> data_manager,
                                       QObject * parent)
    : EditorState(parent),
      _data_manager(std::move(data_manager)) {
}

QString TriageSessionState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TriageSessionState::setDisplayName(QString const & name) {
    auto const name_std = name.toStdString();
    if (_data.display_name != name_std) {
        _data.display_name = name_std;
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TriageSessionState::toJson() const {
    TriageSessionStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TriageSessionState::fromJson(std::string const & json) {
    auto result = rfl::json::read<TriageSessionStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        emit stateChanged();
        return true;
    }
    std::cerr << "TriageSessionState::fromJson: Failed to parse JSON" << std::endl;
    return false;
}

void TriageSessionState::setPipelineJson(std::string const & json) {
    if (_data.pipeline_json != json) {
        _data.pipeline_json = json;
        markDirty();
        emit pipelineChanged();
    }
}

void TriageSessionState::setTrackedRegionsKey(std::string const & key) {
    if (_data.tracked_regions_key != key) {
        _data.tracked_regions_key = key;
        markDirty();
        emit stateChanged();
    }
}
