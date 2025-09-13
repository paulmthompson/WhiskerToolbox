#ifndef WHISKER_TRACING_HPP
#define WHISKER_TRACING_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>

// Forward declaration
namespace whisker {
class WhiskerTracker;
class Line2D;
}// namespace whisker

class MaskData;

/**
 * @brief Parameters for whisker tracing operation
 */
struct WhiskerTracingParameters : public TransformParametersBase {
    bool use_processed_data = true;        // Whether to use processed or raw data
    int clip_length = 0;                   // Number of points to clip from whisker tips
    float whisker_length_threshold = 50.0f;// Minimum whisker length threshold
    int batch_size = 10;                   // Number of frames to process in parallel
    bool use_parallel_processing = true;   // Whether to use OpenMP parallel processing
    bool use_mask_data = false;            // Whether to use mask data for seed selection
    std::shared_ptr<MaskData> mask_data;   // Optional mask data for seed selection
};

/**
 * @brief Whisker tracing operation that detects whiskers in media data
 */
class WhiskerTracingOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    /**
     * @brief Gets the target input type index for this operation.
     * @return The type index for MediaData.
     */
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null MediaData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the whisker tracing operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the whisker tracing calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MediaData object.
     * @param transformParameters Parameters for the calculation, including the media data pointer.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the whisker tracing calculation with progress callback.
     * @param dataVariant The variant holding a non-null shared_ptr to the MediaData object.
     * @param transformParameters Parameters for the calculation, including the media data pointer.
     * @param progressCallback Progress callback function.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;

private:
    /**
     * @brief Converts whisker::Line2D to Line2D
     * @param whisker_line The whisker tracker line
     * @return The converted line
     */
    static Line2D convert_to_Line2D(whisker::Line2D const & whisker_line);

    /**
     * @brief Clips a whisker line by removing the specified number of points from the end
     * @param line The line to clip
     * @param clip_length The number of points to remove
     */
    static void clip_whisker(Line2D & line, int clip_length);

    /**
     * @brief Traces whiskers in a single image
     * @param whisker_tracker The whisker tracker
     * @param image_data The image data
     * @param image_size The image dimensions
     * @param clip_length The number of points to clip from whisker tips
     * @param mask_data Optional mask data for seed selection
     * @param time_index Time index for mask data
     * @return Vector of traced whisker lines
     */
    std::vector<Line2D> trace_single_image(whisker::WhiskerTracker & whisker_tracker,
                                           std::vector<uint8_t> const & image_data,
                                           ImageSize const & image_size,
                                           int clip_length,
                                           MaskData const * mask_data = nullptr,
                                           int time_index = 0);

    /**
     * @brief Traces whiskers in multiple images in parallel
     * @param whisker_tracker The whisker tracker
     * @param images Vector of image data
     * @param image_size The image dimensions
     * @param clip_length The number of points to clip from whisker tips
     * @param mask_data Optional mask data for seed selection
     * @param time_indices Vector of time indices for mask data
     * @return Vector of vectors of traced whisker lines (one vector per image)
     */
    std::vector<std::vector<Line2D>> trace_multiple_images(
            whisker::WhiskerTracker & whisker_tracker,
            std::vector<std::vector<uint8_t>> const & images,
            ImageSize const & image_size,
            int clip_length,
            MaskData const * mask_data = nullptr,
            std::vector<int> const & time_indices = {});

    /**
     * @brief Converts mask data to binary mask format for whisker tracker
     * @param mask_data The mask data object
     * @param time_index The time index to get mask for
     * @param image_size The image dimensions
     * @return Binary mask as vector of uint8_t (255 for true pixels, 0 for false)
     */
    static std::vector<uint8_t> convert_mask_to_binary(MaskData const * mask_data,
                                                       int time_index,
                                                       ImageSize const & image_size);
};

#endif// WHISKER_TRACING_HPP
