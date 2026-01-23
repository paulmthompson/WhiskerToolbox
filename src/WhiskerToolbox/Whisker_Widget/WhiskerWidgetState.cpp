#include "WhiskerWidgetState.hpp"

#include <iostream>

WhiskerWidgetState::WhiskerWidgetState(QObject * parent)
    : EditorState(parent) {
    // Store instance ID in data for serialization
    _data.instance_id = getInstanceId().toStdString();
}

QString WhiskerWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void WhiskerWidgetState::setDisplayName(QString const & name) {
    if (QString::fromStdString(_data.display_name) != name) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string WhiskerWidgetState::toJson() const {
    try {
        // Ensure instance_id is up to date
        auto data_copy = _data;
        data_copy.instance_id = getInstanceId().toStdString();
        return rfl::json::write(data_copy);
    } catch (std::exception const & e) {
        std::cerr << "WhiskerWidgetState::toJson failed: " << e.what() << std::endl;
        return "{}";
    }
}

bool WhiskerWidgetState::fromJson(std::string const & json) {
    try {
        auto result = rfl::json::read<WhiskerWidgetStateData>(json);
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
        std::cerr << "WhiskerWidgetState::fromJson failed: " << e.what() << std::endl;
        return false;
    }
}

// === Getters ===

FaceOrientation WhiskerWidgetState::faceOrientation() const {
    return static_cast<FaceOrientation>(_data.face_orientation);
}

int WhiskerWidgetState::numWhiskersToTrack() const {
    return _data.num_whiskers_to_track;
}

double WhiskerWidgetState::lengthThreshold() const {
    return _data.length_threshold;
}

int WhiskerWidgetState::clipLength() const {
    return _data.clip_length;
}

float WhiskerWidgetState::linkingTolerance() const {
    return _data.linking_tolerance;
}

std::string const & WhiskerWidgetState::whiskerPadKey() const {
    return _data.whisker_pad_key;
}

float WhiskerWidgetState::whiskerPadX() const {
    return _data.whisker_pad_x;
}

float WhiskerWidgetState::whiskerPadY() const {
    return _data.whisker_pad_y;
}

bool WhiskerWidgetState::useMaskMode() const {
    return _data.use_mask_mode;
}

std::string const & WhiskerWidgetState::selectedMaskKey() const {
    return _data.selected_mask_key;
}

int WhiskerWidgetState::currentWhisker() const {
    return _data.current_whisker;
}

bool WhiskerWidgetState::autoDL() const {
    return _data.auto_dl;
}

// === Setters ===

void WhiskerWidgetState::setFaceOrientation(FaceOrientation orientation) {
    int const new_val = static_cast<int>(orientation);
    if (_data.face_orientation != new_val) {
        _data.face_orientation = new_val;
        markDirty();
        emit faceOrientationChanged(orientation);
    }
}

void WhiskerWidgetState::setNumWhiskersToTrack(int num) {
    if (_data.num_whiskers_to_track != num) {
        _data.num_whiskers_to_track = num;
        markDirty();
        emit numWhiskersToTrackChanged(num);
    }
}

void WhiskerWidgetState::setLengthThreshold(double threshold) {
    if (_data.length_threshold != threshold) {
        _data.length_threshold = threshold;
        markDirty();
        emit lengthThresholdChanged(threshold);
    }
}

void WhiskerWidgetState::setClipLength(int length) {
    if (_data.clip_length != length) {
        _data.clip_length = length;
        markDirty();
        emit clipLengthChanged(length);
    }
}

void WhiskerWidgetState::setLinkingTolerance(float tolerance) {
    if (_data.linking_tolerance != tolerance) {
        _data.linking_tolerance = tolerance;
        markDirty();
        emit linkingToleranceChanged(tolerance);
    }
}

void WhiskerWidgetState::setWhiskerPadKey(std::string const & key) {
    if (_data.whisker_pad_key != key) {
        _data.whisker_pad_key = key;
        markDirty();
        emit whiskerPadKeyChanged(key);
    }
}

void WhiskerWidgetState::setWhiskerPadPosition(float x, float y) {
    if (_data.whisker_pad_x != x || _data.whisker_pad_y != y) {
        _data.whisker_pad_x = x;
        _data.whisker_pad_y = y;
        markDirty();
        emit whiskerPadPositionChanged(x, y);
    }
}

void WhiskerWidgetState::setUseMaskMode(bool use_mask) {
    if (_data.use_mask_mode != use_mask) {
        _data.use_mask_mode = use_mask;
        markDirty();
        emit useMaskModeChanged(use_mask);
    }
}

void WhiskerWidgetState::setSelectedMaskKey(std::string const & key) {
    if (_data.selected_mask_key != key) {
        _data.selected_mask_key = key;
        markDirty();
        emit selectedMaskKeyChanged(key);
    }
}

void WhiskerWidgetState::setCurrentWhisker(int whisker) {
    if (_data.current_whisker != whisker) {
        _data.current_whisker = whisker;
        markDirty();
        emit currentWhiskerChanged(whisker);
    }
}

void WhiskerWidgetState::setAutoDL(bool auto_dl) {
    if (_data.auto_dl != auto_dl) {
        _data.auto_dl = auto_dl;
        markDirty();
        emit autoDLChanged(auto_dl);
    }
}
