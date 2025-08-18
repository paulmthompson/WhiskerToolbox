
#include "connected_component.hpp"

#include <algorithm>
#include <queue>

std::vector<uint8_t> remove_small_clusters(std::vector<uint8_t> const & image, ImageSize const image_size, int threshold) {

    auto const height = image_size.height;
    auto const width = image_size.width;

    std::vector<uint8_t> labeled_image(static_cast<size_t>(height * width), 0);
    std::vector<int> cluster_sizes;
    int current_label = 1;

    auto get_index = [width](int row, int col) {
        return row * width + col;
    };

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (image[get_index(row, col)] && !labeled_image[get_index(row, col)]) {
                std::queue<std::pair<int, int>> q;
                q.emplace(row, col);
                labeled_image[get_index(row, col)] = current_label;
                int cluster_size = 0;

                while (!q.empty()) {
                    auto [r, c] = q.front();
                    q.pop();
                    ++cluster_size;

                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            int const nr = r + dr;
                            int const nc = c + dc;
                            if (nr >= 0 && nr < height && nc >= 0 && nc < width &&
                                image[get_index(nr, nc)] && !labeled_image[get_index(nr, nc)]) {
                                q.emplace(nr, nc);
                                labeled_image[get_index(nr, nc)] = current_label;
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
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int const label = labeled_image[get_index(row, col)];
            if (label && cluster_sizes[label - 1] >= threshold) {
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
