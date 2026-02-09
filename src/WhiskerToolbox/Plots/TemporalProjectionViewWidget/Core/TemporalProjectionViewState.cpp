#include "TemporalProjectionViewState.hpp"

#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

#include <algorithm>

TemporalProjectionViewState::TemporalProjectionViewState(QObject * parent)
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

void TemporalProjectionViewState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void TemporalProjectionViewState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void TemporalProjectionViewState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void TemporalProjectionViewState::setXBounds(double x_min, double x_max)
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

void TemporalProjectionViewState::setYBounds(double y_min, double y_max)
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

std::string TemporalProjectionViewState::toJson() const
{
    TemporalProjectionViewStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TemporalProjectionViewState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<TemporalProjectionViewStateData>(json);
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

// === Data Key Management ===

std::vector<QString> TemporalProjectionViewState::getPointDataKeys() const
{
    std::vector<QString> result;
    result.reserve(_data.point_data_keys.size());
    for (auto const & key : _data.point_data_keys) {
        result.push_back(QString::fromStdString(key));
    }
    return result;
}

void TemporalProjectionViewState::addPointDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.point_data_keys.begin(), _data.point_data_keys.end(), key_str);
    if (it == _data.point_data_keys.end()) {
        _data.point_data_keys.push_back(key_str);
        markDirty();
        emit pointDataKeyAdded(key);
        emit stateChanged();
    }
}

void TemporalProjectionViewState::removePointDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.point_data_keys.begin(), _data.point_data_keys.end(), key_str);
    if (it != _data.point_data_keys.end()) {
        _data.point_data_keys.erase(it);
        markDirty();
        emit pointDataKeyRemoved(key);
        emit stateChanged();
    }
}

void TemporalProjectionViewState::clearPointDataKeys()
{
    if (!_data.point_data_keys.empty()) {
        _data.point_data_keys.clear();
        markDirty();
        emit pointDataKeysCleared();
        emit stateChanged();
    }
}

std::vector<QString> TemporalProjectionViewState::getLineDataKeys() const
{
    std::vector<QString> result;
    result.reserve(_data.line_data_keys.size());
    for (auto const & key : _data.line_data_keys) {
        result.push_back(QString::fromStdString(key));
    }
    return result;
}

void TemporalProjectionViewState::addLineDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.line_data_keys.begin(), _data.line_data_keys.end(), key_str);
    if (it == _data.line_data_keys.end()) {
        _data.line_data_keys.push_back(key_str);
        markDirty();
        emit lineDataKeyAdded(key);
        emit stateChanged();
    }
}

void TemporalProjectionViewState::removeLineDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.line_data_keys.begin(), _data.line_data_keys.end(), key_str);
    if (it != _data.line_data_keys.end()) {
        _data.line_data_keys.erase(it);
        markDirty();
        emit lineDataKeyRemoved(key);
        emit stateChanged();
    }
}

void TemporalProjectionViewState::clearLineDataKeys()
{
    if (!_data.line_data_keys.empty()) {
        _data.line_data_keys.clear();
        markDirty();
        emit lineDataKeysCleared();
        emit stateChanged();
    }
}

// === Rendering Parameters ===

void TemporalProjectionViewState::setPointSize(float size)
{
    if (_data.point_size != size) {
        _data.point_size = size;
        markDirty();
        emit pointSizeChanged(size);
        emit stateChanged();
    }
}

void TemporalProjectionViewState::setLineWidth(float width)
{
    if (_data.line_width != width) {
        _data.line_width = width;
        markDirty();
        emit lineWidthChanged(width);
        emit stateChanged();
    }
}

// === Selection Mode ===

void TemporalProjectionViewState::setSelectionMode(QString const & mode)
{
    std::string const mode_str = mode.toStdString();
    if (_data.selection_mode != mode_str) {
        _data.selection_mode = mode_str;
        markDirty();
        emit selectionModeChanged(mode);
        emit stateChanged();
    }
}
