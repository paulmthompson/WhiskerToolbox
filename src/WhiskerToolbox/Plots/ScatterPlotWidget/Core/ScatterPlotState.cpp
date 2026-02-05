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

double ScatterPlotState::getXMin() const
{
    return _data.x_min;
}

void ScatterPlotState::setXMin(double x_min)
{
    if (_data.x_min != x_min) {
        _data.x_min = x_min;
        markDirty();
        emit xMinChanged(x_min);
        emit stateChanged();
    }
}

double ScatterPlotState::getXMax() const
{
    return _data.x_max;
}

void ScatterPlotState::setXMax(double x_max)
{
    if (_data.x_max != x_max) {
        _data.x_max = x_max;
        markDirty();
        emit xMaxChanged(x_max);
        emit stateChanged();
    }
}

double ScatterPlotState::getYMin() const
{
    return _data.y_min;
}

void ScatterPlotState::setYMin(double y_min)
{
    if (_data.y_min != y_min) {
        _data.y_min = y_min;
        markDirty();
        emit yMinChanged(y_min);
        emit stateChanged();
    }
}

double ScatterPlotState::getYMax() const
{
    return _data.y_max;
}

void ScatterPlotState::setYMax(double y_max)
{
    if (_data.y_max != y_max) {
        _data.y_max = y_max;
        markDirty();
        emit yMaxChanged(y_max);
        emit stateChanged();
    }
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
