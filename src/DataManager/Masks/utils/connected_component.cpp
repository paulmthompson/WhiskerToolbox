
#include "connected_component.hpp"

#include <algorithm>
#include <queue>

std::vector<uint8_t> remove_small_clusters(std::vector<uint8_t> const & image, ImageSize const image_size, int threshold) {

    auto const height = image_size.height;
    auto const width = image_size.width;

    std::vector<uint8_t> labeled_image(static_cast<size_t>(height * width), 0);
    std::vector<int> cluster_sizes;
    int current_label = 1;

    auto get_index = [width](size_t row, size_t col) {
        return row * static_cast<size_t>(width) + col;
    };

    for (size_t row = 0; row < static_cast<size_t>(height); ++row) {
        for (size_t col = 0; col < static_cast<size_t>(width); ++col) {
            if (image[get_index(row, col)] && !labeled_image[get_index(row, col)]) {
                std::queue<std::pair<size_t, size_t>> q;
                q.emplace(row, col);
                labeled_image[get_index(row, col)] = static_cast<uint8_t>(current_label);
                int cluster_size = 0;

                while (!q.empty()) {
                    auto [r, c] = q.front();
                    q.pop();
                    ++cluster_size;

                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            auto nr = static_cast<size_t>(static_cast<int>(r) + dr);
                            auto nc = static_cast<size_t>(static_cast<int>(c) + dc);
                            if (nr < static_cast<size_t>(height) && nc < static_cast<size_t>(width) &&
                                image[get_index(nr, nc)] && !labeled_image[get_index(nr, nc)]) {
                                q.emplace(nr, nc);
                                labeled_image[get_index(nr, nc)] = static_cast<uint8_t>(current_label);
                            }
                        }
                    }
                }

                cluster_sizes.push_back(cluster_size);
                ++current_label;
            }
        }
    }

    std::vector<uint8_t> result(static_cast<size_t>(height * width), 0);
    for (size_t row = 0; row < static_cast<size_t>(height); ++row) {
        for (size_t col = 0; col < static_cast<size_t>(width); ++col) {
            int const label = labeled_image[get_index(row, col)];
            if (label && cluster_sizes[static_cast<size_t>(label) - 1] >= threshold) {
                result[get_index(row, col)] = 1;
            }
        }
    }

    return result;
}

Image remove_small_clusters(Image const & input_image, int threshold) {
    // Delegate to the existing function  
    auto result_data = remove_small_clusters(input_image.data, input_image.size, threshold);
    
    // Return as Image struct
    return Image(std::move(result_data), input_image.size);
}
