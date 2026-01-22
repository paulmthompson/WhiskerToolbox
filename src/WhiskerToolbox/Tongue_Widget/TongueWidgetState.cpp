#include "TongueWidgetState.hpp"

#include <iostream>

TongueWidgetState::TongueWidgetState(QObject * parent)
    : EditorState(parent) {
    // Store instance ID in data for serialization
    _data.instance_id = getInstanceId().toStdString();
}

QString TongueWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TongueWidgetState::setDisplayName(QString const & name) {
    if (QString::fromStdString(_data.display_name) != name) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TongueWidgetState::toJson() const {
    try {
        // Ensure instance_id is up to date
        auto data_copy = _data;
        data_copy.instance_id = getInstanceId().toStdString();
        return rfl::json::write(data_copy);
    } catch (std::exception const & e) {
        std::cerr << "TongueWidgetState::toJson failed: " << e.what() << std::endl;
        return "{}";
    }
}

bool TongueWidgetState::fromJson(std::string const & json) {
    try {
        auto result = rfl::json::read<TongueWidgetStateData>(json);
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
        std::cerr << "TongueWidgetState::fromJson failed: " << e.what() << std::endl;
        return false;
    }
}

void TongueWidgetState::addProcessedFrame(int frame) {
    _data.processed_frames.push_back(frame);
    markDirty();
    emit frameProcessed(frame);
}

std::vector<int> const & TongueWidgetState::processedFrames() const {
    return _data.processed_frames;
}

void TongueWidgetState::clearProcessedFrames() {
    if (!_data.processed_frames.empty()) {
        _data.processed_frames.clear();
        markDirty();
        emit processedFramesCleared();
    }
}
