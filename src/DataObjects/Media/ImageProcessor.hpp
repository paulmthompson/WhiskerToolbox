#ifndef WHISKERTOOLBOX_IMAGE_PROCESSOR_HPP
#define WHISKERTOOLBOX_IMAGE_PROCESSOR_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "MediaStorage.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace ImageProcessing {

// Use the shared MediaStorage types
using ImageData = MediaStorage::ImageDataVariant;

/**
 * @brief Base interface for image processing chains
 * 
 * This abstract base class provides a framework for implementing different
 * image processing backends while maintaining type safety and avoiding
 * unnecessary conversions within processing chains.
 * 
 * The design allows for backend-specific implementations (e.g., OpenCV, SIMD, etc.)
 * while providing a unified interface for MediaData to use.
 * 
 * Uses variant approach to handle both 8-bit (uint8_t) and 32-bit (float) image data.
 */
class ImageProcessor {
public:
    virtual ~ImageProcessor() = default;

    /**
     * @brief Process image data through the configured processing chain
     * @param input_data Raw image data as ImageData variant
     * @param image_size Dimensions of the image
     * @return Processed image data as ImageData variant (same type as input)
     */
    virtual ImageData processImage(ImageData const& input_data, ImageSize const& image_size) = 0;

    /**
     * @brief Add a processing step to the chain
     * @param key Unique identifier for the processing step
     * @param processor Function that processes the internal image format
     */
    virtual void addProcessingStep(std::string const& key, 
                                 std::function<void(void*)> processor) = 0;

    /**
     * @brief Remove a processing step from the chain
     * @param key Identifier of the processing step to remove
     */
    virtual void removeProcessingStep(std::string const& key) = 0;

    /**
     * @brief Clear all processing steps
     */
    virtual void clearProcessingSteps() = 0;

    /**
     * @brief Check if a processing step exists
     * @param key Identifier to check
     * @return true if the step exists, false otherwise
     */
    virtual bool hasProcessingStep(std::string const& key) const = 0;

    /**
     * @brief Get the number of processing steps in the chain
     * @return Number of steps
     */
    virtual size_t getProcessingStepCount() const = 0;

protected:
    /**
     * @brief Convert from ImageData variant to internal format
     * Called once at the beginning of the processing chain
     */
    virtual void* convertFromRaw(ImageData const& data, ImageSize const& size) = 0;

    /**
     * @brief Convert from internal format back to ImageData variant
     * Called once at the end of the processing chain
     * @param internal_data The internal representation of the image
     * @param size Image dimensions
     * @param output_type Index indicating which variant type to output (0 for uint8_t, 1 for float)
     */
    virtual ImageData convertToRaw(void* internal_data, ImageSize const& size, size_t output_type) = 0;
};

/**
 * @brief Factory function type for creating ImageProcessor instances
 */
using ProcessorFactory = std::function<std::unique_ptr<ImageProcessor>()>;

/**
 * @brief Registry for different ImageProcessor implementations
 * 
 * This allows runtime selection of different processing backends
 * and makes it easy to add new implementations without modifying existing code.
 */
class ProcessorRegistry {
public:
    /**
     * @brief Register a processor factory with a given name
     * @param name Unique name for the processor type
     * @param factory Factory function that creates processor instances
     */
    static void registerProcessor(std::string const& name, ProcessorFactory factory);

    /**
     * @brief Create a processor instance by name
     * @param name Name of the registered processor type
     * @return Unique pointer to the processor instance, or nullptr if not found
     */
    static std::unique_ptr<ImageProcessor> createProcessor(std::string const& name);

    /**
     * @brief Get all registered processor names
     * @return Vector of available processor names
     */
    static std::vector<std::string> getAvailableProcessors();

    /**
     * @brief Check if a processor type is available
     * @param name Name to check
     * @return true if available, false otherwise
     */
    static bool isProcessorAvailable(std::string const& name);

private:
    static std::map<std::string, ProcessorFactory>& getRegistry();
};

} // namespace ImageProcessing

#endif // WHISKERTOOLBOX_IMAGE_PROCESSOR_HPP
