
#include "Media/Media_Data.hpp"

#include "utils/opencv_utility.hpp"
#include "ImageProcessor.hpp"
#include "OpenCVImageProcessor.hpp"

#include <opencv2/core/mat.hpp>

MediaData::MediaData() {
    // Register OpenCV processor
    static bool opencv_registered = false;
    if (!opencv_registered) {
        ImageProcessing::registerOpenCVProcessor();
        opencv_registered = true;
    }
    
    // Default to OpenCV processor
    setImageProcessor("opencv");
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

// New ImageProcessor-based methods
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

void MediaData::_processData() {
    _processedData = _rawData;

    // Use new ImageProcessor system if available
    if (_image_processor && _image_processor->getProcessingStepCount() > 0) {
        _processedData = _image_processor->processImage(_processedData, getImageSize());
    }
    // Fallback to legacy OpenCV system for backward compatibility
    else if (!_process_chain.empty()) {
        auto m2 = convert_vector_to_mat(_processedData, getImageSize());

        for (auto const & [key, process]: _process_chain) {
            process(m2);
        }

        m2.reshape(1, getWidth() * getHeight());
        _processedData.assign(m2.data, m2.data + m2.total() * m2.channels());
    }

    _last_processed_frame = _last_loaded_frame;
}
