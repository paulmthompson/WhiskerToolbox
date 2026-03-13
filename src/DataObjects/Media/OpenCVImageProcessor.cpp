#include "OpenCVImageProcessor.hpp"
#include <opencv2/opencv.hpp>
#include <memory>
#include <cstring>

namespace ImageProcessing {

ImageData OpenCVImageProcessor::processImage(ImageData const& input_data, ImageSize const& image_size) {
    if (_processing_steps.empty()) {
        return input_data;  // Return unmodified data if no processing needed
    }

    // Convert to internal format (cv::Mat)
    std::unique_ptr<cv::Mat> mat_ptr(static_cast<cv::Mat*>(convertFromRaw(input_data, image_size)));
    
    if (!mat_ptr || mat_ptr->empty()) {
        return input_data;  // Return original data if conversion failed
    }

    // Apply all processing steps
    for (auto const& [key, process] : _processing_steps) {
        process(mat_ptr.get());
    }

    // Convert back to the same format as input
    size_t output_type = input_data.index();  // 0 for uint8_t, 1 for float
    return convertToRaw(mat_ptr.get(), image_size, output_type);
}

void OpenCVImageProcessor::addProcessingStep(std::string const& key, 
                                           std::function<void(void*)> processor) {
    _processing_steps[key] = std::move(processor);
}

void OpenCVImageProcessor::addOpenCVProcessingStep(std::string const& key, 
                                                 std::function<void(cv::Mat&)> processor) {
    // Wrap the cv::Mat& processor to work with void*
    auto wrapped_processor = [processor](void* mat_ptr) {
        auto* mat = static_cast<cv::Mat*>(mat_ptr);
        processor(*mat);
    };
    _processing_steps[key] = std::move(wrapped_processor);
}

void OpenCVImageProcessor::removeProcessingStep(std::string const& key) {
    _processing_steps.erase(key);
}

void OpenCVImageProcessor::clearProcessingSteps() {
    _processing_steps.clear();
}

bool OpenCVImageProcessor::hasProcessingStep(std::string const& key) const {
    return _processing_steps.find(key) != _processing_steps.end();
}

size_t OpenCVImageProcessor::getProcessingStepCount() const {
    return _processing_steps.size();
}

void* OpenCVImageProcessor::convertFromRaw(ImageData const& data, ImageSize const& size) {
    return std::visit([&size](auto const& vec) -> void* {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        
        if (vec.empty()) {
            return nullptr;
        }
        
        int cv_type = CV_8UC1;  // Default initialization
        if constexpr (std::is_same_v<T, uint8_t>) {
            cv_type = CV_8UC1;
        } else if constexpr (std::is_same_v<T, float>) {
            cv_type = CV_32FC1;
        } else {
            return nullptr;  // Unsupported type
        }
        
        // Create cv::Mat from the vector data
        // Note: We create a copy to ensure the cv::Mat owns the data during processing
        auto* mat = new cv::Mat(size.height, size.width, cv_type);
        std::memcpy(mat->data, vec.data(), vec.size() * sizeof(T));
        
        return mat;
    }, data);
}

ImageData OpenCVImageProcessor::convertToRaw(void* internal_data, ImageSize const& /*size*/, size_t output_type) {
    if (!internal_data) {
        if (output_type == 0) {
            return std::vector<uint8_t>{};
        } else {
            return std::vector<float>{};
        }
    }
    
    auto* mat = static_cast<cv::Mat*>(internal_data);
    
    if (mat->empty()) {
        if (output_type == 0) {
            return std::vector<uint8_t>{};
        } else {
            return std::vector<float>{};
        }
    }
    
    if (output_type == 0) {
        // Convert to uint8_t vector
        std::vector<uint8_t> result;
        result.resize(static_cast<size_t>(mat->total() * mat->channels()));
        
        if (mat->type() == CV_8UC1) {
            // Direct copy for 8-bit data
            if (mat->isContinuous()) {
                std::memcpy(result.data(), mat->data, result.size());
            } else {
                size_t idx = 0;
                for (int i = 0; i < mat->rows; ++i) {
                    uint8_t* row_ptr = mat->ptr<uint8_t>(i);
                    auto cols_channels = static_cast<size_t>(mat->cols * mat->channels());
                    std::memcpy(result.data() + idx, row_ptr, cols_channels);
                    idx += cols_channels;
                }
            }
        } else if (mat->type() == CV_32FC1) {
            // Convert from float to uint8_t
            cv::Mat temp_mat;
            mat->convertTo(temp_mat, CV_8UC1);
            if (temp_mat.isContinuous()) {
                std::memcpy(result.data(), temp_mat.data, result.size());
            } else {
                size_t idx = 0;
                for (int i = 0; i < temp_mat.rows; ++i) {
                    uint8_t* row_ptr = temp_mat.ptr<uint8_t>(i);
                    auto cols_channels = static_cast<size_t>(temp_mat.cols * temp_mat.channels());
                    std::memcpy(result.data() + idx, row_ptr, cols_channels);
                    idx += cols_channels;
                }
            }
        }
        return result;
    } else {
        // Convert to float vector  
        std::vector<float> result;
        result.resize(static_cast<size_t>(mat->total() * mat->channels()));
        
        if (mat->type() == CV_32FC1) {
            // Direct copy for float data
            if (mat->isContinuous()) {
                std::memcpy(result.data(), mat->data, result.size() * sizeof(float));
            } else {
                size_t idx = 0;
                for (int i = 0; i < mat->rows; ++i) {
                    float* row_ptr = mat->ptr<float>(i);
                    auto cols_channels = static_cast<size_t>(mat->cols * mat->channels());
                    std::memcpy(result.data() + idx, row_ptr, cols_channels * sizeof(float));
                    idx += cols_channels;
                }
            }
        } else if (mat->type() == CV_8UC1) {
            // Convert from uint8_t to float
            cv::Mat temp_mat;
            mat->convertTo(temp_mat, CV_32FC1);
            if (temp_mat.isContinuous()) {
                std::memcpy(result.data(), temp_mat.data, result.size() * sizeof(float));
            } else {
                size_t idx = 0;
                for (int i = 0; i < temp_mat.rows; ++i) {
                    float* row_ptr = temp_mat.ptr<float>(i);
                    auto cols_channels = static_cast<size_t>(temp_mat.cols * temp_mat.channels());
                    std::memcpy(result.data() + idx, row_ptr, cols_channels * sizeof(float));
                    idx += cols_channels;
                }
            }
        }
        return result;
    }
}

void registerOpenCVProcessor() {
    ProcessorRegistry::registerProcessor("opencv", []() -> std::unique_ptr<ImageProcessor> {
        return std::make_unique<OpenCVImageProcessor>();
    });
}

} // namespace ImageProcessing
