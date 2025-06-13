#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <ranges>
#include <random>
#include <vector>


TEST_CASE("AnalogTimeSeries - Core functionality", "[analog][timeseries][core]") {

    SECTION("Construction from vector with times") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
        AnalogTimeSeries series(data, times);

        auto const & stored_data = series.getAnalogTimeSeries();
        auto const & time_data = series.getTimeSeries();

        REQUIRE(stored_data.size() == 5);
        REQUIRE(time_data.size() == 5);

        REQUIRE(stored_data == data);
        REQUIRE(time_data == times);
    }

    SECTION("Construction from map") {
        std::map<int, float> data_map{
                {10, 1.0f},
                {20, 2.0f},
                {30, 3.0f},
                {40, 4.0f},
                {50, 5.0f}};

        AnalogTimeSeries series(data_map);

        auto const & stored_data = series.getAnalogTimeSeries();
        auto const & time_data = series.getTimeSeries();

        REQUIRE(stored_data.size() == 5);
        REQUIRE(time_data.size() == 5);

        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
        REQUIRE(time_data == std::vector<TimeFrameIndex>{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)});
    }

    SECTION("Overwrite data at specific TimeFrameIndex values") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
        AnalogTimeSeries series(data, times);

        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<TimeFrameIndex> target_times{TimeFrameIndex(20), TimeFrameIndex(40)};

        series.overwriteAtTimeIndexes(new_data, target_times);

        auto const & stored_data = series.getAnalogTimeSeries();

        REQUIRE(stored_data == std::vector<float>{1.0f, 9.0f, 3.0f, 8.0f, 5.0f});
    }

    SECTION("Overwrite data at specific DataArrayIndex positions") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
        AnalogTimeSeries series(data, times);

        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<DataArrayIndex> target_indices{DataArrayIndex(1), DataArrayIndex(3)};

        series.overwriteAtDataArrayIndexes(new_data, target_indices);

        auto const & stored_data = series.getAnalogTimeSeries();

        REQUIRE(stored_data == std::vector<float>{1.0f, 9.0f, 3.0f, 8.0f, 5.0f});
    }

}

TEST_CASE("AnalogTimeSeries - Edge cases and error handling", "[analog][timeseries][error]") {

    SECTION("Overwriting with non-existent TimeFrameIndex values") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        AnalogTimeSeries series(data, times);

        // Attempt to overwrite at TimeFrameIndex values that don't exist
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<TimeFrameIndex> non_existent_times{TimeFrameIndex(15), TimeFrameIndex(25)};

        // This should not change the original data since the TimeFrameIndex values don't exist
        series.overwriteAtTimeIndexes(new_data, non_existent_times);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }

    SECTION("Overwriting with out-of-bounds DataArrayIndex values") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        AnalogTimeSeries series(data, times);

        // Attempt to overwrite at DataArrayIndex values that are out of bounds
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<DataArrayIndex> out_of_bounds_indices{DataArrayIndex(5), DataArrayIndex(10)};

        // This should not change the original data since the indices are out of bounds
        series.overwriteAtDataArrayIndexes(new_data, out_of_bounds_indices);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }

    SECTION("Overwriting with mismatched vector sizes - TimeFrameIndex") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        AnalogTimeSeries series(data, times);

        // Create vectors with mismatched sizes
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<TimeFrameIndex> mismatched_times{TimeFrameIndex(10)};

        // This should not change the original data due to size mismatch
        series.overwriteAtTimeIndexes(new_data, mismatched_times);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }

    SECTION("Overwriting with mismatched vector sizes - DataArrayIndex") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        AnalogTimeSeries series(data, times);

        // Create vectors with mismatched sizes
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<DataArrayIndex> mismatched_indices{DataArrayIndex(1)};

        // This should not change the original data due to size mismatch
        series.overwriteAtDataArrayIndexes(new_data, mismatched_indices);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }

    SECTION("Dense time range - overwrite by TimeFrameIndex") {
        // Test with dense time storage (consecutive indices starting from 0)
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, data.size()); // This should create dense storage

        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<TimeFrameIndex> target_times{TimeFrameIndex(1), TimeFrameIndex(3)};

        series.overwriteAtTimeIndexes(new_data, target_times);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 9.0f, 3.0f, 8.0f, 5.0f});
    }
}
