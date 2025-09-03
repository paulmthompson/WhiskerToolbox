#ifndef WHISKERTOOLBOX_OPENCV_IMAGE_PROCESSOR_HPP
#define WHISKERTOOLBOX_OPENCV_IMAGE_PROCESSOR_HPP

#include "ImageProcessor.hpp"
#include <opencv2/opencv.hpp>
#include <functional>
#include <map>
#include <string>

namespace ImageProcessing {

/**
 * @brief OpenCV-based image processor implementation
 * 
 * This class provides the OpenCV backend for the image processing chain.
 * It maintains the current MediaData processing functionality while
 * implementing the generic ImageProcessor interface.
 * 
 * The internal format is cv::Mat, allowing the processing chain to work
 * directly with OpenCV functions without conversions between steps.
 * 
 * Uses variant approach to handle both 8-bit (CV_8U) and 32-bit float (CV_32F) processing.
 */
class OpenCVImageProcessor : public ImageProcessor {
public:
    OpenCVImageProcessor() = default;
    ~OpenCVImageProcessor() override = default;

    /**
     * @brief Process image data through the OpenCV processing chain
     * @param input_data Raw image data as ImageData variant
     * @param image_size Dimensions of the image
     * @return Processed image data as ImageData variant (same type as input)
     */
    ImageData processImage(ImageData const& input_data, ImageSize const& image_size) override;

    /**
     * @brief Add an OpenCV processing step to the chain
     * @param key Unique identifier for the processing step
     * @param processor Function that operates on cv::Mat* (cast from void*)
     */
    void addProcessingStep(std::string const& key, 
                         std::function<void(void*)> processor) override;

    /**
     * @brief Convenience method to add OpenCV processing step with proper typing
     * @param key Unique identifier for the processing step
     * @param processor Function that operates on cv::Mat&
     */
    void addOpenCVProcessingStep(std::string const& key, 
                               std::function<void(cv::Mat&)> processor);

    /**
     * @brief Remove a processing step from the chain
     * @param key Identifier of the processing step to remove
     */
    void removeProcessingStep(std::string const& key) override;

    /**
     * @brief Clear all processing steps
     */
    void clearProcessingSteps() override;

    /**
     * @brief Check if a processing step exists
     * @param key Identifier to check
     * @return true if the step exists, false otherwise
     */
    bool hasProcessingStep(std::string const& key) const override;

    /**
     * @brief Get the number of processing steps in the chain
     * @return Number of steps
     */
    size_t getProcessingStepCount() const override;

protected:
    /**
     * @brief Convert from ImageData variant to cv::Mat
     * @param data Input data as variant
     * @param size Image dimensions
     * @return Pointer to cv::Mat (as void*) - caller must cast and manage memory
     */
    void* convertFromRaw(ImageData const& data, ImageSize const& size) override;

    /**
     * @brief Convert from cv::Mat back to ImageData variant
     * @param internal_data Pointer to cv::Mat (as void*)
     * @param size Image dimensions  
     * @param output_type Variant index (0 for uint8_t, 1 for float)
     * @return ImageData variant in requested format
     */
    ImageData convertToRaw(void* internal_data, ImageSize const& size, size_t output_type) override;

private:
    std::map<std::string, std::function<void(void*)>> _processing_steps;
};

/**
 * @brief Registration helper for OpenCV processor
 * 
 * This function registers the OpenCV processor with the ProcessorRegistry
 * when OpenCV support is enabled. It should be called during initialization.
 */
void registerOpenCVProcessor();

} // namespace ImageProcessing

#endif // WHISKERTOOLBOX_OPENCV_IMAGE_PROCESSOR_HPP
