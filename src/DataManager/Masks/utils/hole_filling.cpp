#include "hole_filling.hpp"

#include <algorithm>
#include <queue>

std::vector<uint8_t> fill_holes(std::vector<uint8_t> const & image, ImageSize const image_size) {
    auto const height = image_size.height;
    auto const width = image_size.width;
    
    if (height <= 0 || width <= 0) {
        return {};
    }
    
    // Create result image - start with a copy of the input, normalized to 0/1
    std::vector<uint8_t> result(static_cast<size_t>(height * width));
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = (image[i] > 0) ? 1 : 0;
    }
    
    // Create temporary image for flood fill - inverted version of input
    std::vector<uint8_t> temp_image(static_cast<size_t>(height * width));
    for (size_t i = 0; i < temp_image.size(); ++i) {
        temp_image[i] = (image[i] > 0) ? 0 : 1;  // Invert: background becomes 1, foreground becomes 0
    }
    
    // Helper function to get linear index from row, col
    auto get_index = [width](int row, int col) {
        return row * width + col;
    };
    
    // Helper function to check if coordinates are valid
    auto is_valid = [height, width](int row, int col) {
        return row >= 0 && row < height && col >= 0 && col < width;
    };
    
    // Flood fill from all boundary pixels to mark background regions connected to borders
    std::queue<std::pair<int, int>> fill_queue;
    std::vector<bool> visited(static_cast<size_t>(height * width), false);
    
    // Add all boundary pixels that are background (value 1 in inverted image) to the queue
    // Top and bottom rows
    for (int col = 0; col < width; ++col) {
        // Top row
        if (temp_image[get_index(0, col)] == 1 && !visited[get_index(0, col)]) {
            fill_queue.emplace(0, col);
            visited[get_index(0, col)] = true;
        }
        // Bottom row
        if (temp_image[get_index(height - 1, col)] == 1 && !visited[get_index(height - 1, col)]) {
            fill_queue.emplace(height - 1, col);
            visited[get_index(height - 1, col)] = true;
        }
    }
    
    // Left and right columns (excluding corners already processed)
    for (int row = 1; row < height - 1; ++row) {
        // Left column
        if (temp_image[get_index(row, 0)] == 1 && !visited[get_index(row, 0)]) {
            fill_queue.emplace(row, 0);
            visited[get_index(row, 0)] = true;
        }
        // Right column
        if (temp_image[get_index(row, width - 1)] == 1 && !visited[get_index(row, width - 1)]) {
            fill_queue.emplace(row, width - 1);
            visited[get_index(row, width - 1)] = true;
        }
    }
    
    // 4-connectivity directions (up, down, left, right)
    int const dr[] = {-1, 1, 0, 0};
    int const dc[] = {0, 0, -1, 1};
    
    // Perform flood fill to mark all background pixels connected to boundaries
    while (!fill_queue.empty()) {
        auto [row, col] = fill_queue.front();
        fill_queue.pop();
        
        // Check all 4 neighbors
        for (int i = 0; i < 4; ++i) {
            int const new_row = row + dr[i];
            int const new_col = col + dc[i];
            
            if (is_valid(new_row, new_col)) {
                int const index = get_index(new_row, new_col);
                if (temp_image[index] == 1 && !visited[index]) {
                    visited[index] = true;
                    fill_queue.emplace(new_row, new_col);
                }
            }
        }
    }
    
    // Fill holes: any background pixel (0 in original) that was NOT visited is a hole
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int const index = get_index(row, col);
            // If this was originally background (0) but not connected to boundary, it's a hole
            if (image[index] == 0 && !visited[index]) {
                result[index] = 1;  // Fill the hole
            }
        }
    }
    
    return result;
}

Image fill_holes(Image const & input_image) {
    // Delegate to the existing function
    auto result_data = fill_holes(input_image.data, input_image.size);
    
    // Return as Image struct
    return Image(std::move(result_data), input_image.size);
} 