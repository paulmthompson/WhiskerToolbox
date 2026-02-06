#include "SpectrogramState.hpp"

#include <rfl/json.hpp>

SpectrogramState::SpectrogramState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString SpectrogramState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void SpectrogramState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// === View State ===

void SpectrogramState::setViewState(SpectrogramViewState const & view_state)
{
    _data.view_state = view_state;
    markDirty();
    emit viewStateChanged();
    emit stateChanged();
}

void SpectrogramState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
        // Note: No stateChanged() here - zoom is a view-only change that
        // doesn't require scene rebuild. The projection matrix handles it.
    }
}

void SpectrogramState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
        // Note: No stateChanged() here - zoom is a view-only change that
        // doesn't require scene rebuild. The projection matrix handles it.
    }
}

void SpectrogramState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
        // Note: No stateChanged() here - pan is a view-only change that
        // doesn't require scene rebuild. The projection matrix handles it.
    }
}

void SpectrogramState::setXBounds(double x_min, double x_max)
{
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void SpectrogramState::setAxisOptions(SpectrogramAxisOptions const & options)
{
    _data.axis_options = options;
    markDirty();
    emit axisOptionsChanged();
    emit stateChanged();
}

QString SpectrogramState::getBackgroundColor() const
{
    return QString::fromStdString(_data.background_color);
}

void SpectrogramState::setBackgroundColor(QString const & hex_color)
{
    std::string hex_str = hex_color.toStdString();
    if (_data.background_color != hex_str) {
        _data.background_color = hex_str;
        markDirty();
        emit backgroundColorChanged(hex_color);
        emit stateChanged();
    }
}

void SpectrogramState::setPinned(bool pinned)
{
    if (_data.pinned != pinned) {
        _data.pinned = pinned;
        markDirty();
        emit pinnedChanged(pinned);
        emit stateChanged();
    }
}

QString SpectrogramState::getAnalogSignalKey() const
{
    return QString::fromStdString(_data.analog_signal_key);
}

void SpectrogramState::setAnalogSignalKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.analog_signal_key != key_str) {
        _data.analog_signal_key = key_str;
        markDirty();
        emit analogSignalKeyChanged(key);
        emit stateChanged();
    }
}

std::string SpectrogramState::toJson() const
{
    // Include instance_id in serialization for restoration
    SpectrogramStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool SpectrogramState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<SpectrogramStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Emit all signals to ensure UI updates
        emit viewStateChanged();
        emit axisOptionsChanged();
        emit pinnedChanged(_data.pinned);
        emit analogSignalKeyChanged(QString::fromStdString(_data.analog_signal_key));
        emit stateChanged();
        return true;
    }
    return false;
}
