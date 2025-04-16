
#include "Media/Media_Data.hpp"

#include "ImageSize/ImageSize.hpp"
#include "utils/opencv_utility.hpp"

#include <opencv2/core/mat.hpp>

MediaData::MediaData() {
    int const new_size = _height * _width * _display_format_bytes;
    _rawData.resize(static_cast<size_t>(new_size));
    _processedData.resize(static_cast<size_t>(new_size));
    setFormat(DisplayFormat::Gray);
};

void MediaData::setFormat(DisplayFormat const format) {
    _format = format;
    switch (_format) {
        case DisplayFormat::Gray:
            _display_format_bytes = 1;
            break;
        case DisplayFormat::Color:
            _display_format_bytes = 4;
            break;
        default:
            _display_format_bytes = 1;
            break;
    }
    int const new_size = _height * _width * _display_format_bytes;
    _rawData.resize(static_cast<size_t>(new_size));
    _processedData.resize(static_cast<size_t>(new_size));
};

void MediaData::updateHeight(int const height) {
    _height = height;
    int const new_size = _height * _width * _display_format_bytes;
    _rawData.resize(static_cast<size_t>(new_size));
    _processedData.resize(static_cast<size_t>(new_size));
};

void MediaData::updateWidth(int const width) {
    _width = width;
    int const new_size = _height * _width * _display_format_bytes;
    _rawData.resize(static_cast<size_t>(new_size));
    _processedData.resize(static_cast<size_t>(new_size));
};

void MediaData::LoadMedia(std::string const & name) {
    doLoadMedia(name);
}

void MediaData::LoadFrame(int const frame_id) {
    doLoadFrame(frame_id);

    _last_loaded_frame = frame_id;
}

std::vector<uint8_t> const & MediaData::getRawData(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    return _rawData;
}

std::vector<uint8_t> MediaData::getProcessedData(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (_last_processed_frame != _last_loaded_frame) {
        _processData();
    }

    return _processedData;
}

void MediaData::setProcess(std::string const & key, std::function<void(cv::Mat & input)> process) {
    this->_process_chain[key] = std::move(process);
    _processData();

    notifyObservers();
    //NOTIFY
}

void MediaData::removeProcess(std::string const & key) {
    _process_chain.erase(key);
    _processData();

    notifyObservers();
    //NOTIFY
}

void MediaData::_processData() {

    _processedData = _rawData;

    auto m2 = convert_vector_to_mat(_processedData, getImageSize());

    for (auto const & [key, process]: _process_chain) {
        process(m2);
    }

    m2.reshape(1, getWidth() * getHeight());

    _processedData.assign(m2.data, m2.data + m2.total() * m2.channels());

    _last_processed_frame = _last_loaded_frame;
}
