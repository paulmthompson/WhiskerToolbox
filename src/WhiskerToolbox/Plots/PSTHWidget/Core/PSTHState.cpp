#include "PSTHState.hpp"

#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"

#include <rfl/json.hpp>

PSTHState::PSTHState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this))
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Sync initial alignment data from member state
    _data.alignment = _alignment_state->data();

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &PSTHState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &PSTHState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &PSTHState::offsetChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, &PSTHState::windowSizeChanged);
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

double PSTHState::getYMin() const
{
    return _data.y_min;
}

void PSTHState::setYMin(double y_min)
{
    if (_data.y_min != y_min) {
        _data.y_min = y_min;
        markDirty();
        emit yMinChanged(y_min);
        emit stateChanged();
    }
}

double PSTHState::getYMax() const
{
    return _data.y_max;
}

void PSTHState::setYMax(double y_max)
{
    if (_data.y_max != y_max) {
        _data.y_max = y_max;
        markDirty();
        emit yMaxChanged(y_max);
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

        emit stateChanged();
        return true;
    }
    return false;
}
