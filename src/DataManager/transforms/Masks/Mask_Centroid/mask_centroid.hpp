#ifndef WHISKERTOOLBOX_MASK_CENTROID_HPP
#define WHISKERTOOLBOX_MASK_CENTROID_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class MaskData;
class PointData;

struct MaskCentroidParameters : public TransformParametersBase {
    // No additional parameters needed for basic centroid calculation
    // The centroid calculation assumes constant density and is parameter-free

    // Optional: could add parameters for future extensions
    // bool weight_by_distance = false;
    // float center_x = 0.0f;
    // float center_y = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Calculate the centroid of masks at each timestamp
 *
 * For each timestamp in the mask data, calculates the centroid (center of mass)
 * of all mask points, assuming constant density. If multiple masks exist at the
 * same timestamp, each mask gets its own centroid point.
 *
 * @param mask_data The mask data to calculate centroids from
 * @param params The centroid calculation parameters (currently unused but available for future extensions)
 * @return A new PointData containing centroid points at each timestamp
 */
std::shared_ptr<PointData> calculate_mask_centroid(
        MaskData const * mask_data,
        MaskCentroidParameters const * params = nullptr);

/**
 * @brief Calculate the centroid of masks at each timestamp with progress reporting
 *
 * @param mask_data The mask data to calculate centroids from
 * @param params The centroid calculation parameters
 * @param progressCallback Progress reporting callback
 * @return A new PointData containing centroid points at each timestamp
 */
std::shared_ptr<PointData> calculate_mask_centroid(
        MaskData const * mask_data,
        MaskCentroidParameters const * params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class MaskCentroidOperation final : public TransformOperation {
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
     * @return A unique_ptr to default MaskCentroidParameters
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the mask centroid calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @return DataTypeVariant containing a std::shared_ptr<PointData> on success,
     * or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the mask centroid calculation with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @param progressCallback Progress reporting callback
     * @return DataTypeVariant containing a std::shared_ptr<PointData> on success,
     * or an empty variant on failure
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_MASK_CENTROID_HPP