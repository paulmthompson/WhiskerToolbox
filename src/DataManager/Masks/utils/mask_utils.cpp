#include "mask_utils.hpp"

#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <omp.h>



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
    
    // Collect all masks and their metadata for processing
    struct MaskJob {
        std::vector<Point2D<uint32_t>> mask;
        TimeFrameIndex time;
        size_t original_index;
        bool is_empty;

        // Constructor to help with initialization
        MaskJob(std::vector<Point2D<uint32_t>> const& m, TimeFrameIndex t, size_t idx, bool empty)
            : mask(m), time(t), original_index(idx), is_empty(empty) {}
    };

    std::vector<MaskJob> mask_jobs;

    for (auto const & mask_time_pair : mask_data->getAllAsRange()) {
        auto time = mask_time_pair.time;
        auto const & masks = mask_time_pair.masks;

        for (auto const & mask : masks) {
            mask_jobs.emplace_back(mask, time, mask_jobs.size(), mask.empty());
        }
    }
    
    if (mask_jobs.empty()) {
        progress_callback(100);
        return result_mask_data;
    }
    
    progress_callback(0);
    
    // Prepare results vector to store processed masks
    std::vector<std::vector<Point2D<uint32_t>>> processed_masks(mask_jobs.size());
    std::vector<bool> should_add_mask(mask_jobs.size(), false);

    // Process masks in parallel using OpenMP
    // Each thread processes different masks independently
    #pragma omp parallel for schedule(dynamic, 4) default(none) \
        shared(mask_jobs, processed_masks, should_add_mask, binary_processor, image_size, preserve_empty_masks)
    for (size_t i = 0; i < mask_jobs.size(); ++i) {
        auto const & job = mask_jobs[i];

        if (job.is_empty) {
            if (preserve_empty_masks) {
                should_add_mask[i] = true;
                processed_masks[i] = std::vector<Point2D<uint32_t>>{};
            }
        } else {
            // Convert mask to binary image
            Image binary_image = mask_to_binary_image(job.mask, image_size);

            // Apply the binary processing algorithm
            Image processed_image = binary_processor(binary_image);
            
            // Convert processed image back to mask points
            std::vector<Point2D<uint32_t>> processed_points = binary_image_to_mask(processed_image);
            
            // Store result if it has points
            if (!processed_points.empty()) {
                should_add_mask[i] = true;
                processed_masks[i] = std::move(processed_points);
            }
        }
    }

    // Add processed masks to result in original order
    // This must be done sequentially to maintain thread safety
    for (size_t i = 0; i < mask_jobs.size(); ++i) {
        if (should_add_mask[i]) {
            result_mask_data->addAtTime(mask_jobs[i].time, processed_masks[i], false);
        }

        // Update progress
        int progress = static_cast<int>(
            std::round(static_cast<double>(i + 1) / mask_jobs.size() * 100.0)
        );
        progress_callback(progress);
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

Mask2D resize_mask(Mask2D const & mask, ImageSize const & source_size, ImageSize const & dest_size) {
    // Validate input parameters
    if (mask.empty() || source_size.width <= 0 || source_size.height <= 0 ||
        dest_size.width <= 0 || dest_size.height <= 0) {
        return {};
    }

    // If sizes are the same, return a copy of the original mask
    if (source_size.width == dest_size.width && source_size.height == dest_size.height) {
        return mask;
    }

    // Create a binary image from the mask
    std::vector<uint8_t> source_image(source_size.width * source_size.height, 0);

    // Set mask pixels to 1
    for (auto const & point: mask) {
        if (point.x < static_cast<uint32_t>(source_size.width) &&
            point.y < static_cast<uint32_t>(source_size.height)) {
            source_image[point.y * source_size.width + point.x] = 1;
        }
    }

    // Create the destination image
    std::vector<uint8_t> dest_image(dest_size.width * dest_size.height, 0);

    // Calculate scaling factors
    double x_scale = static_cast<double>(source_size.width) / dest_size.width;
    double y_scale = static_cast<double>(source_size.height) / dest_size.height;

    // Perform nearest neighbor interpolation
    for (int dest_y = 0; dest_y < dest_size.height; ++dest_y) {
        for (int dest_x = 0; dest_x < dest_size.width; ++dest_x) {
            // Find the nearest source pixel using OpenCV-like mapping
            // OpenCV uses: src_x = (dest_x + 0.5) * scale - 0.5
            // But for integer coordinates, we can simplify to: src_x = dest_x * scale
            int source_x = static_cast<int>((dest_x + 0.5) * x_scale - 0.5);
            int source_y = static_cast<int>((dest_y + 0.5) * y_scale - 0.5);

            // Clamp to source bounds
            source_x = std::max(0, std::min(source_x, source_size.width - 1));
            source_y = std::max(0, std::min(source_y, source_size.height - 1));

            // Copy the pixel value
            dest_image[dest_y * dest_size.width + dest_x] = 
                source_image[source_y * source_size.width + source_x];
        }
    }

    // Extract the new mask points from the resized binary image
    Mask2D resized_mask;
    for (int y = 0; y < dest_size.height; ++y) {
        for (int x = 0; x < dest_size.width; ++x) {
            if (dest_image[y * dest_size.width + x] > 0) {
                resized_mask.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
            }
        }
    }

    return resized_mask;
}
