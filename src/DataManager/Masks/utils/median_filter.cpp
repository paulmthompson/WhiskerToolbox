#include "median_filter.hpp"

#include <algorithm>
#include <vector>

namespace {
    /**
     * @brief Helper function to get pixel value with reflection padding
     * 
     * @param image The input image data
     * @param width Image width
     * @param height Image height
     * @param row Row coordinate (can be negative or >= height)
     * @param col Column coordinate (can be negative or >= width)
     * @return uint8_t Pixel value with reflection padding applied
     */
    uint8_t get_pixel_with_padding(std::vector<uint8_t> const & image, int width, int height, int row, int col) {
        // Apply reflection padding
        if (row < 0) row = -row - 1;
        if (row >= height) row = 2 * height - row - 1;
        if (col < 0) col = -col - 1;
        if (col >= width) col = 2 * width - col - 1;
        
        // Clamp to valid bounds (in case of very small images)
        row = std::max(0, std::min(row, height - 1));
        col = std::max(0, std::min(col, width - 1));
        
        return image[static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col)];
    }
    
    /**
     * @brief Helper function to convert input to binary (0 or 1)
     * 
     * @param value Input pixel value
     * @return uint8_t 0 if input is 0, 1 otherwise
     */
    uint8_t normalize_binary(uint8_t value) {
        return value > 0 ? 1 : 0;
    }
}

std::vector<uint8_t> median_filter(std::vector<uint8_t> const & image, ImageSize const image_size, int window_size) {
    auto const height = image_size.height;
    auto const width = image_size.width;
    
    // Validate inputs
    if (width <= 0 || height <= 0) {
        return {};
    }
    
    if (window_size <= 0 || window_size % 2 == 0) {
        // Invalid window size, return normalized input
        std::vector<uint8_t> result(image.size());
        std::transform(image.begin(), image.end(), result.begin(), normalize_binary);
        return result;
    }
    
    if (static_cast<size_t>(width * height) != image.size()) {
        return {};
    }
    
    std::vector<uint8_t> result(static_cast<size_t>(height * width));
    int const half_window = window_size / 2;
    
    // For each pixel in the output image
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            
            // Collect pixels in the window
            std::vector<uint8_t> window_pixels;
            window_pixels.reserve(static_cast<size_t>(window_size * window_size));
            
            for (int wr = -half_window; wr <= half_window; ++wr) {
                for (int wc = -half_window; wc <= half_window; ++wc) {
                    uint8_t pixel = get_pixel_with_padding(image, width, height, row + wr, col + wc);
                    window_pixels.push_back(normalize_binary(pixel));
                }
            }
            
            // Find median
            std::sort(window_pixels.begin(), window_pixels.end());
            size_t const median_index = window_pixels.size() / 2;
            result[static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col)] = window_pixels[median_index];
        }
    }
    
    return result;
}

Image median_filter(Image const & input_image, int window_size) {
    // Delegate to the existing function  
    auto result_data = median_filter(input_image.data, input_image.size, window_size);
    
    // Return as Image struct
    return Image(std::move(result_data), input_image.size);
} 