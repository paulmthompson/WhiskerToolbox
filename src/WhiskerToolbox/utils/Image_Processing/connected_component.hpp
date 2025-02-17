#ifndef WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
#define WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP

#include <vector>
#include <queue>
#include <algorithm>

std::vector<uint8_t> remove_small_clusters(const std::vector<uint8_t>& image, int height, int width, int threshold) {
    std::vector<uint8_t> labeled_image(height * width, 0);
    std::vector<int> cluster_sizes;
    int current_label = 1;

    auto get_index = [width](int row, int col) {
        return row * width + col;
    };

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (image[get_index(row, col)] && !labeled_image[get_index(row, col)]) {
                std::queue<std::pair<int, int>> q;
                q.push({row, col});
                labeled_image[get_index(row, col)] = current_label;
                int cluster_size = 0;

                while (!q.empty()) {
                    auto [r, c] = q.front();
                    q.pop();
                    ++cluster_size;

                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            int nr = r + dr;
                            int nc = c + dc;
                            if (nr >= 0 && nr < height && nc >= 0 && nc < width &&
                                image[get_index(nr, nc)] && !labeled_image[get_index(nr, nc)]) {
                                q.push({nr, nc});
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

    std::vector<uint8_t> result(height * width, 0);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int label = labeled_image[get_index(row, col)];
            if (label && cluster_sizes[label - 1] >= threshold) {
                result[get_index(row, col)] = 1;
            }
        }
    }

    return result;
}


#endif //WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
