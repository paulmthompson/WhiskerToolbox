#include "OpenCVImageProcessor.hpp"
#include "utils/opencv_utility.hpp"
#include <memory>

namespace ImageProcessing {

std::vector<uint8_t> OpenCVImageProcessor::processImage(
    std::vector<uint8_t> const& input_data,
    ImageSize const& image_size) {
    
    if (input_data.empty() || _opencv_process_chain.empty()) {
        return input_data;  // Return unmodified data if no processing needed
    }

    // Convert to internal format (cv::Mat)
    std::unique_ptr<cv::Mat> mat_ptr(
        static_cast<cv::Mat*>(convertFromRaw(input_data, image_size)));
    
    if (!mat_ptr || mat_ptr->empty()) {
        return input_data;  // Return original data if conversion failed
    }

    // Apply all processing steps in order
    for (auto const& [key, process] : _opencv_process_chain) {
        process(*mat_ptr);
    }

    // Convert back to raw format
    return convertToRaw(mat_ptr.get(), image_size);
}

void OpenCVImageProcessor::addProcessingStep(std::string const& key, 
                                           std::function<void(void*)> processor) {
    // Wrap the void* processor to work with cv::Mat*
    auto opencv_processor = [processor](cv::Mat& mat) {
        processor(&mat);
    };
    _opencv_process_chain[key] = std::move(opencv_processor);
}

void OpenCVImageProcessor::addOpenCVProcessingStep(std::string const& key, 
                                                 std::function<void(cv::Mat&)> processor) {
    _opencv_process_chain[key] = std::move(processor);
}

void OpenCVImageProcessor::removeProcessingStep(std::string const& key) {
    _opencv_process_chain.erase(key);
}

void OpenCVImageProcessor::clearProcessingSteps() {
    _opencv_process_chain.clear();
}

bool OpenCVImageProcessor::hasProcessingStep(std::string const& key) const {
    return _opencv_process_chain.find(key) != _opencv_process_chain.end();
}

size_t OpenCVImageProcessor::getProcessingStepCount() const {
    return _opencv_process_chain.size();
}

void* OpenCVImageProcessor::convertFromRaw(std::vector<uint8_t> const& data, 
                                         ImageSize const& size) {
    // Create a mutable copy for the conversion function
    std::vector<uint8_t> mutable_data = data;
    
    // Use existing utility function to convert to cv::Mat
    cv::Mat mat = convert_vector_to_mat(mutable_data, size);
    
    if (mat.empty()) {
        return nullptr;
    }
    
    // Return a copy that the caller owns
    return new cv::Mat(mat.clone());
}

std::vector<uint8_t> OpenCVImageProcessor::convertToRaw(void* internal_data, 
                                                       ImageSize const& size) {
    if (!internal_data) {
        return {};
    }
    
    cv::Mat* mat = static_cast<cv::Mat*>(internal_data);
    
    if (mat->empty()) {
        return {};
    }
    
    // Ensure the mat is in the right format for conversion
    mat->reshape(1, size.width * size.height);
    
    // Convert back to vector using the total size
    std::vector<uint8_t> result;
    result.resize(mat->total() * mat->channels());
    
    if (mat->isContinuous()) {
        std::memcpy(result.data(), mat->data, result.size());
    } else {
        // Handle non-continuous matrices
        size_t idx = 0;
        for (int i = 0; i < mat->rows; ++i) {
            uint8_t* row_ptr = mat->ptr<uint8_t>(i);
            std::memcpy(result.data() + idx, row_ptr, mat->cols * mat->channels());
            idx += mat->cols * mat->channels();
        }
    }
    
    return result;
}

void registerOpenCVProcessor() {
    ProcessorRegistry::registerProcessor("opencv", []() -> std::unique_ptr<ImageProcessor> {
        return std::make_unique<OpenCVImageProcessor>();
    });
}

} // namespace ImageProcessing
