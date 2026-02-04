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

QString EventPlotState::getPlotEventKey() const
{
    return QString::fromStdString(_data.plot_event_key);
}

void EventPlotState::setPlotEventKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.plot_event_key != key_str) {
        _data.plot_event_key = key_str;
        markDirty();
        emit plotEventKeyChanged(key);
        emit stateChanged();
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
