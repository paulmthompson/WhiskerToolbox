#include "HorizontalAxisState.hpp"

HorizontalAxisState::HorizontalAxisState(QObject * parent)
    : QObject(parent)
{
}

double HorizontalAxisState::getXMin() const
{
    return _data.x_min;
}

void HorizontalAxisState::setXMin(double x_min)
{
    if (_data.x_min != x_min) {
        _data.x_min = x_min;
        emit xMinChanged(x_min);
        emit rangeChanged(_data.x_min, _data.x_max);
    }
}

double HorizontalAxisState::getXMax() const
{
    return _data.x_max;
}

void HorizontalAxisState::setXMax(double x_max)
{
    if (_data.x_max != x_max) {
        _data.x_max = x_max;
        emit xMaxChanged(x_max);
        emit rangeChanged(_data.x_min, _data.x_max);
    }
}

void HorizontalAxisState::setRange(double x_min, double x_max)
{
    bool changed = (_data.x_min != x_min) || (_data.x_max != x_max);
    if (changed) {
        _data.x_min = x_min;
        _data.x_max = x_max;
        emit xMinChanged(x_min);
        emit xMaxChanged(x_max);
        emit rangeChanged(x_min, x_max);
    }
}

void HorizontalAxisState::setRangeSilent(double x_min, double x_max)
{
    bool changed = (_data.x_min != x_min) || (_data.x_max != x_max);
    if (changed) {
        _data.x_min = x_min;
        _data.x_max = x_max;
        // Only emit rangeUpdated, not the individual change signals
        // This allows UI to update without triggering recursive updates
        emit rangeUpdated(x_min, x_max);
    }
}
