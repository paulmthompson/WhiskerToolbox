#include "DataInspectorState.hpp"

#include <algorithm>

DataInspectorState::DataInspectorState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString DataInspectorState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataInspectorState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string DataInspectorState::toJson() const {
    // Include instance_id in serialization for restoration
    DataInspectorStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataInspectorState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataInspectorStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit inspectedDataKeyChanged(QString::fromStdString(_data.inspected_data_key));
        emit pinnedChanged(_data.is_pinned);
        return true;
    }
    return false;
}

void DataInspectorState::setInspectedDataKey(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.inspected_data_key != key_std) {
        _data.inspected_data_key = key_std;
        markDirty();
        emit inspectedDataKeyChanged(key);
    }
}

QString DataInspectorState::inspectedDataKey() const {
    return QString::fromStdString(_data.inspected_data_key);
}

void DataInspectorState::setPinned(bool pinned) {
    if (_data.is_pinned != pinned) {
        _data.is_pinned = pinned;
        markDirty();
        emit pinnedChanged(pinned);
    }
}

bool DataInspectorState::isPinned() const {
    return _data.is_pinned;
}

void DataInspectorState::setSectionCollapsed(QString const & section_id, bool collapsed) {
    std::string const section_std = section_id.toStdString();
    
    auto it = std::find(_data.collapsed_sections.begin(), 
                        _data.collapsed_sections.end(), 
                        section_std);
    bool const currently_collapsed = (it != _data.collapsed_sections.end());
    
    if (collapsed && !currently_collapsed) {
        _data.collapsed_sections.push_back(section_std);
        markDirty();
        emit sectionCollapsedChanged(section_id, collapsed);
    } else if (!collapsed && currently_collapsed) {
        _data.collapsed_sections.erase(it);
        markDirty();
        emit sectionCollapsedChanged(section_id, collapsed);
    }
}

bool DataInspectorState::isSectionCollapsed(QString const & section_id) const {
    std::string const section_std = section_id.toStdString();
    return std::find(_data.collapsed_sections.begin(),
                     _data.collapsed_sections.end(),
                     section_std) != _data.collapsed_sections.end();
}

void DataInspectorState::setTypeSpecificState(nlohmann::json const & json) {
    std::string const json_str = json.dump();
    if (_data.ui_state_json != json_str) {
        _data.ui_state_json = json_str;
        markDirty();
    }
}

nlohmann::json DataInspectorState::getTypeSpecificState() const {
    try {
        return nlohmann::json::parse(_data.ui_state_json);
    } catch (...) {
        return nlohmann::json::object();
    }
}
