#ifndef SCATTER_POINT_DATA_HPP
#define SCATTER_POINT_DATA_HPP

/**
 * @file ScatterPointData.hpp
 * @brief Data structure for computed scatter plot point pairs
 *
 * ScatterPointData holds the result of joining two axis sources into
 * matched (x, y) pairs. Each point retains its TimeFrameIndex for
 * click-to-navigate and group integration in later phases.
 *
 * @see buildScatterPoints() for the factory function
 * @see ScatterAxisSource for the source descriptor
 */

#include "TimeFrame/TimeFrame.hpp"

#include <vector>

/**
 * @brief Holds computed scatter point data from two axis sources
 *
 * Parallel arrays: x_values[i], y_values[i], time_indices[i] describe
 * the i-th scatter point.
 */
struct ScatterPointData {
    std::vector<float> x_values;
    std::vector<float> y_values;
    std::vector<TimeFrameIndex> time_indices;  ///< For click-to-navigate and group integration

    [[nodiscard]] std::size_t size() const { return x_values.size(); }
    [[nodiscard]] bool empty() const { return x_values.empty(); }

    void clear() {
        x_values.clear();
        y_values.clear();
        time_indices.clear();
    }
};

#endif  // SCATTER_POINT_DATA_HPP
