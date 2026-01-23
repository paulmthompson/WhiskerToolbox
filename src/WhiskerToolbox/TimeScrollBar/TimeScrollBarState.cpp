#include "TimeScrollBarState.hpp"

#include <iostream>

TimeScrollBarState::TimeScrollBarState(QObject * parent)
    : EditorState(parent) {
    // Store instance ID in data for serialization
    _data.instance_id = getInstanceId().toStdString();
}

QString TimeScrollBarState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TimeScrollBarState::setDisplayName(QString const & name) {
    if (QString::fromStdString(_data.display_name) != name) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TimeScrollBarState::toJson() const {
    try {
        // Ensure instance_id is up to date
        auto data_copy = _data;
        data_copy.instance_id = getInstanceId().toStdString();
        return rfl::json::write(data_copy);
    } catch (std::exception const & e) {
        std::cerr << "TimeScrollBarState::toJson failed: " << e.what() << std::endl;
        return "{}";
    }
}

bool TimeScrollBarState::fromJson(std::string const & json) {
    try {
        auto result = rfl::json::read<TimeScrollBarStateData>(json);
        if (result) {
            _data = result.value();
            // Restore instance ID from serialized data
            if (!_data.instance_id.empty()) {
                setInstanceId(QString::fromStdString(_data.instance_id));
            }
            markClean();
            return true;
        }
        return false;
    } catch (std::exception const & e) {
        std::cerr << "TimeScrollBarState::fromJson failed: " << e.what() << std::endl;
        return false;
    }
}

// === Getters ===

int TimeScrollBarState::playSpeed() const {
    return _data.play_speed;
}

int TimeScrollBarState::frameJump() const {
    return _data.frame_jump;
}

bool TimeScrollBarState::isPlaying() const {
    return _data.is_playing;
}

// === Setters ===

void TimeScrollBarState::setPlaySpeed(int speed) {
    if (_data.play_speed != speed) {
        _data.play_speed = speed;
        markDirty();
        emit playSpeedChanged(speed);
    }
}

void TimeScrollBarState::setFrameJump(int jump) {
    if (_data.frame_jump != jump) {
        _data.frame_jump = jump;
        markDirty();
        emit frameJumpChanged(jump);
    }
}

void TimeScrollBarState::setIsPlaying(bool playing) {
    if (_data.is_playing != playing) {
        _data.is_playing = playing;
        markDirty();
        emit isPlayingChanged(playing);
    }
}
