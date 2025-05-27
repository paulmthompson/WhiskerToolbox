#include "mask_skeletonize.hpp"

#include "Masks/Mask_Data.hpp"
#include "utils/skeletonize.hpp"

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
    
    auto result_mask_data = std::make_shared<MaskData>();
    
    if (!mask_data) {
        progressCallback(100);
        return result_mask_data;
    }
    
    // Copy image size from input
    result_mask_data->setImageSize(mask_data->getImageSize());
    
    // Get image dimensions
    auto image_size = mask_data->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        // Use default size if mask data doesn't specify
        image_size.width = 256;
        image_size.height = 256;
        result_mask_data->setImageSize(image_size);
    }
    
    // Count total masks to process for progress calculation
    size_t total_masks = 0;
    for (auto const & mask_time_pair : mask_data->getAllAsRange()) {
        if (!mask_time_pair.masks.empty()) {
            total_masks += mask_time_pair.masks.size();
        }
    }
    
    if (total_masks == 0) {
        progressCallback(100);
        return result_mask_data;
    }
    
    progressCallback(0);
    
    // Create binary image buffer (reused for each mask)
    std::vector<uint8_t> binary_image(image_size.width * image_size.height);
    
    size_t processed_masks = 0;
    
    for (auto const & mask_time_pair : mask_data->getAllAsRange()) {
        int time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;
        
        for (auto const & mask : masks) {
            if (mask.empty()) {
                processed_masks++;
                continue;
            }
            
            // Clear the binary image
            std::fill(binary_image.begin(), binary_image.end(), 0);
            
            // Convert mask points to binary image
            for (auto const & point : mask) {
                int x = static_cast<int>(std::round(point.x));
                int y = static_cast<int>(std::round(point.y));
                
                // Check bounds
                if (x >= 0 && x < image_size.width && 
                    y >= 0 && y < image_size.height) {
                    binary_image[y * image_size.width + x] = 1;
                }
            }
            
            // Apply skeletonization
            auto skeleton = fast_skeletonize(binary_image, image_size.height, image_size.width);
            
            // Convert skeleton back to mask points
            std::vector<Point2D<float>> skeleton_points;
            for (int y = 0; y < image_size.height; ++y) {
                for (int x = 0; x < image_size.width; ++x) {
                    if (skeleton[y * image_size.width + x] > 0) {
                        skeleton_points.push_back({static_cast<float>(x), static_cast<float>(y)});
                    }
                }
            }
            
            // Add the skeletonized mask to result (only if it has points)
            if (!skeleton_points.empty()) {
                result_mask_data->addAtTime(time, skeleton_points, false);
            }
            
            processed_masks++;
            
            // Update progress
            int progress = static_cast<int>(
                std::round(static_cast<double>(processed_masks) / total_masks * 100.0)
            );
            progressCallback(progress);
        }
    }
    
    progressCallback(100);
    return result_mask_data;
}

// MaskSkeletonizeOperation implementation

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
