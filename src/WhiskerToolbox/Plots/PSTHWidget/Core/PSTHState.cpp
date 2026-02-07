#include "PSTHState.hpp"

#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/Core/RelativeTimeAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

PSTHState::PSTHState(QObject * parent)
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
    double window_size = _data.alignment.window_size;
    double half_window = window_size / 2.0;
    _relative_time_axis_state->setRangeSilent(-half_window, half_window);
    _data.time_axis = _relative_time_axis_state->data();

    // Initialize view state bounds from window size
    _data.view_state.x_min = -half_window;
    _data.view_state.x_max = half_window;

    // Sync initial vertical axis data from member state
    _data.vertical_axis = _vertical_axis_state->data();

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &PSTHState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &PSTHState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &PSTHState::offsetChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, [this](double window_size) {
                // Update view state data bounds when window size changes
                double half_window = window_size / 2.0;
                _data.view_state.x_min = -half_window;
                _data.view_state.x_max = half_window;
                // Reset zoom/pan when the window changes
                _data.view_state.x_zoom = 1.0;
                _data.view_state.x_pan = 0.0;
                // Update time axis range
                _relative_time_axis_state->setRangeSilent(-half_window, half_window);
                // Sync to data
                _data.time_axis = _relative_time_axis_state->data();
                markDirty();
                emit windowSizeChanged(window_size);
                emit viewStateChanged();
                emit stateChanged();
            });

    // Forward relative time axis state signals
    // When range changes, update window size (for backward compatibility)
    auto syncTimeAxisData = [this]() {
        // Sync to data for serialization
        _data.time_axis = _relative_time_axis_state->data();
        // Update window size to match range (centered at 0)
        double range = _data.time_axis.max_range - _data.time_axis.min_range;
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

    // Forward vertical axis state signals to this object's signals
    // Note: We don't emit yMinChanged/yMaxChanged from PSTHState anymore
    // Components should connect directly to verticalAxisState() signals
    auto syncVerticalAxisData = [this]() {
        // Sync to data for serialization
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
            this, syncVerticalAxisData);
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeUpdated,
            this, syncVerticalAxisData);
}

QString PSTHState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void PSTHState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString PSTHState::getAlignmentEventKey() const
{
    return _alignment_state->getAlignmentEventKey();
}

void PSTHState::setAlignmentEventKey(QString const & key)
{
    _alignment_state->setAlignmentEventKey(key);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

IntervalAlignmentType PSTHState::getIntervalAlignmentType() const
{
    return _alignment_state->getIntervalAlignmentType();
}

void PSTHState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    _alignment_state->setIntervalAlignmentType(type);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double PSTHState::getOffset() const
{
    return _alignment_state->getOffset();
}

void PSTHState::setOffset(double offset)
{
    _alignment_state->setOffset(offset);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double PSTHState::getWindowSize() const
{
    return _alignment_state->getWindowSize();
}

void PSTHState::setWindowSize(double window_size)
{
    _alignment_state->setWindowSize(window_size);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

void PSTHState::addPlotEvent(QString const & event_name, QString const & event_key)
{
    std::string name_str = event_name.toStdString();
    std::string key_str = event_key.toStdString();

    PSTHEventOptions options;
    options.event_key = key_str;

    _data.plot_events[name_str] = options;
    markDirty();
    emit plotEventAdded(event_name);
    emit stateChanged();
}

void PSTHState::removePlotEvent(QString const & event_name)
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

std::vector<QString> PSTHState::getPlotEventNames() const
{
    std::vector<QString> names;
    names.reserve(_data.plot_events.size());
    for (auto const & [name, _] : _data.plot_events) {
        names.push_back(QString::fromStdString(name));
    }
    return names;
}

std::optional<PSTHEventOptions> PSTHState::getPlotEventOptions(QString const & event_name) const
{
    std::string name_str = event_name.toStdString();
    auto it = _data.plot_events.find(name_str);
    if (it != _data.plot_events.end()) {
        return it->second;
    }
    return std::nullopt;
}

void PSTHState::updatePlotEventOptions(QString const & event_name, PSTHEventOptions const & options)
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

PSTHStyle PSTHState::getStyle() const
{
    return _data.style;
}

void PSTHState::setStyle(PSTHStyle style)
{
    if (_data.style != style) {
        _data.style = style;
        markDirty();
        emit styleChanged(style);
        emit stateChanged();
    }
}

double PSTHState::getBinSize() const
{
    return _data.bin_size;
}

void PSTHState::setBinSize(double bin_size)
{
    if (_data.bin_size != bin_size) {
        _data.bin_size = bin_size;
        markDirty();
        emit binSizeChanged(bin_size);
        emit stateChanged();
    }
}

// === View State (Zoom / Pan / Bounds) ===

void PSTHState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void PSTHState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void PSTHState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void PSTHState::setXBounds(double x_min, double x_max)
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


std::string PSTHState::toJson() const
{
    // Include instance_id in serialization for restoration
    PSTHStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool PSTHState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<PSTHStateData>(json);
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

        // Restore vertical axis state from serialized data
        _vertical_axis_state->data() = _data.vertical_axis;

        emit stateChanged();
        return true;
    }
    return false;
}
