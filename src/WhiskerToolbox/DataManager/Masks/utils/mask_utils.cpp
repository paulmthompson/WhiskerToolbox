#include "mask_utils.hpp"

#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

std::shared_ptr<MaskData> apply_binary_image_algorithm(
    MaskData const * mask_data,
    std::function<Image(Image const &)> binary_processor,
    std::function<void(int)> progress_callback,
    bool preserve_empty_masks) {
    
    auto result_mask_data = std::make_shared<MaskData>();
    
    if (!mask_data) {
        progress_callback(100);
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
        progress_callback(100);
        return result_mask_data;
    }
    
    progress_callback(0);
    
    size_t processed_masks = 0;
    
    for (auto const & mask_time_pair : mask_data->getAllAsRange()) {
        auto time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;
        
        for (auto const & mask : masks) {
            if (mask.empty()) {
                if (preserve_empty_masks) {
                    result_mask_data->addAtTime(time, std::vector<Point2D<uint32_t>>{}, false);
                }
                processed_masks++;
                continue;
            }
            
            // Convert mask to binary image
            Image binary_image = mask_to_binary_image(mask, image_size);
            
            // Apply the binary processing algorithm
            Image processed_image = binary_processor(binary_image);
            
            // Convert processed image back to mask points
            std::vector<Point2D<uint32_t>> processed_points = binary_image_to_mask(processed_image);
            
            // Add the processed mask to result (only if it has points)
            if (!processed_points.empty()) {
                result_mask_data->addAtTime(time, processed_points, false);
            }
            
            processed_masks++;
            
            // Update progress
            int progress = static_cast<int>(
                std::round(static_cast<double>(processed_masks) / total_masks * 100.0)
            );
            progress_callback(progress);
        }
    }
    
    progress_callback(100);
    return result_mask_data;
}

Image mask_to_binary_image(std::vector<Point2D<uint32_t>> const & mask, ImageSize image_size) {
    std::vector<uint8_t> image_data(image_size.width * image_size.height, 0);
    
    // Set mask points to 1 in the binary image
    for (auto const & point : mask) {
        int x = static_cast<int>(point.x);
        int y = static_cast<int>(point.y);
        
        // Check bounds
        if (x >= 0 && x < image_size.width && 
            y >= 0 && y < image_size.height) {
            image_data[y * image_size.width + x] = 1;
        }
    }
    
    return Image(std::move(image_data), image_size);
}

std::vector<Point2D<uint32_t>> binary_image_to_mask(Image const & binary_image) {
    std::vector<Point2D<uint32_t>> mask_points;
    
    for (int y = 0; y < binary_image.size.height; ++y) {
        for (int x = 0; x < binary_image.size.width; ++x) {
            if (binary_image.at(y, x) > 0) {
                mask_points.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
            }
        }
    }
    
    return mask_points;
} 