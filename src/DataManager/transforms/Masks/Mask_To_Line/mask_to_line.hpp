#ifndef MASK_TO_LINE_HPP
#define MASK_TO_LINE_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;
class MaskData;

enum class LinePointSelectionMethod {
    NearestToReference,  // Start from point nearest to reference
    Skeletonize          // Use skeletonization to create the line
};

struct MaskToLineParameters : public TransformParametersBase {
    // Reference point parameters
    float reference_x = 0.0f;
    float reference_y = 0.0f;

    // Processing parameters
    LinePointSelectionMethod method = LinePointSelectionMethod::Skeletonize;
    int polynomial_order = 3;
    float error_threshold = 5.0f;  // Maximum allowable error for points to be included
    bool remove_outliers = true;
    int input_point_subsample_factor = 1;  // For point subsampling of the input mask (1 = use all points)
    bool should_smooth_line = false; // Smooth the final line using polynomial fit
    float output_resolution = 5.0f; // Approximate spacing in pixels between output points
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Convert a mask to a line by ordering points
 *
 * @param mask_data The mask data to convert
 * @param params Parameters controlling the conversion process
 * @return std::shared_ptr<LineData> The resulting line data
 */
std::shared_ptr<LineData> mask_to_line(MaskData const * mask_data, 
                                       MaskToLineParameters const * params);

std::shared_ptr<LineData> mask_to_line(MaskData const* mask_data,
                                       MaskToLineParameters const* params,
                                       ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class MaskToLineOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null MaskData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the mask-to-line operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the mask-to-line conversion using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the conversion.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;
                           
    /**
     * @brief Executes the mask-to-line conversion with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the conversion.
     * @param progressCallback Callback function to report progress (0-100).
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif // MASK_TO_LINE_HPP 
