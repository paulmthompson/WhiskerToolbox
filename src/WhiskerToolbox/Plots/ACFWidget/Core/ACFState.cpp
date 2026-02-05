#include "ACFState.hpp"

#include <rfl/json.hpp>

ACFState::ACFState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString ACFState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void ACFState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString ACFState::getEventKey() const
{
    return QString::fromStdString(_data.event_key);
}

void ACFState::setEventKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.event_key != key_str) {
        _data.event_key = key_str;
        markDirty();
        emit eventKeyChanged(key);
        emit stateChanged();
    }
}

std::string ACFState::toJson() const
{
    // Include instance_id in serialization for restoration
    ACFStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool ACFState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<ACFStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        return true;
    }
    return false;
}
