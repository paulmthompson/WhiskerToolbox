#include "TimeScrollBarState.hpp"

#include "TimeScrollBarPlayback.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
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
        nlohmann::json j = nlohmann::json::parse(json);
        if (!j.is_object()) {
            return false;
        }
        if (j.contains("target_fps") && j.contains("play_speed")) {
            j.erase("play_speed");
        }
        if (!j.contains("target_fps") && j.contains("play_speed")) {
            int const legacy = std::max(1, j["play_speed"].get<int>());
            j["target_fps"] = 25.F * static_cast<float>(legacy);
            j.erase("play_speed");
        }
        auto result = rfl::json::read<TimeScrollBarStateData>(j.dump());
        if (result) {
            _data = result.value();
            if (!std::isfinite(_data.target_fps)) {
                _data.target_fps = 25.F;
            }
            _data.target_fps = time_scroll_bar::snapTargetFpsToPreset(_data.target_fps);
            // Restore instance ID from serialized data
            if (!_data.instance_id.empty()) {
                setInstanceId(QString::fromStdString(_data.instance_id));
            }
            emit stateChanged();
            return true;
        }
        return false;
    } catch (std::exception const & e) {
        std::cerr << "TimeScrollBarState::fromJson failed: " << e.what() << std::endl;
        return false;
    }
}

// === Getters ===

float TimeScrollBarState::targetFps() const {
    return _data.target_fps;
}

int TimeScrollBarState::frameJump() const {
    return _data.frame_jump;
}

bool TimeScrollBarState::isPlaying() const {
    return _data.is_playing;
}

// === Setters ===

void TimeScrollBarState::setTargetFps(float fps) {
    if (!std::isfinite(fps)) {
        fps = 25.F;
    }
    float const snapped = time_scroll_bar::snapTargetFpsToPreset(fps);
    if (std::abs(_data.target_fps - snapped) > 1e-4F) {
        _data.target_fps = snapped;
        markDirty();
        emit targetFpsChanged(snapped);
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
