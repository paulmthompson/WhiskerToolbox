#include "mask_connected_component.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/connected_component.hpp"
#include "Masks/utils/mask_utils.hpp"

#include <iostream>

std::shared_ptr<MaskData> remove_small_connected_components(
        MaskData const * mask_data,
        MaskConnectedComponentParameters const * params) {
    return remove_small_connected_components(mask_data, params, [](int){});
}

std::shared_ptr<MaskData> remove_small_connected_components(
        MaskData const * mask_data,
        MaskConnectedComponentParameters const * params,
        ProgressCallback progressCallback) {

    if (!mask_data) {
        progressCallback(100);
        return std::make_shared<MaskData>();
    }
    
    // Get parameters
    int threshold = 10; // default threshold
    if (params) {
        threshold = params->threshold;
        if (threshold <= 0) {
            std::cerr << "MaskConnectedComponent: threshold must be > 0, using default value 10" << std::endl;
            threshold = 10;
        }
    }
    
    // Create binary processing function that uses the connected component algorithm
    auto binary_processor = [threshold](Image const & input_image) -> Image {
        return remove_small_clusters(input_image, threshold);
    };
    
    // Use the utility function to apply the algorithm
    return apply_binary_image_algorithm(mask_data, binary_processor, progressCallback);
}

///////////////////////////////////////////////////////////////////////////////// 
// MaskConnectedComponentOperation implementation

std::string MaskConnectedComponentOperation::getName() const {
    return "Remove Small Connected Components";
}

std::type_index MaskConnectedComponentOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskConnectedComponentOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> MaskConnectedComponentOperation::getDefaultParameters() const {
    return std::make_unique<MaskConnectedComponentParameters>();
}

DataTypeVariant MaskConnectedComponentOperation::execute(DataTypeVariant const & dataVariant,
                                                        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant MaskConnectedComponentOperation::execute(DataTypeVariant const & dataVariant,
                                                        TransformParametersBase const * transformParameters,
                                                        ProgressCallback progressCallback) {
    
    auto const * mask_data_ptr = std::get_if<std::shared_ptr<MaskData>>(&dataVariant);
    
    if (!mask_data_ptr || !(*mask_data_ptr)) {
        std::cerr << "MaskConnectedComponentOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    MaskData const * input_mask_data = (*mask_data_ptr).get();
    
    MaskConnectedComponentParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;
    
    if (transformParameters) {
        params = dynamic_cast<MaskConnectedComponentParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "MaskConnectedComponentOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = static_cast<MaskConnectedComponentParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = static_cast<MaskConnectedComponentParameters const *>(default_params_owner.get());
    }
    
    if (!params) {
        std::cerr << "MaskConnectedComponentOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }
    
    std::shared_ptr<MaskData> result = remove_small_connected_components(input_mask_data, params, progressCallback);
    
    if (!result) {
        std::cerr << "MaskConnectedComponentOperation::execute: 'remove_small_connected_components' failed to produce a result." << std::endl;
        return {};
    }
    
    std::cout << "MaskConnectedComponentOperation executed successfully." << std::endl;
    return result;
} 