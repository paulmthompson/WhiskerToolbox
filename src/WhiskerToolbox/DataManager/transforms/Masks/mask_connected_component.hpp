#ifndef MASK_CONNECTED_COMPONENT_HPP
#define MASK_CONNECTED_COMPONENT_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class MaskData;

struct MaskConnectedComponentParameters : public TransformParametersBase {
    /**
     * @brief Minimum size (in pixels) for a connected component to be preserved
     * 
     * Connected components smaller than this threshold will be removed from the mask.
     * Must be greater than 0.
     */
    int threshold = 10;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Remove small connected components from mask data
 * 
 * This function applies connected component analysis to remove small isolated
 * regions from masks. Uses 8-connectivity (considers diagonal neighbors as connected).
 * 
 * @param mask_data The MaskData to process
 * @param params The connected component parameters
 * @return A new MaskData with small connected components removed
 */
std::shared_ptr<MaskData> remove_small_connected_components(
        MaskData const * mask_data,
        MaskConnectedComponentParameters const * params = nullptr);

/**
 * @brief Remove small connected components from mask data with progress reporting
 * 
 * @param mask_data The MaskData to process
 * @param params The connected component parameters
 * @param progressCallback Progress reporting callback
 * @return A new MaskData with small connected components removed
 */
std::shared_ptr<MaskData> remove_small_connected_components(
        MaskData const * mask_data,
        MaskConnectedComponentParameters const * params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class MaskConnectedComponentOperation final : public TransformOperation {
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

#endif // MASK_CONNECTED_COMPONENT_HPP 