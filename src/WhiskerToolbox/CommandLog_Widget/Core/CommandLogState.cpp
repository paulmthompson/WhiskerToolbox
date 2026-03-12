/**
 * @file CommandLogState.cpp
 * @brief Implementation of the CommandLogState EditorState subclass
 */

#include "CommandLogState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

CommandLogState::CommandLogState(QObject * parent)
    : EditorState(parent) {
    _data.instance_id = getInstanceId().toStdString();
}

QString CommandLogState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void CommandLogState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string CommandLogState::toJson() const {
    CommandLogStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool CommandLogState::fromJson(std::string const & json) {
    auto result = rfl::json::read<CommandLogStateData>(json);
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
