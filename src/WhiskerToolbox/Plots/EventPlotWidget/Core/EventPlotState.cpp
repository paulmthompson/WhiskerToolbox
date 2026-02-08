#include "EventPlotState.hpp"

#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"

#include <rfl/json.hpp>

EventPlotState::EventPlotState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this)),
      _relative_time_axis_state(std::make_unique<RelativeTimeAxisState>(this))
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Sync initial alignment data from member state
    _data.alignment = _alignment_state->data();

    // Initialize time axis range from view state bounds
    _relative_time_axis_state->setRangeSilent(_data.view_state.x_min, _data.view_state.x_max);
    _data.time_axis = _relative_time_axis_state->data();

    // EventPlot Y-axis is fixed trial viewport [-1, 1]; keep view_state in sync
    _data.view_state.y_min = -1.0;
    _data.view_state.y_max = 1.0;

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &EventPlotState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &EventPlotState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &EventPlotState::offsetChanged);
    
    // When window size changes via PlotAlignmentState (from PlotAlignmentWidget),
    // update our view state bounds and emit viewStateChanged
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, [this](double window_size) {
                // Sync to data for serialization
                _data.alignment = _alignment_state->data();
                
                // Auto-sync view bounds to match window size (centered on alignment point)
                double half_window = window_size / 2.0;
                _data.view_state.x_min = -half_window;
                _data.view_state.x_max = half_window;
                // Reset pan/zoom when window changes to avoid confusion
                _data.view_state.x_pan = 0.0;
                _data.view_state.x_zoom = 1.0;
                
                // Update time axis range
                _relative_time_axis_state->setRangeSilent(_data.view_state.x_min, _data.view_state.x_max);
                _data.time_axis = _relative_time_axis_state->data();
                
                markDirty();
                emit windowSizeChanged(window_size);
                emit viewStateChanged();
                emit stateChanged();
            });

    // Forward relative time axis state signals
    // When range changes from user input (rangeChanged), update view state bounds
    // When range is updated programmatically (rangeUpdated), only sync data for serialization
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeChanged,
            this, [this]() {
                // Sync to data for serialization
                _data.time_axis = _relative_time_axis_state->data();
                // Update view state bounds to match time axis range (user changed it via spinboxes)
                _data.view_state.x_min = _data.time_axis.min_range;
                _data.view_state.x_max = _data.time_axis.max_range;
                markDirty();
                emit viewStateChanged();
                emit stateChanged();
            });
    connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeUpdated,
            this, [this]() {
                // Only sync to data for serialization, don't update view bounds
                // This is called when range is updated programmatically (e.g., from pan/zoom)
                _data.time_axis = _relative_time_axis_state->data();
                markDirty();
                emit stateChanged();
            });
}

QString EventPlotState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void EventPlotState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString EventPlotState::getAlignmentEventKey() const
{
    return _alignment_state->getAlignmentEventKey();
}

