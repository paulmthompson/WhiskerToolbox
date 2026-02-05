#include "TemporalProjectionViewState.hpp"

#include <rfl/json.hpp>

TemporalProjectionViewState::TemporalProjectionViewState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString TemporalProjectionViewState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void TemporalProjectionViewState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TemporalProjectionViewState::toJson() const
{
    // Include instance_id in serialization for restoration
    TemporalProjectionViewStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TemporalProjectionViewState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<TemporalProjectionViewStateData>(json);
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
