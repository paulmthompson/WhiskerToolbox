/**
 * @file TimeController.cpp
 * @brief Implementation of Qt-free visualization time state
 */

#include "TimeController.hpp"

#include <utility>

void TimeController::setCurrentTime(TimePosition const & position) {

    if (_time_update_in_progress) {
        return;
    }

    if (_current_position == position) {
        return;
    }

    _time_update_in_progress = true;

    _current_position = position;

    if (_on_time_changed) {
        _on_time_changed(position);
    }

    _time_update_in_progress = false;
}

void TimeController::setCurrentTime(TimeFrameIndex index, std::shared_ptr<TimeFrame> tf) {
    setCurrentTime(TimePosition(index, std::move(tf)));
}

TimePosition TimeController::currentPosition() const {
    return _current_position;
}

TimeFrameIndex TimeController::currentTimeIndex() const {
    return _current_position.index;
}

std::shared_ptr<TimeFrame> TimeController::currentTimeFrame() const {
    return _current_position.time_frame;
}

void TimeController::setActiveTimeKey(TimeKey key) {
    if (key == _active_time_key) {
        return;
    }
    TimeKey const old_key = _active_time_key;
    _active_time_key = std::move(key);
    if (_on_time_key_changed) {
        _on_time_key_changed(_active_time_key, old_key);
    }
}

TimeKey TimeController::activeTimeKey() const {
    return _active_time_key;
}

void TimeController::setOnTimeChanged(TimeChangedCallback cb) {
    _on_time_changed = std::move(cb);
}

void TimeController::setOnTimeKeyChanged(TimeKeyChangedCallback cb) {
    _on_time_key_changed = std::move(cb);
}
