#include "HeatmapState.hpp"

#include "Plots/PlotAlignmentWidget/Core/PlotAlignmentState.hpp"

#include <rfl/json.hpp>

HeatmapState::HeatmapState(QObject * parent)
    : EditorState(parent),
      _alignment_state(std::make_unique<PlotAlignmentState>(this))
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Sync initial alignment data from member state
    _data.alignment = _alignment_state->data();

    // Forward alignment state signals to this object's signals
    connect(_alignment_state.get(), &PlotAlignmentState::alignmentEventKeyChanged,
            this, &HeatmapState::alignmentEventKeyChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::intervalAlignmentTypeChanged,
            this, &HeatmapState::intervalAlignmentTypeChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::offsetChanged,
            this, &HeatmapState::offsetChanged);
    connect(_alignment_state.get(), &PlotAlignmentState::windowSizeChanged,
            this, &HeatmapState::windowSizeChanged);
}

QString HeatmapState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void HeatmapState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

QString HeatmapState::getAlignmentEventKey() const
{
    return _alignment_state->getAlignmentEventKey();
}

void HeatmapState::setAlignmentEventKey(QString const & key)
{
    _alignment_state->setAlignmentEventKey(key);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

IntervalAlignmentType HeatmapState::getIntervalAlignmentType() const
{
    return _alignment_state->getIntervalAlignmentType();
}

void HeatmapState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    _alignment_state->setIntervalAlignmentType(type);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double HeatmapState::getOffset() const
{
    return _alignment_state->getOffset();
}

void HeatmapState::setOffset(double offset)
{
    _alignment_state->setOffset(offset);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

double HeatmapState::getWindowSize() const
{
    return _alignment_state->getWindowSize();
}

void HeatmapState::setWindowSize(double window_size)
{
    _alignment_state->setWindowSize(window_size);
    // Sync to data for serialization
    _data.alignment = _alignment_state->data();
    markDirty();
    emit stateChanged();
}

std::string HeatmapState::toJson() const
{
    // Include instance_id in serialization for restoration
    HeatmapStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool HeatmapState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<HeatmapStateData>(json);
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
