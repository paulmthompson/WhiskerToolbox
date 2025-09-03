
#include "Media/Media_Data.hpp"
#ifdef ENABLE_OPENCV
#include "OpenCVImageProcessor.hpp"
#endif

#include <algorithm>
#include <stdexcept>

MediaData::MediaData() {
#ifdef ENABLE_OPENCV
    // Register OpenCV processor
    static bool opencv_registered = false;
    if (!opencv_registered) {
        ImageProcessing::registerOpenCVProcessor();
        opencv_registered = true;
    }
    
    // Default to OpenCV processor
    setImageProcessor("opencv");
#endif
};

MediaData::~MediaData() = default;

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
    _resizeDataStorage();
};

void MediaData::updateHeight(int const height) {
    _height = height;
    _resizeDataStorage();
};

void MediaData::updateWidth(int const width) {
    _width = width;
    _resizeDataStorage();
};

void MediaData::LoadMedia(std::string const & name) {
    doLoadMedia(name);
}

void MediaData::LoadFrame(int const frame_id) {
    doLoadFrame(frame_id);

    _last_loaded_frame = frame_id;
}

std::vector<uint8_t> const & MediaData::getRawData8(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (MediaStorage::is8Bit(_rawData)) {
        return std::get<MediaStorage::ImageData8>(_rawData);
    } else {
        // Convert from 32-bit to 8-bit
        auto const& float_data = std::get<MediaStorage::ImageData32>(_rawData);
        _convertTo8Bit(float_data, _temp_8bit_buffer);
        return _temp_8bit_buffer;
    }
}

std::vector<float> const & MediaData::getRawData32(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (MediaStorage::is32Bit(_rawData)) {
        return std::get<MediaStorage::ImageData32>(_rawData);
    } else {
        // Convert from 8-bit to 32-bit
        auto const& uint8_data = std::get<MediaStorage::ImageData8>(_rawData);
        _convertTo32Bit(uint8_data, _temp_32bit_buffer);
        return _temp_32bit_buffer;
    }
}

MediaData::DataStorage const& MediaData::getRawDataVariant(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }
    return _rawData;
}

std::vector<uint8_t> MediaData::getProcessedData8(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (_last_processed_frame != _last_loaded_frame) {
        _processData();
    }

    if (MediaStorage::is8Bit(_processedData)) {
        return std::get<MediaStorage::ImageData8>(_processedData);
    } else {
        // Convert from 32-bit to 8-bit
        auto const& float_data = std::get<MediaStorage::ImageData32>(_processedData);
        std::vector<uint8_t> result;
        _convertTo8Bit(float_data, result);
        return result;
    }
}

std::vector<float> MediaData::getProcessedData32(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (_last_processed_frame != _last_loaded_frame) {
        _processData();
    }

    if (MediaStorage::is32Bit(_processedData)) {
        return std::get<MediaStorage::ImageData32>(_processedData);
    } else {
        // Convert from 8-bit to 32-bit
        auto const& uint8_data = std::get<MediaStorage::ImageData8>(_processedData);
        std::vector<float> result;
        _convertTo32Bit(uint8_data, result);
        return result;
    }
}

MediaData::DataStorage MediaData::getProcessedDataVariant(int const frame_number) {
    if (frame_number != _last_loaded_frame) {
        LoadFrame(frame_number);
    }

    if (_last_processed_frame != _last_loaded_frame) {
        _processData();
    }

    return _processedData;
}

void MediaData::setRawData(MediaStorage::ImageData8 data) {
    _bit_depth = BitDepth::Bit8;
    _rawData = std::move(data);
    // Only resize processed data to match the new bit depth, don't touch _rawData
    if (MediaStorage::is32Bit(_processedData)) {
        int const new_size = _height * _width * _display_format_bytes;
        _processedData = MediaStorage::ImageData8(static_cast<size_t>(new_size));
    }
    // Invalidate processed data since raw data changed
    _last_processed_frame = -1;
}

void MediaData::setRawData(MediaStorage::ImageData32 data) {
    _bit_depth = BitDepth::Bit32;
    _rawData = std::move(data);
    // Only resize processed data to match the new bit depth, don't touch _rawData
    if (MediaStorage::is8Bit(_processedData)) {
        int const new_size = _height * _width * _display_format_bytes;
        _processedData = MediaStorage::ImageData32(static_cast<size_t>(new_size));
    }
    // Invalidate processed data since raw data changed
    _last_processed_frame = -1;
}


