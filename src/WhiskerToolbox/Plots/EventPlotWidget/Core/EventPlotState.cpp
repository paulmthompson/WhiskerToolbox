#include "EventPlotState.hpp"

#include <rfl/json.hpp>

EventPlotState::EventPlotState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
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
    return QString::fromStdString(_data.alignment_event_key);
}

void EventPlotState::setAlignmentEventKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.alignment_event_key != key_str) {
        _data.alignment_event_key = key_str;
        markDirty();
        emit alignmentEventKeyChanged(key);
        emit stateChanged();
    }
}

IntervalAlignmentType EventPlotState::getIntervalAlignmentType() const
{
    return _data.interval_alignment_type;
}

void EventPlotState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    if (_data.interval_alignment_type != type) {
        _data.interval_alignment_type = type;
        markDirty();
        emit intervalAlignmentTypeChanged(type);
        emit stateChanged();
    }
}

double EventPlotState::getOffset() const
{
    return _data.offset;
}

void EventPlotState::setOffset(double offset)
{
    if (_data.offset != offset) {
        _data.offset = offset;
        markDirty();
        emit offsetChanged(offset);
        emit stateChanged();
    }
}

double EventPlotState::getWindowSize() const
{
    return _data.window_size;
}

void EventPlotState::setWindowSize(double window_size)
{
    if (_data.window_size != window_size) {
        _data.window_size = window_size;
        markDirty();
        emit windowSizeChanged(window_size);
        emit stateChanged();
    }
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

        emit stateChanged();
        return true;
    }
    return false;
}
