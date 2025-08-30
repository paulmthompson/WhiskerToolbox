#ifndef MASK_HOLE_FILLING_HPP
#define MASK_HOLE_FILLING_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class MaskData;

struct MaskHoleFillingParameters : public TransformParametersBase {
    // No additional parameters needed for hole filling
    // The algorithm automatically fills all enclosed holes
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Fill holes in mask data using flood fill algorithm
 * 
 * This function applies hole filling to remove enclosed background regions
 * (holes) within mask objects. The algorithm identifies background pixels that
 * are completely surrounded by foreground pixels and fills them in.
 * 
 * Uses 4-connectivity for flood fill from image boundaries to identify
 * background regions connected to borders. Any background region not connected
 * to the border is considered a hole and gets filled.
 * 
 * @param mask_data Input mask data containing binary masks
 * @param params Transform parameters (currently unused but kept for consistency)
 * @return std::shared_ptr<MaskData> New mask data with holes filled
 */
std::shared_ptr<MaskData> fill_mask_holes(
        MaskData const * mask_data,
        MaskHoleFillingParameters const * params);

/**
 * @brief Fill holes in mask data with progress reporting
 * 
 * @param mask_data Input mask data containing binary masks
 * @param params Transform parameters (currently unused but kept for consistency)
 * @param progressCallback Function to report progress (0-100)
 * @return std::shared_ptr<MaskData> New mask data with holes filled
 */
std::shared_ptr<MaskData> fill_mask_holes(
        MaskData const * mask_data,
        MaskHoleFillingParameters const * params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Operation class for mask hole filling transform
 * 
 * Implements the TransformOperation interface for mask hole filling.
 * This allows the transform to be registered and used within the transform system.
 */
class MaskHoleFillingOperation final : public TransformOperation {
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
     * @brief Gets default parameters for this operation.
     * @return A unique_ptr to default MaskHoleFillingParameters
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the mask hole filling using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @return DataTypeVariant containing a std::shared_ptr<MaskData> on success,
     * or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the mask hole filling with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<MaskData> on success,
     * or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif // MASK_HOLE_FILLING_HPP 