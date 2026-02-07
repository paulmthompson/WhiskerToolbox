#include "HeatmapState.hpp"

#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"

#include <rfl/json.hpp>

HeatmapState::HeatmapState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this)),
      _relative_time_axis_state(std::make_unique<RelativeTimeAxisState>(this))
{
    _data.instance_id = getInstanceId().toStdString();
    _data.alignment = _alignment_state->data();

    _relative_time_axis_state->setRangeSilent(_data.view_state.x_min, _data.view_state.x_max);
    _data.time_axis = _relative_time_axis_state->data();

    // Forward alignment state signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &HeatmapState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &HeatmapState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &HeatmapState::offsetChanged);

    // Window size -> view state bounds auto-sync
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, [this](double window_size) {
                _data.alignment = _alignment_state->data();
                double half_window = window_size / 2.0;
                _data.view_state.x_min = -half_window;
                _data.view_state.x_max = half_window;
                _data.view_state.x_pan = 0.0;
                _data.view_state.x_zoom = 1.0;
                _relative_time_axis_state->setRangeSilent(_data.view_state.x_min, _data.view_state.x_max);
                _data.time_axis = _relative_time_axis_state->data();
                markDirty();
                emit windowSizeChanged(window_size);
                emit viewStateChanged();
                emit stateChanged();
            });

    // Time axis range changed from user input -> update view bounds
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeChanged,
            this, [this]() {
                _data.time_axis = _relative_time_axis_state->data();
                _data.view_state.x_min = _data.time_axis.min_range;
                _data.view_state.x_max = _data.time_axis.max_range;
                markDirty();
                emit viewStateChanged();
                emit stateChanged();
            });
    // Time axis range updated programmatically -> only sync data
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeUpdated,
            this, [this]() {
                _data.time_axis = _relative_time_axis_state->data();
                markDirty();
                emit stateChanged();
            });
}

QString HeatmapState::getDisplayName() const { return QString::fromStdString(_data.display_name); }

void HeatmapState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString HeatmapState::getAlignmentEventKey() const { return _alignment_state->getAlignmentEventKey(); }
void HeatmapState::setAlignmentEventKey(QString const & key) {
    _alignment_state->setAlignmentEventKey(key);
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

IntervalAlignmentType HeatmapState::getIntervalAlignmentType() const { return _alignment_state->getIntervalAlignmentType(); }
void HeatmapState::setIntervalAlignmentType(IntervalAlignmentType type) {
    _alignment_state->setIntervalAlignmentType(type);
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double HeatmapState::getOffset() const { return _alignment_state->getOffset(); }
void HeatmapState::setOffset(double offset) {
    _alignment_state->setOffset(offset);
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double HeatmapState::getWindowSize() const { return _alignment_state->getWindowSize(); }
void HeatmapState::setWindowSize(double window_size) {
    _alignment_state->setWindowSize(window_size);
}

// === View State ===

void HeatmapState::setViewState(HeatmapViewState const & view_state) {
    _data.view_state = view_state;
    _relative_time_axis_state->setRangeSilent(view_state.x_min, view_state.x_max);
    _data.time_axis = _relative_time_axis_state->data();
    markDirty();
    emit viewStateChanged();
    emit stateChanged();
}

void HeatmapState::setXZoom(double zoom) {
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void HeatmapState::setYZoom(double zoom) {
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void HeatmapState::setPan(double x_pan, double y_pan) {
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void HeatmapState::setXBounds(double x_min, double x_max) {
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        _relative_time_axis_state->setRangeSilent(x_min, x_max);
        _data.time_axis = _relative_time_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

// === Background Color ===

QString HeatmapState::getBackgroundColor() const { return QString::fromStdString(_data.background_color); }
void HeatmapState::setBackgroundColor(QString const & hex_color) {
    std::string hex_str = hex_color.toStdString();
    if (_data.background_color != hex_str) {
        _data.background_color = hex_str;
        markDirty();
        emit backgroundColorChanged(hex_color);
        emit stateChanged();
    }
}

// === Serialization ===

std::string HeatmapState::toJson() const {
    HeatmapStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool HeatmapState::fromJson(std::string const & json) {
    auto result = rfl::json::read<HeatmapStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        _alignment_state->data() = _data.alignment;
        _relative_time_axis_state->data() = _data.time_axis;
        emit viewStateChanged();
        emit stateChanged();
        return true;
    }
    return false;
}
