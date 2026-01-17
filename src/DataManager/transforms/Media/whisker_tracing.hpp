#ifndef REFACTORED_WHISKER_TRACING_HPP
#define REFACTORED_WHISKER_TRACING_HPP

#include "transforms/data_transforms.hpp"
#include "datamanager_export.h"

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
 * @brief Converts mask data to a binary mask image sized to the media frame.
 */
std::vector<uint8_t> DATAMANAGER_EXPORT convert_mask_to_binary(MaskData const* mask_data,
                                            int time_index,
                                            ImageSize const& image_size);

/**
 * @brief Parameters for whisker tracing operation
 */
struct WhiskerTracingParameters : public TransformParametersBase {
    bool use_processed_data = true;
    int clip_length = 0;
    float whisker_length_threshold = 50.0f;
    int batch_size = 100;
    bool use_parallel_processing = true;
    bool use_mask_data = false;
    std::shared_ptr<MaskData> mask_data;
    std::shared_ptr<whisker::WhiskerTracker> tracker;
    size_t queue_size = 20;
    // Note: producer_threads is no longer needed as the pipeline uses a single producer
};

/**
 * @brief Whisker tracing operation that detects whiskers in media data
 */
class WhiskerTracingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const& dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                            TransformParametersBase const* transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                            TransformParametersBase const* transformParameters,
                            ProgressCallback progressCallback) override;

private:
    static Line2D convert_to_Line2D(whisker::Line2D const& whisker_line);
    static void clip_whisker(Line2D& line, int clip_length);

    std::vector<Line2D> trace_single_image(whisker::WhiskerTracker& whisker_tracker,
                                           std::vector<uint8_t> const& image_data,
                                           ImageSize const& image_size,
                                           int clip_length,
                                           MaskData const* mask_data = nullptr,
                                           int time_index = 0);

    std::vector<std::vector<Line2D>> trace_multiple_images(
        whisker::WhiskerTracker& whisker_tracker,
        std::vector<std::vector<uint8_t>> const& images,
        ImageSize const& image_size,
        int clip_length,
        MaskData const* mask_data = nullptr,
        std::vector<int> const& time_indices = {});
};

#endif // REFACTORED_WHISKER_TRACING_HPP
