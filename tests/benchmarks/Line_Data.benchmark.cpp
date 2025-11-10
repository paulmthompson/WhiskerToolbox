#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

#include "DataManager/Lines/Line_Data.hpp"

#include <memory>
#include <vector>

// Define a function that creates a sample LineData
static LineData create_test_data(size_t num_times, size_t num_lines_per_time, size_t num_points_per_line) {
    LineData line_data;
    for (size_t t = 0; t < num_times; ++t) {
        for (size_t l = 0; l < num_lines_per_time; ++l) {
            Line2D line;
            for (size_t p = 0; p < num_points_per_line; ++p) {
                line.push_back({static_cast<float>(p), static_cast<float>(p)});
            }
            line_data.addAtTime(TimeFrameIndex(static_cast<int64_t>(t)), line, NotifyObservers::No);
        }
    }
    return std::move(line_data);
}

