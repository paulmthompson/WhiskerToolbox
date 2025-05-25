#ifndef MASK_SKELETONIZE_HPP
#define MASK_SKELETONIZE_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class MaskData;

struct MaskSkeletonizeParameters : public TransformParametersBase {
    // No additional parameters needed for basic skeletonization
    // The skeletonization algorithm is parameter-free
    
    // Optional: could add parameters for post-processing in the future
    // bool remove_isolated_pixels = false;
    // int min_branch_length = 0;
};

/**
 * @brief Skeletonize mask data by converting to binary image, applying skeletonization, and converting back
 * @param mask_data The MaskData to skeletonize
 * @param params The skeletonization parameters (currently unused but available for future extensions)
 * @return A new MaskData containing the skeletonized mask points
 */
std::shared_ptr<MaskData> skeletonize_mask(
        MaskData const * mask_data,
        MaskSkeletonizeParameters const * params = nullptr);

/**
 * @brief Skeletonize mask data with progress reporting
 * @param mask_data The MaskData to skeletonize
 * @param params The skeletonization parameters
 * @param progressCallback Progress reporting callback
 * @return A new MaskData containing the skeletonized mask points
 */
std::shared_ptr<MaskData> skeletonize_mask(
        MaskData const * mask_data,
        MaskSkeletonizeParameters const * params,
        ProgressCallback progressCallback);

class MaskSkeletonizeOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;
                           
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif // MASK_SKELETONIZE_HPP 