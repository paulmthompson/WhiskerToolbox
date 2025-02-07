#ifndef MASK_OPERATIONS_HPP
#define MASK_OPERATIONS_HPP

#include "DataManager/Points/points.hpp"
#include "order_line.hpp"
#include "skeletonize.hpp"
#include "connected_component.hpp"
#include "simplify_line.hpp"


#include <chrono>
#include <iterator>
#include <vector>

std::vector<Point2D<float>> convert_mask_to_line(
    std::vector<uint8_t> mask,
    Point2D<float> base_point,
    const uint8_t mask_threshold = 128)
{

    std::vector<uint8_t> binary_mask;
    std::transform(mask.begin(), mask.end(), std::back_inserter(binary_mask), [mask_threshold](uint8_t pixel) {
        return pixel > mask_threshold ? 1 : 0;
    });

    auto t1 = std::chrono::high_resolution_clock::now();

    auto output_vec = fast_skeletonize(binary_mask, 256, 256);

    auto t2 = std::chrono::high_resolution_clock::now();

    output_vec = remove_small_clusters(output_vec, 256, 256, 10);

    auto output_line = order_line(output_vec, 256, 256, base_point);

    //remove_extreme_angles(output_line, Degree(45.0f));

    auto t3 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t2 - t1;

    return output_line;
}


#endif // MASK_OPERATIONS_HPP