// ImageProcessor-based methods
bool MediaData::setImageProcessor(std::string const& processor_name) {
    auto new_processor = ImageProcessing::ProcessorRegistry::createProcessor(processor_name);
    if (new_processor) {
        _image_processor = std::move(new_processor);
        _processor_name = processor_name;
        // Reprocess data with new processor
        if (_last_loaded_frame != -1) {
            _processData();
            notifyObservers();
        }
        return true;
    }
    return false;
}

std::string MediaData::getImageProcessorName() const {
    return _processor_name;
}

void MediaData::addProcessingStep(std::string const& key, std::function<void(void*)> processor) {
    if (_image_processor) {
        _image_processor->addProcessingStep(key, std::move(processor));
        _processData();
        notifyObservers();
    }
}

void MediaData::removeProcessingStep(std::string const& key) {
    if (_image_processor) {
        _image_processor->removeProcessingStep(key);
        _processData();
        notifyObservers();
    }
}

void MediaData::clearProcessingSteps() {
    if (_image_processor) {
        _image_processor->clearProcessingSteps();
        _processData();
        notifyObservers();
    }
}

bool MediaData::hasProcessingStep(std::string const& key) const {
    return _image_processor ? _image_processor->hasProcessingStep(key) : false;
}

size_t MediaData::getProcessingStepCount() const {
    return _image_processor ? _image_processor->getProcessingStepCount() : 0;
}

void MediaData::setBitDepth(BitDepth depth) {
    if (_bit_depth != depth) {
        _bit_depth = depth;
        _resizeDataStorage();
        // Invalidate processed data since bit depth changed
        _last_processed_frame = -1;
    }
}

void MediaData::_processData() {
    _processedData = _rawData;  // Copy raw data to processed data

    // Use ImageProcessor system if available
    if (_image_processor && _image_processor->getProcessingStepCount() > 0) {
        // No conversion needed! Both use the same MediaStorage::ImageDataVariant type
        _processedData = _image_processor->processImage(_processedData, getImageSize());
    }

    _last_processed_frame = _last_loaded_frame;
}

void MediaData::_convertTo8Bit(MediaStorage::ImageData32 const& source, MediaStorage::ImageData8& target) const {
    target.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        // Clamp to [0, 255] range and convert to uint8_t
        float clamped = std::max(0.0f, std::min(255.0f, source[i]));
        target[i] = static_cast<uint8_t>(clamped);
    }
}

void MediaData::_convertTo32Bit(MediaStorage::ImageData8 const& source, MediaStorage::ImageData32& target) const {
    target.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        target[i] = static_cast<float>(source[i]);
    }
}

void MediaData::_resizeDataStorage() {
    int const new_size = _height * _width * _display_format_bytes;
    
    // Only resize if size or bit depth has actually changed
    bool needs_resize = false;
    
    if (_bit_depth == BitDepth::Bit8) {
        if (!MediaStorage::is8Bit(_rawData) || std::get<MediaStorage::ImageData8>(_rawData).size() != static_cast<size_t>(new_size)) {
            _rawData = MediaStorage::ImageData8(static_cast<size_t>(new_size));
            needs_resize = true;
        }
        if (!MediaStorage::is8Bit(_processedData) || std::get<MediaStorage::ImageData8>(_processedData).size() != static_cast<size_t>(new_size)) {
            _processedData = MediaStorage::ImageData8(static_cast<size_t>(new_size));
            needs_resize = true;
        }
    } else {
        if (!MediaStorage::is32Bit(_rawData) || std::get<MediaStorage::ImageData32>(_rawData).size() != static_cast<size_t>(new_size)) {
            _rawData = MediaStorage::ImageData32(static_cast<size_t>(new_size));
            needs_resize = true;
        }
        if (!MediaStorage::is32Bit(_processedData) || std::get<MediaStorage::ImageData32>(_processedData).size() != static_cast<size_t>(new_size)) {
            _processedData = MediaStorage::ImageData32(static_cast<size_t>(new_size));
            needs_resize = true;
        }
    }
    
    // Only resize temp buffers if something actually changed
    if (needs_resize) {
        _temp_8bit_buffer.resize(static_cast<size_t>(new_size));
        _temp_32bit_buffer.resize(static_cast<size_t>(new_size));
    }
}
