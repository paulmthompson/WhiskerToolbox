#include "LinePlotState.hpp"

#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

LinePlotState::LinePlotState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this)),
      _relative_time_axis_state(std::make_unique<RelativeTimeAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this))
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Sync initial alignment data from member state
    _data.alignment = _alignment_state->data();

    // Initialize time axis range from window size (centered at 0)
    double const window_size = _data.alignment.window_size;
    double const half_window = window_size / 2.0;
    _relative_time_axis_state->setRangeSilent(-half_window, half_window);
    _data.time_axis = _relative_time_axis_state->data();

    // Initialize view state bounds from window size
    _data.view_state.x_min = -half_window;
    _data.view_state.x_max = half_window;

    // Sync initial vertical axis data from member state
    _data.vertical_axis = _vertical_axis_state->data();

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &LinePlotState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &LinePlotState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &LinePlotState::offsetChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, [this](double new_window_size) {
                double const half = new_window_size / 2.0;
                _data.view_state.x_min = -half;
                _data.view_state.x_max = half;
                _data.view_state.x_zoom = 1.0;
                _data.view_state.x_pan = 0.0;
                _relative_time_axis_state->setRangeSilent(-half, half);
                _data.time_axis = _relative_time_axis_state->data();
                markDirty();
                emit windowSizeChanged(new_window_size);
                emit viewStateChanged();
                emit stateChanged();
            });

    // Sync relative time axis state to data when range changes
    auto syncTimeAxisData = [this]() {
        _data.time_axis = _relative_time_axis_state->data();
        double const range = _data.time_axis.max_range - _data.time_axis.min_range;
        if (std::abs(range - _data.alignment.window_size) > 0.01) {
            _data.alignment.window_size = range;
            _alignment_state->data().window_size = range;
        }
        markDirty();
        emit stateChanged();
    };
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeChanged,
            this, syncTimeAxisData);
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeUpdated,
            this, syncTimeAxisData);

    // Sync vertical axis state to data
    auto syncVerticalAxisData = [this]() {
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
            this, syncVerticalAxisData);
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeUpdated,
            this, syncVerticalAxisData);
}

QString LinePlotState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void LinePlotState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString LinePlotState::getAlignmentEventKey() const
{
    return _alignment_state->getAlignmentEventKey();
}

void LinePlotState::setAlignmentEventKey(QString const & key)
{
    _alignment_state->setAlignmentEventKey(key);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

IntervalAlignmentType LinePlotState::getIntervalAlignmentType() const
{
    return _alignment_state->getIntervalAlignmentType();
}

void LinePlotState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    _alignment_state->setIntervalAlignmentType(type);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double LinePlotState::getOffset() const
{
    return _alignment_state->getOffset();
}

void LinePlotState::setOffset(double offset)
{
    _alignment_state->setOffset(offset);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double LinePlotState::getWindowSize() const
{
    return _alignment_state->getWindowSize();
}

void LinePlotState::setWindowSize(double window_size)
{
    _alignment_state->setWindowSize(window_size);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

void LinePlotState::addPlotSeries(QString const & series_name, QString const & series_key)
{
    std::string name_str = series_name.toStdString();
    std::string key_str = series_key.toStdString();

    LinePlotOptions options;
    options.series_key = key_str;

    _data.plot_series[name_str] = options;
    markDirty();
    emit plotSeriesAdded(series_name);
    emit stateChanged();
}

void LinePlotState::removePlotSeries(QString const & series_name)
{
    std::string name_str = series_name.toStdString();
    auto it = _data.plot_series.find(name_str);
    if (it != _data.plot_series.end()) {
        _data.plot_series.erase(it);
        markDirty();
        emit plotSeriesRemoved(series_name);
        emit stateChanged();
    }
}

std::vector<QString> LinePlotState::getPlotSeriesNames() const
{
    std::vector<QString> names;
    names.reserve(_data.plot_series.size());
    for (auto const & [name, _] : _data.plot_series) {
        names.push_back(QString::fromStdString(name));
    }
    return names;
}

std::optional<LinePlotOptions> LinePlotState::getPlotSeriesOptions(QString const & series_name) const
{
    std::string name_str = series_name.toStdString();
    auto it = _data.plot_series.find(name_str);
    if (it != _data.plot_series.end()) {
        return it->second;
    }
    return std::nullopt;
}

void LinePlotState::updatePlotSeriesOptions(QString const & series_name, LinePlotOptions const & options)
{
    std::string name_str = series_name.toStdString();
    auto it = _data.plot_series.find(name_str);
    if (it != _data.plot_series.end()) {
        it->second = options;
        markDirty();
        emit plotSeriesOptionsChanged(series_name);
        emit stateChanged();
    }
}

// === View State (Zoom / Pan / Bounds) ===

void LinePlotState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void LinePlotState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void LinePlotState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void LinePlotState::setXBounds(double x_min, double x_max)
{
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

std::string LinePlotState::toJson() const
{
    // Include instance_id in serialization for restoration
    LinePlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool LinePlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<LinePlotStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Restore alignment state from serialized data
        _alignment_state->data() = _data.alignment;

        // Restore relative time axis and vertical axis state from serialized data
        _relative_time_axis_state->data() = _data.time_axis;
        _vertical_axis_state->data() = _data.vertical_axis;

        emit stateChanged();
        return true;
    }
    return false;
}
