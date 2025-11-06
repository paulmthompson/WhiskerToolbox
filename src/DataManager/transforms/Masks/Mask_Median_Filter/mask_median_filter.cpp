#include "mask_median_filter.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Masks/utils/median_filter.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <iostream>

std::shared_ptr<MaskData> apply_median_filter(
        MaskData const * mask_data,
        MaskMedianFilterParameters const * params) {
    return apply_median_filter(mask_data, params, [](int){});
}

std::shared_ptr<MaskData> apply_median_filter(
        MaskData const * mask_data,
        MaskMedianFilterParameters const * params,
        ProgressCallback progressCallback) {

    if (!mask_data) {
        progressCallback(100);
        return std::make_shared<MaskData>();
    }
    
    // Use default window size if no parameters provided
    int window_size = 3;
    if (params) {
        window_size = params->window_size;
        
        // Validate window size
        if (window_size <= 0 || window_size % 2 == 0) {
            std::cerr << "MaskMedianFilter: Invalid window size " << window_size 
                      << ". Using default window size of 3." << std::endl;
            window_size = 3;
        }
    }
    
    // Create binary processing function that uses the median filter algorithm
    auto binary_processor = [window_size](Image const & input_image) -> Image {
        return median_filter(input_image, window_size);
    };
    
    // Use the utility function to apply the algorithm
    return apply_binary_image_algorithm(
        mask_data,
        binary_processor,
        progressCallback
        // Don't preserve empty masks - if median filtering removes all pixels, 
        // the mask should be removed from the result
    );
}

///////////////////////////////////////////////////////////////////////////////

std::string MaskMedianFilterOperation::getName() const {
    return "Apply Median Filter";
}

std::type_index MaskMedianFilterOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MaskData>);
}

bool MaskMedianFilterOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<MaskData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> MaskMedianFilterOperation::getDefaultParameters() const {
    return std::make_unique<MaskMedianFilterParameters>();
}

DataTypeVariant MaskMedianFilterOperation::execute(DataTypeVariant const & dataVariant,
                                                   TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant MaskMedianFilterOperation::execute(DataTypeVariant const & dataVariant,
                                                   TransformParametersBase const * transformParameters,
                                                   ProgressCallback progressCallback) {
    
    // Check if the variant holds the expected type
    if (!std::holds_alternative<std::shared_ptr<MaskData>>(dataVariant)) {
        std::cerr << "MaskMedianFilterOperation: Input data variant does not hold MaskData" << std::endl;
        progressCallback(100);
        return DataTypeVariant{};
    }
    
    // Extract the shared_ptr<MaskData> from the variant
    auto mask_data = std::get<std::shared_ptr<MaskData>>(dataVariant);
    if (!mask_data) {
        std::cerr << "MaskMedianFilterOperation: Input MaskData is null" << std::endl;
        progressCallback(100);
        return DataTypeVariant{};
    }
    
    // Cast parameters to the correct type
    auto median_filter_params = dynamic_cast<MaskMedianFilterParameters const *>(transformParameters);
    if (!median_filter_params) {
        std::cerr << "MaskMedianFilterOperation: Invalid parameter type. Using defaults." << std::endl;
        
        // Use default parameters
        MaskMedianFilterParameters default_params;
        auto result = apply_median_filter(mask_data.get(), &default_params, progressCallback);
        return DataTypeVariant{result};
    }
    
    // Execute the transformation
    auto result = apply_median_filter(mask_data.get(), median_filter_params, progressCallback);
    
    // Return the result wrapped in a DataTypeVariant
    return DataTypeVariant{result};
} 