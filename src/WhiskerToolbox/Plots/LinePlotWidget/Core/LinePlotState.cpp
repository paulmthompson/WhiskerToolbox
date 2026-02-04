#include "LinePlotState.hpp"

#include "Plots/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"

#include <rfl/json.hpp>

LinePlotState::LinePlotState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this))
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Sync initial alignment data from member state
    _data.alignment = _alignment_state->data();

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &LinePlotState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &LinePlotState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &LinePlotState::offsetChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, &LinePlotState::windowSizeChanged);
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

        emit stateChanged();
        return true;
    }
    return false;
}
