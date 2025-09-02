#include "OpenCVImageProcessor.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include <memory>

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
        cv::Mat* mat = static_cast<cv::Mat*>(mat_ptr);
        processor(*mat);
    };
    _processing_steps[key] = std::move(wrapped_processor);
}
}

void OpenCVImageProcessor::addOpenCVProcessingStep(std::string const& key, 
                                                 std::function<void(cv::Mat&)> processor) {
    _opencv_process_chain[key] = std::move(processor);
}

void OpenCVImageProcessor::addOpenCVProcessingStep8(std::string const& key, 
                                                  std::function<void(cv::Mat&)> processor) {
    _opencv_process_chain_8bit[key] = std::move(processor);
}

void OpenCVImageProcessor::addOpenCVProcessingStep32(std::string const& key, 
                                                   std::function<void(cv::Mat&)> processor) {
    _opencv_process_chain_32bit[key] = std::move(processor);
}

void OpenCVImageProcessor::removeProcessingStep(std::string const& key) {
    _opencv_process_chain.erase(key);
    _opencv_process_chain_8bit.erase(key);
    _opencv_process_chain_32bit.erase(key);
}

void OpenCVImageProcessor::clearProcessingSteps() {
    _opencv_process_chain.clear();
    _opencv_process_chain_8bit.clear();
    _opencv_process_chain_32bit.clear();
}

bool OpenCVImageProcessor::hasProcessingStep(std::string const& key) const {
    return _opencv_process_chain.find(key) != _opencv_process_chain.end() ||
           _opencv_process_chain_8bit.find(key) != _opencv_process_chain_8bit.end() ||
           _opencv_process_chain_32bit.find(key) != _opencv_process_chain_32bit.end();
}

size_t OpenCVImageProcessor::getProcessingStepCount() const {
    return _opencv_process_chain.size() + _opencv_process_chain_8bit.size() + _opencv_process_chain_32bit.size();
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
