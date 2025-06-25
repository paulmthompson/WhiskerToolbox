#ifndef WHISKERTOOLBOX_MASK_PRINCIPAL_AXIS_HPP
#define WHISKERTOOLBOX_MASK_PRINCIPAL_AXIS_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class MaskData;
class LineData;

enum class PrincipalAxisType {
    Major,// Largest eigenvalue (direction of maximum variance)
    Minor // Smallest eigenvalue (direction of minimum variance)
};

struct MaskPrincipalAxisParameters : public TransformParametersBase {
    PrincipalAxisType axis_type = PrincipalAxisType::Major;

    // Optional: could add parameters for future extensions
    // bool normalize_to_unit_length = false;
    // float line_extension_factor = 1.0f;  // How far to extend beyond bounding box
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Calculate the principal axis of masks at each timestamp
 *
 * For each timestamp in the mask data, calculates the principal axis (major or minor)
 * of all mask points using eigenvalue decomposition of the covariance matrix.
 * The resulting line passes through the centroid and extends to touch the bounding box.
 * If multiple masks exist at the same timestamp, each mask gets its own principal axis line.
 *
 * @param mask_data The mask data to calculate principal axes from
 * @param params The principal axis calculation parameters
 * @return A new LineData containing principal axis lines at each timestamp
 */
std::shared_ptr<LineData> calculate_mask_principal_axis(
        MaskData const * mask_data,
        MaskPrincipalAxisParameters const * params = nullptr);

/**
 * @brief Calculate the principal axis of masks at each timestamp with progress reporting
 *
 * @param mask_data The mask data to calculate principal axes from
 * @param params The principal axis calculation parameters
 * @param progressCallback Progress reporting callback
 * @return A new LineData containing principal axis lines at each timestamp
 */
std::shared_ptr<LineData> calculate_mask_principal_axis(
        MaskData const * mask_data,
        MaskPrincipalAxisParameters const * params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class MaskPrincipalAxisOperation final : public TransformOperation {
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
     * @return A unique_ptr to default MaskPrincipalAxisParameters
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the mask principal axis calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the mask principal axis calculation with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the MaskData object.
     * @param transformParameters Parameters for the transformation
     * @param progressCallback Progress reporting callback
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_MASK_PRINCIPAL_AXIS_HPP