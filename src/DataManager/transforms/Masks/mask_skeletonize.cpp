#include "mask_skeletonize.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Masks/utils/skeletonize.hpp"

#include <algorithm>
#include <cmath>        // std::round
#include <iostream>     // std::cout
#include <vector>

std::shared_ptr<MaskData> skeletonize_mask(
        MaskData const * mask_data,
        MaskSkeletonizeParameters const * params) {
    return skeletonize_mask(mask_data, params, [](int){});
}

std::shared_ptr<MaskData> skeletonize_mask(
        MaskData const * mask_data,
        MaskSkeletonizeParameters const * params,
        ProgressCallback progressCallback) {

    static_cast<void>(params);
    
    if (!mask_data) {
        progressCallback(100);
        return std::make_shared<MaskData>();
    }
    
    // Create binary processing function that uses the skeletonization algorithm
    auto binary_processor = [](Image const & input_image) -> Image {
        return fast_skeletonize(input_image);
    };
    
    // Use the utility function to apply the algorithm
    return apply_binary_image_algorithm(mask_data, binary_processor, progressCallback);
}

///////////////////////////////////////////////////////////////////////////////// MaskSkeletonizeOperation implementation

std::string MaskSkeletonizeOperation::getName() const {
    return "Skeletonize Mask";
}

std::type_index MaskSkeletonizeOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskSkeletonizeOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> MaskSkeletonizeOperation::getDefaultParameters() const {
    return std::make_unique<MaskSkeletonizeParameters>();
}

DataTypeVariant MaskSkeletonizeOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant MaskSkeletonizeOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters,
                                                 ProgressCallback progressCallback) {
    
    auto const * mask_data_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    
    if (!mask_data_ptr || !(*mask_data_ptr)) {
        std::cerr << "MaskSkeletonizeOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    MaskData const * input_mask_data = (*mask_data_ptr).get();
    
    MaskSkeletonizeParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;
    
    if (transformParameters) {
        params = dynamic_cast<MaskSkeletonizeParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "MaskSkeletonizeOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<MaskSkeletonizeParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<MaskSkeletonizeParameters const *>(default_params_owner.get());
    }
    
    if (!params) {
        std::cerr << "MaskSkeletonizeOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    std::shared_ptr<MaskData> result = skeletonize_mask(input_mask_data, params, progressCallback);
    
    if (!result) {
        std::cerr << "MaskSkeletonizeOperation::execute: 'skeletonize_mask' failed to produce a result." << std::endl;
        return {};
    }
    
    std::cout << "MaskSkeletonizeOperation executed successfully." << std::endl;
    return result;
} 
