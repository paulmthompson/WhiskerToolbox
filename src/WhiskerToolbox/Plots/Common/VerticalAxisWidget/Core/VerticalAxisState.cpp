#include "VerticalAxisState.hpp"

VerticalAxisState::VerticalAxisState(QObject * parent)
    : QObject(parent)
{
}

double VerticalAxisState::getYMin() const
{
    return _data.y_min;
}

void VerticalAxisState::setYMin(double y_min)
{
    if (_data.y_min != y_min) {
        _data.y_min = y_min;
        emit yMinChanged(y_min);
        emit rangeChanged(_data.y_min, _data.y_max);
    }
}

double VerticalAxisState::getYMax() const
{
    return _data.y_max;
}

void VerticalAxisState::setYMax(double y_max)
{
    if (_data.y_max != y_max) {
        _data.y_max = y_max;
        emit yMaxChanged(y_max);
        emit rangeChanged(_data.y_min, _data.y_max);
    }
}

void VerticalAxisState::setRange(double y_min, double y_max)
{
    bool changed = (_data.y_min != y_min) || (_data.y_max != y_max);
    if (changed) {
        _data.y_min = y_min;
        _data.y_max = y_max;
        emit yMinChanged(y_min);
        emit yMaxChanged(y_max);
        emit rangeChanged(y_min, y_max);
    }
}

void VerticalAxisState::setRangeSilent(double y_min, double y_max)
{
    bool changed = (_data.y_min != y_min) || (_data.y_max != y_max);
    if (changed) {
        _data.y_min = y_min;
        _data.y_max = y_max;
        // Only emit rangeUpdated, not the individual change signals
        // This allows UI to update without triggering recursive updates
        emit rangeUpdated(y_min, y_max);
    }
}
