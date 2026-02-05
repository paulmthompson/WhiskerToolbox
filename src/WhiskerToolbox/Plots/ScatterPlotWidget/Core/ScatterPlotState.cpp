#include "ScatterPlotState.hpp"

#include <rfl/json.hpp>

ScatterPlotState::ScatterPlotState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString ScatterPlotState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void ScatterPlotState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string ScatterPlotState::toJson() const
{
    // Include instance_id in serialization for restoration
    ScatterPlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool ScatterPlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<ScatterPlotStateData>(json);
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
