#include "PlotAlignmentState.hpp"

PlotAlignmentState::PlotAlignmentState(QObject * parent)
    : QObject(parent)
{
}

QString PlotAlignmentState::getAlignmentEventKey() const
{
    return QString::fromStdString(_data.alignment_event_key);
}

void PlotAlignmentState::setAlignmentEventKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.alignment_event_key != key_str) {
        _data.alignment_event_key = key_str;
        emit alignmentEventKeyChanged(key);
    }
}

IntervalAlignmentType PlotAlignmentState::getIntervalAlignmentType() const
{
    return _data.interval_alignment_type;
}

void PlotAlignmentState::setIntervalAlignmentType(IntervalAlignmentType type)
{
    if (_data.interval_alignment_type != type) {
        _data.interval_alignment_type = type;
        emit intervalAlignmentTypeChanged(type);
    }
}

double PlotAlignmentState::getOffset() const
{
    return _data.offset;
}

void PlotAlignmentState::setOffset(double offset)
{
    if (_data.offset != offset) {
        _data.offset = offset;
        emit offsetChanged(offset);
    }
}

double PlotAlignmentState::getWindowSize() const
{
    return _data.window_size;
}

void PlotAlignmentState::setWindowSize(double window_size)
{
    if (_data.window_size != window_size) {
        _data.window_size = window_size;
        emit windowSizeChanged(window_size);
    }
}
