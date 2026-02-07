#include "ACFState.hpp"

#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

ACFState::ACFState(QObject * parent)
    : EditorState(parent),
      _horizontal_axis_state(std::make_unique<HorizontalAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this))
{
    _data.instance_id = getInstanceId().toStdString();
    _data.horizontal_axis = _horizontal_axis_state->data();
    _data.vertical_axis = _vertical_axis_state->data();

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

void ACFState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ACFState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ACFState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
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

        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        _horizontal_axis_state->data() = _data.horizontal_axis;
        _vertical_axis_state->data() = _data.vertical_axis;

        emit stateChanged();
        return true;
    }
    return false;
}
