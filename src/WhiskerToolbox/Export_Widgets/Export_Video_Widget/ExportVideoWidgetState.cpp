#include "ExportVideoWidgetState.hpp"

#include <iostream>

ExportVideoWidgetState::ExportVideoWidgetState(QObject * parent)
    : EditorState(parent) {
    // Store instance ID in data for serialization
    _data.instance_id = getInstanceId().toStdString();
}

QString ExportVideoWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void ExportVideoWidgetState::setDisplayName(QString const & name) {
    if (QString::fromStdString(_data.display_name) != name) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string ExportVideoWidgetState::toJson() const {
    try {
        // Ensure instance_id is up to date
        auto data_copy = _data;
        data_copy.instance_id = getInstanceId().toStdString();
        return rfl::json::write(data_copy);
    } catch (std::exception const & e) {
        std::cerr << "ExportVideoWidgetState::toJson failed: " << e.what() << std::endl;
        return "{}";
    }
}

bool ExportVideoWidgetState::fromJson(std::string const & json) {
    try {
        auto result = rfl::json::read<ExportVideoWidgetStateData>(json);
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
        std::cerr << "ExportVideoWidgetState::fromJson failed: " << e.what() << std::endl;
        return false;
    }
}

void ExportVideoWidgetState::setSelectedMediaWidgetId(QString const & id) {
    if (QString::fromStdString(_data.selected_media_widget_id) != id) {
        _data.selected_media_widget_id = id.toStdString();
        markDirty();
        emit selectedMediaWidgetIdChanged(id);
    }
}

QString ExportVideoWidgetState::selectedMediaWidgetId() const {
    return QString::fromStdString(_data.selected_media_widget_id);
}

void ExportVideoWidgetState::setOutputFilename(QString const & filename) {
    if (QString::fromStdString(_data.output_filename) != filename) {
        _data.output_filename = filename.toStdString();
        markDirty();
        emit outputFilenameChanged(filename);
    }
}

QString ExportVideoWidgetState::outputFilename() const {
    return QString::fromStdString(_data.output_filename);
}

void ExportVideoWidgetState::setStartFrame(int frame) {
    if (_data.start_frame != frame) {
        _data.start_frame = frame;
        markDirty();
        emit startFrameChanged(frame);
    }
}

int ExportVideoWidgetState::startFrame() const {
    return _data.start_frame;
}

void ExportVideoWidgetState::setEndFrame(int frame) {
    if (_data.end_frame != frame) {
        _data.end_frame = frame;
        markDirty();
        emit endFrameChanged(frame);
    }
}

int ExportVideoWidgetState::endFrame() const {
    return _data.end_frame;
}

void ExportVideoWidgetState::setFrameRate(int rate) {
    if (_data.frame_rate != rate) {
        _data.frame_rate = rate;
        markDirty();
        emit frameRateChanged(rate);
    }
}

int ExportVideoWidgetState::frameRate() const {
    return _data.frame_rate;
}

void ExportVideoWidgetState::setOutputWidth(int width) {
    if (_data.output_width != width) {
        _data.output_width = width;
        markDirty();
        emit outputDimensionsChanged(width, _data.output_height);
    }
}

int ExportVideoWidgetState::outputWidth() const {
    return _data.output_width;
}

void ExportVideoWidgetState::setOutputHeight(int height) {
    if (_data.output_height != height) {
        _data.output_height = height;
        markDirty();
        emit outputDimensionsChanged(_data.output_width, height);
    }
}

int ExportVideoWidgetState::outputHeight() const {
    return _data.output_height;
}
