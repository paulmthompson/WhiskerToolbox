#include "RelativeTimeAxisState.hpp"

RelativeTimeAxisState::RelativeTimeAxisState(QObject * parent)
    : QObject(parent)
{
}

double RelativeTimeAxisState::getMinRange() const
{
    return _data.min_range;
}

void RelativeTimeAxisState::setMinRange(double min_range)
{
    if (_data.min_range != min_range) {
        _data.min_range = min_range;
        emit minRangeChanged(min_range);
        emit rangeChanged(_data.min_range, _data.max_range);
    }
}

double RelativeTimeAxisState::getMaxRange() const
{
    return _data.max_range;
}

void RelativeTimeAxisState::setMaxRange(double max_range)
{
    if (_data.max_range != max_range) {
        _data.max_range = max_range;
        emit maxRangeChanged(max_range);
        emit rangeChanged(_data.min_range, _data.max_range);
    }
}

void RelativeTimeAxisState::setRange(double min_range, double max_range)
{
    bool changed = (_data.min_range != min_range) || (_data.max_range != max_range);
    if (changed) {
        _data.min_range = min_range;
        _data.max_range = max_range;
        emit minRangeChanged(min_range);
        emit maxRangeChanged(max_range);
        emit rangeChanged(min_range, max_range);
    }
}

void RelativeTimeAxisState::setRangeSilent(double min_range, double max_range)
{
    bool changed = (_data.min_range != min_range) || (_data.max_range != max_range);
    if (changed) {
        _data.min_range = min_range;
        _data.max_range = max_range;
        // Only emit rangeUpdated, not the individual change signals
        // This allows UI to update without triggering recursive updates
        emit rangeUpdated(min_range, max_range);
    }
}
