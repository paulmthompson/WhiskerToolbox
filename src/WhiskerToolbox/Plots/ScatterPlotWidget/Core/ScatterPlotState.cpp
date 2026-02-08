#include "ScatterPlotState.hpp"

#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

ScatterPlotState::ScatterPlotState(QObject * parent)
    : EditorState(parent),
      _horizontal_axis_state(std::make_unique<HorizontalAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this))
{
    _data.instance_id = getInstanceId().toStdString();
    _data.horizontal_axis = _horizontal_axis_state->data();
    _data.vertical_axis = _vertical_axis_state->data();

    // Sync view state bounds from axis states so view state and axes never drift
    _data.view_state.x_min = _horizontal_axis_state->getXMin();
    _data.view_state.x_max = _horizontal_axis_state->getXMax();
    _data.view_state.y_min = _vertical_axis_state->getYMin();
    _data.view_state.y_max = _vertical_axis_state->getYMax();

    auto syncHorizontalData = [this]() {
        _data.horizontal_axis = _horizontal_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeChanged,
            this, syncHorizontalData);
    connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeUpdated,
            this, syncHorizontalData);

    auto syncVerticalData = [this]() {
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
            this, syncVerticalData);
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeUpdated,
            this, syncVerticalData);
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

double ScatterPlotState::getXMin() const
{
    return _horizontal_axis_state->getXMin();
}

double ScatterPlotState::getXMax() const
{
    return _horizontal_axis_state->getXMax();
}

double ScatterPlotState::getYMin() const
{
    return _vertical_axis_state->getYMin();
}

double ScatterPlotState::getYMax() const
{
    return _vertical_axis_state->getYMax();
}

void ScatterPlotState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setXBounds(double x_min, double x_max)
{
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        _horizontal_axis_state->data().x_min = x_min;
        _horizontal_axis_state->data().x_max = x_max;
        _data.horizontal_axis = _horizontal_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void ScatterPlotState::setYBounds(double y_min, double y_max)
{
    if (_data.view_state.y_min != y_min || _data.view_state.y_max != y_max) {
        _data.view_state.y_min = y_min;
        _data.view_state.y_max = y_max;
        _vertical_axis_state->data().y_min = y_min;
        _vertical_axis_state->data().y_max = y_max;
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

std::string ScatterPlotState::toJson() const
{
    ScatterPlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool ScatterPlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<ScatterPlotStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        _horizontal_axis_state->data() = _data.horizontal_axis;
        _vertical_axis_state->data() = _data.vertical_axis;

        // Sync view state bounds from axis states so they never drift
        _data.view_state.x_min = _horizontal_axis_state->getXMin();
        _data.view_state.x_max = _horizontal_axis_state->getXMax();
        _data.view_state.y_min = _vertical_axis_state->getYMin();
        _data.view_state.y_max = _vertical_axis_state->getYMax();

        emit stateChanged();
        return true;
    }
    return false;
}
