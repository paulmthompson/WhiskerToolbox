#include "OnionSkinViewState.hpp"

#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

#include <algorithm>

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

// === Data Key Management — Point ===

std::vector<QString> OnionSkinViewState::getPointDataKeys() const
{
    std::vector<QString> result;
    result.reserve(_data.point_data_keys.size());
    for (auto const & key : _data.point_data_keys) {
        result.push_back(QString::fromStdString(key));
    }
    return result;
}

void OnionSkinViewState::addPointDataKey(QString const & key)
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

void OnionSkinViewState::removePointDataKey(QString const & key)
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

void OnionSkinViewState::clearPointDataKeys()
{
    if (!_data.point_data_keys.empty()) {
        _data.point_data_keys.clear();
        markDirty();
        emit pointDataKeysCleared();
        emit stateChanged();
    }
}

// === Data Key Management — Line ===

std::vector<QString> OnionSkinViewState::getLineDataKeys() const
{
    std::vector<QString> result;
    result.reserve(_data.line_data_keys.size());
    for (auto const & key : _data.line_data_keys) {
        result.push_back(QString::fromStdString(key));
    }
    return result;
}

void OnionSkinViewState::addLineDataKey(QString const & key)
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

void OnionSkinViewState::removeLineDataKey(QString const & key)
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

void OnionSkinViewState::clearLineDataKeys()
{
    if (!_data.line_data_keys.empty()) {
        _data.line_data_keys.clear();
        markDirty();
        emit lineDataKeysCleared();
        emit stateChanged();
    }
}

// === Data Key Management — Mask ===

std::vector<QString> OnionSkinViewState::getMaskDataKeys() const
{
    std::vector<QString> result;
    result.reserve(_data.mask_data_keys.size());
    for (auto const & key : _data.mask_data_keys) {
        result.push_back(QString::fromStdString(key));
    }
    return result;
}

void OnionSkinViewState::addMaskDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.mask_data_keys.begin(), _data.mask_data_keys.end(), key_str);
    if (it == _data.mask_data_keys.end()) {
        _data.mask_data_keys.push_back(key_str);
        markDirty();
        emit maskDataKeyAdded(key);
        emit stateChanged();
    }
}

void OnionSkinViewState::removeMaskDataKey(QString const & key)
{
    std::string const key_str = key.toStdString();
    auto it = std::find(_data.mask_data_keys.begin(), _data.mask_data_keys.end(), key_str);
    if (it != _data.mask_data_keys.end()) {
        _data.mask_data_keys.erase(it);
        markDirty();
        emit maskDataKeyRemoved(key);
        emit stateChanged();
    }
}

void OnionSkinViewState::clearMaskDataKeys()
{
    if (!_data.mask_data_keys.empty()) {
        _data.mask_data_keys.clear();
        markDirty();
        emit maskDataKeysCleared();
        emit stateChanged();
    }
}

// === Temporal Window Parameters ===

void OnionSkinViewState::setWindowBehind(int behind)
{
    behind = std::max(0, behind);
    if (_data.window_behind != behind) {
        _data.window_behind = behind;
        markDirty();
        emit windowBehindChanged(behind);
        emit stateChanged();
    }
}

void OnionSkinViewState::setWindowAhead(int ahead)
{
    ahead = std::max(0, ahead);
    if (_data.window_ahead != ahead) {
        _data.window_ahead = ahead;
        markDirty();
        emit windowAheadChanged(ahead);
        emit stateChanged();
    }
}

// === Alpha Curve Settings ===

void OnionSkinViewState::setAlphaCurve(QString const & curve)
{
    std::string const curve_str = curve.toStdString();
    if (_data.alpha_curve != curve_str) {
        _data.alpha_curve = curve_str;
        markDirty();
        emit alphaCurveChanged(curve);
        emit stateChanged();
    }
}

void OnionSkinViewState::setMinAlpha(float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (_data.min_alpha != alpha) {
        _data.min_alpha = alpha;
        markDirty();
        emit minAlphaChanged(alpha);
        emit stateChanged();
    }
}

void OnionSkinViewState::setMaxAlpha(float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (_data.max_alpha != alpha) {
        _data.max_alpha = alpha;
        markDirty();
        emit maxAlphaChanged(alpha);
        emit stateChanged();
    }
}

// === Rendering Parameters ===

void OnionSkinViewState::setPointSize(float size)
{
    if (_data.point_size != size) {
        _data.point_size = size;
        markDirty();
        emit pointSizeChanged(size);
        emit stateChanged();
    }
}

void OnionSkinViewState::setLineWidth(float width)
{
    if (_data.line_width != width) {
        _data.line_width = width;
        markDirty();
        emit lineWidthChanged(width);
        emit stateChanged();
    }
}

void OnionSkinViewState::setHighlightCurrent(bool highlight)
{
    if (_data.highlight_current != highlight) {
        _data.highlight_current = highlight;
        markDirty();
        emit highlightCurrentChanged(highlight);
        emit stateChanged();
    }
}

// === Serialization ===

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
