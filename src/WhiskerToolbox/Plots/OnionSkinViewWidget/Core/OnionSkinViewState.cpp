#include "OnionSkinViewState.hpp"

#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

OnionSkinViewState::OnionSkinViewState(QObject * parent)
    : EditorState(parent),
      _horizontal_axis_state(std::make_unique<HorizontalAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this))
{
    _data.instance_id = getInstanceId().toStdString();
    _data.horizontal_axis = _horizontal_axis_state->data();
    _data.vertical_axis = _vertical_axis_state->data();
    // Sync view state bounds from axes so they never drift
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

QString OnionSkinViewState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void OnionSkinViewState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

void OnionSkinViewState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void OnionSkinViewState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void OnionSkinViewState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void OnionSkinViewState::setXBounds(double x_min, double x_max)
{
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        _horizontal_axis_state->setRangeSilent(x_min, x_max);
        _data.horizontal_axis = _horizontal_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void OnionSkinViewState::setYBounds(double y_min, double y_max)
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

std::string OnionSkinViewState::toJson() const
{
    OnionSkinViewStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool OnionSkinViewState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<OnionSkinViewStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        _horizontal_axis_state->data() = _data.horizontal_axis;
        _vertical_axis_state->data() = _data.vertical_axis;
        // Sync view state bounds from axes so they never drift
        _data.view_state.x_min = _horizontal_axis_state->getXMin();
        _data.view_state.x_max = _horizontal_axis_state->getXMax();
        _data.view_state.y_min = _vertical_axis_state->getYMin();
        _data.view_state.y_max = _vertical_axis_state->getYMax();
        emit stateChanged();
        return true;
    }
    return false;
}
