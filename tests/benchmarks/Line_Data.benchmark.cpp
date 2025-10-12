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
            line_data.addAtTime(TimeFrameIndex(static_cast<int64_t>(t)), line);
        }
    }
    return line_data;
}

TEST_CASE("Benchmark LineData Copy and Move", "[!benchmark]") {
    auto line_data_small = create_test_data(10, 10, 10);
    auto line_data_medium = create_test_data(100, 100, 10);
    auto line_data_large = create_test_data(1000, 100, 10);

    BENCHMARK("Copy small LineData") {
        LineData target;
        line_data_small.copyTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(9)});
        return target;
    };

    BENCHMARK("Move small LineData") {
        LineData source = line_data_small;
        LineData target;
        source.moveTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(9)});
        return target;
    };

    BENCHMARK("Copy medium LineData") {
        LineData target;
        line_data_medium.copyTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(99)});
        return target;
    };

    BENCHMARK("Move medium LineData") {
        LineData source = line_data_medium;
        LineData target;
        source.moveTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(99)});
        return target;
    };

    BENCHMARK("Copy large LineData") {
        LineData target;
        line_data_large.copyTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(999)});
        return target;
    };

    BENCHMARK("Move large LineData") {
        LineData source = line_data_large;
        LineData target;
        source.moveTo(target, TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(999)});
        return target;
    };
}
