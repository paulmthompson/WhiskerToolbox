#include "GroupManagementWidgetState.hpp"

GroupManagementWidgetState::GroupManagementWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString GroupManagementWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void GroupManagementWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string GroupManagementWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    GroupManagementWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool GroupManagementWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<GroupManagementWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit selectedGroupChanged(_data.selected_group_id);
        return true;
    }
    return false;
}

void GroupManagementWidgetState::setSelectedGroupId(int group_id) {
    if (_data.selected_group_id != group_id) {
        _data.selected_group_id = group_id;
        markDirty();
        emit selectedGroupChanged(group_id);
    }
}

int GroupManagementWidgetState::selectedGroupId() const {
    return _data.selected_group_id;
}