void EventPlotState::setAlignmentEventKey(QString const & key)
{
    _alignment_state->setAlignmentEventKey(key);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

IntervalAlignmentType EventPlotState::getIntervalAlignmentType() const
{
    return _alignment_state->getIntervalAlignmentType();
}

void EventPlotState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    _alignment_state->setIntervalAlignmentType(type);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double EventPlotState::getOffset() const
{
    return _alignment_state->getOffset();
}

void EventPlotState::setOffset(double offset)
{
    _alignment_state->setOffset(offset);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double EventPlotState::getWindowSize() const
{
    return _alignment_state->getWindowSize();
}

void EventPlotState::setWindowSize(double window_size)
{
    // Delegate to alignment state - the signal connection in constructor
    // handles updating view state bounds and emitting viewStateChanged
    _alignment_state->setWindowSize(window_size);
}

void EventPlotState::addPlotEvent(QString const & event_name, QString const & event_key)
{
    std::string name_str = event_name.toStdString();
    std::string key_str = event_key.toStdString();

    EventPlotOptions options;
    options.event_key = key_str;

    _data.plot_events[name_str] = options;
    markDirty();
    emit plotEventAdded(event_name);
    emit stateChanged();
}

void EventPlotState::removePlotEvent(QString const & event_name)
{
    std::string name_str = event_name.toStdString();
    auto it = _data.plot_events.find(name_str);
    if (it != _data.plot_events.end()) {
        _data.plot_events.erase(it);
        markDirty();
        emit plotEventRemoved(event_name);
        emit stateChanged();
    }
}

std::vector<QString> EventPlotState::getPlotEventNames() const
{
    std::vector<QString> names;
    names.reserve(_data.plot_events.size());
    for (auto const & [name, _] : _data.plot_events) {
        names.push_back(QString::fromStdString(name));
    }
    return names;
}

std::optional<EventPlotOptions> EventPlotState::getPlotEventOptions(QString const & event_name) const
{
    std::string name_str = event_name.toStdString();
    auto it = _data.plot_events.find(name_str);
    if (it != _data.plot_events.end()) {
        return it->second;
    }
    return std::nullopt;
}

void EventPlotState::updatePlotEventOptions(QString const & event_name, EventPlotOptions const & options)
{
    std::string name_str = event_name.toStdString();
    auto it = _data.plot_events.find(name_str);
    if (it != _data.plot_events.end()) {
        it->second = options;
        markDirty();
        emit plotEventOptionsChanged(event_name);
        emit stateChanged();
    }
}

// === View State ===

void EventPlotState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
        // Note: No stateChanged() here - zoom is a view-only change that
        // doesn't require scene rebuild. The projection matrix handles it.
    }
}

void EventPlotState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
        // Note: No stateChanged() here - zoom is a view-only change that
        // doesn't require scene rebuild. The projection matrix handles it.
    }
}

void EventPlotState::setPan(double x_pan, double y_pan)
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

void EventPlotState::setXBounds(double x_min, double x_max)
{
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        // Update time axis state to match
        _relative_time_axis_state->setRangeSilent(x_min, x_max);
        _data.time_axis = _relative_time_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void EventPlotState::setAxisOptions(EventPlotAxisOptions const & options)
{
    _data.axis_options = options;
    markDirty();
    emit axisOptionsChanged();
    emit stateChanged();
}

QString EventPlotState::getBackgroundColor() const
{
    return QString::fromStdString(_data.background_color);
}

void EventPlotState::setBackgroundColor(QString const & hex_color)
{
    std::string hex_str = hex_color.toStdString();
    if (_data.background_color != hex_str) {
        _data.background_color = hex_str;
        markDirty();
        emit backgroundColorChanged(hex_color);
        emit stateChanged();
    }
}

void EventPlotState::setPinned(bool pinned)
{
    if (_data.pinned != pinned) {
        _data.pinned = pinned;
        markDirty();
        emit pinnedChanged(pinned);
        emit stateChanged();
    }
}

void EventPlotState::setSortingMode(TrialSortMode mode)
{
    if (_data.sorting_mode != mode) {
        _data.sorting_mode = mode;
        markDirty();
        emit sortingModeChanged(mode);
        emit stateChanged();  // Triggers scene rebuild
    }
}

std::string EventPlotState::toJson() const
{
    // Include instance_id in serialization for restoration
    EventPlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool EventPlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<EventPlotStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Restore alignment state from serialized data
        _alignment_state->data() = _data.alignment;

        // Restore relative time axis state from serialized data
        _relative_time_axis_state->data() = _data.time_axis;

        // EventPlot Y-axis is fixed [-1, 1]; ensure view_state matches
        _data.view_state.y_min = -1.0;
        _data.view_state.y_max = 1.0;

        // Emit all signals to ensure UI updates
        emit viewStateChanged();
        emit axisOptionsChanged();
        emit pinnedChanged(_data.pinned);
        emit stateChanged();
        return true;
    }
    return false;
}
