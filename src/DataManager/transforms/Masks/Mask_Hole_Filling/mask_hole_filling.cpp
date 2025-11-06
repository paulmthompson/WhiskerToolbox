#include "mask_hole_filling.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/hole_filling.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <iostream>

std::shared_ptr<MaskData> fill_mask_holes(
        MaskData const * mask_data,
        MaskHoleFillingParameters const * params) {
    return fill_mask_holes(mask_data, params, [](int){});
}

std::shared_ptr<MaskData> fill_mask_holes(
        MaskData const * mask_data,
        MaskHoleFillingParameters const * params,
        ProgressCallback progressCallback) {

    if (!mask_data) {
        progressCallback(100);
        return std::make_shared<MaskData>();
    }
    
    // Parameters are not used for hole filling, but we keep them for consistency
    static_cast<void>(params);
    
    // Use the utility function to apply hole filling to the mask data
    auto binary_processor = [](Image const & binary_image) -> Image {
        return fill_holes(binary_image);
    };
    
    return apply_binary_image_algorithm(
        mask_data,
        binary_processor,
        progressCallback,
        true // preserve_empty_masks - keep frames even if they become empty
    );
}

///////////////////////////////////////////////////////////////////////////////

std::string MaskHoleFillingOperation::getName() const {
    return "Fill Mask Holes";
}

std::type_index MaskHoleFillingOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskHoleFillingOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> MaskHoleFillingOperation::getDefaultParameters() const {
    return std::make_unique<MaskHoleFillingParameters>();
}

DataTypeVariant MaskHoleFillingOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant MaskHoleFillingOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters,
                                                 ProgressCallback progressCallback) {
    // Check if the variant holds the expected type
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        std::cerr << "MaskHoleFillingOperation: Input data variant does not hold MaskData" << std::endl;
        progressCallback(100);
        return DataTypeVariant{};
    }
    
    // Extract the shared_ptr<MaskData> from the variant
    auto mask_data = std::get<std::shared_ptr<MaskData>>(dataVariant);
    if (!mask_data) {
        std::cerr << "MaskHoleFillingOperation: Input MaskData is null" << std::endl;
        progressCallback(100);
        return DataTypeVariant{};
    }
    
    // Cast parameters to the correct type
    auto hole_filling_params = dynamic_cast<MaskHoleFillingParameters const *>(transformParameters);
    if (!hole_filling_params) {
        std::cerr << "MaskHoleFillingOperation: Invalid parameter type" << std::endl;
        progressCallback(100);
        return DataTypeVariant{};
    }
    
    // Execute the transformation
    auto result = fill_mask_holes(mask_data.get(), hole_filling_params, progressCallback);
    
    // Return the result wrapped in a DataTypeVariant
    return DataTypeVariant{result};
} 