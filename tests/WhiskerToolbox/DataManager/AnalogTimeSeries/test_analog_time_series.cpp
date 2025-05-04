#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <vector>


TEST_CASE("AnalogTimeSeries - Core functionality", "[analog][timeseries][core]") {
    SECTION("Construction from vector") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data);

        auto const & stored_data = series.getAnalogTimeSeries();
        auto const & time_data = series.getTimeSeries();

        REQUIRE(stored_data.size() == 5);
        REQUIRE(time_data.size() == 5);

        REQUIRE(stored_data == data);
        REQUIRE(time_data == std::vector<size_t>{0, 1, 2, 3, 4});
    }

    SECTION("Construction from vector with times") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<size_t> times{10, 20, 30, 40, 50};
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
                {50, 5.0f}
        };

        AnalogTimeSeries series(data_map);

        auto const & stored_data = series.getAnalogTimeSeries();
        auto const & time_data = series.getTimeSeries();

        REQUIRE(stored_data.size() == 5);
        REQUIRE(time_data.size() == 5);

        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
        REQUIRE(time_data == std::vector<size_t>{10, 20, 30, 40, 50});
    }

    SECTION("Statistical calculations") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data);

        REQUIRE_THAT(calculate_mean(series) , Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series) , Catch::Matchers::WithinRel(1.41421f, 1e-3f)); // 1.41421 for N, 1.5811 for N-1 denom
        REQUIRE(series.getMinValue() == 1.0f);
        REQUIRE(calculate_max(series) == 5.0f);

        // Test with range. 1 to 4 will reduce the data to {2.0f, 3.0f, 4.0f}
        REQUIRE_THAT(calculate_mean(series, 1, 4) , Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series, 1, 4) , Catch::Matchers::WithinRel(0.8165f, 1e-3f)); // 0.8165 for N, 1.0 for N-1 denom
        REQUIRE(series.getMinValue(1, 4) == 2.0f);
        REQUIRE(calculate_max(series, 1, 4) == 4.0f);
    }

    SECTION("Data ranges") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<size_t> times{10, 20, 30, 40, 50};
        AnalogTimeSeries series(data, times);

        auto [filtered_times, filtered_values] = series.getDataVectorsInRange(20, 40);

        REQUIRE(filtered_times.size() == 3);
        REQUIRE(filtered_values.size() == 3);
        REQUIRE(filtered_times == std::vector<size_t>{20, 30, 40});
        REQUIRE(filtered_values == std::vector<float>{2.0f, 3.0f, 4.0f});

        // Test with transform function
        auto [transformed_times, transformed_values] = series.getDataVectorsInRange(2, 4,
                                                                                    [](size_t t) { return static_cast<float>(t) / 10.0f; });

        REQUIRE(transformed_times.size() == 3);
        REQUIRE(transformed_values.size() == 3);
        REQUIRE(transformed_times == std::vector<size_t>{20, 30, 40});
        REQUIRE(transformed_values == std::vector<float>{2.0f, 3.0f, 4.0f});
    }

    SECTION("Overwrite data at specific times") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<size_t> times{10, 20, 30, 40, 50};
        AnalogTimeSeries series(data, times);

        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<size_t> new_times{20, 40};

        series.overwriteAtTimes(new_data, new_times);

        auto const & stored_data = series.getAnalogTimeSeries();

        REQUIRE(stored_data == std::vector<float>{1.0f, 9.0f, 3.0f, 8.0f, 5.0f});
    }
}

TEST_CASE("AnalogTimeSeries - Edge cases and error handling", "[analog][timeseries][error]") {
    SECTION("Empty data") {
        // Create with empty vector
        AnalogTimeSeries series(std::vector<float>{});

        REQUIRE(series.getAnalogTimeSeries().empty());
        REQUIRE(series.getTimeSeries().empty());
    }

    SECTION("Single data point") {
        std::vector<float> data{42.0f};
        AnalogTimeSeries series(data);

        REQUIRE(calculate_mean(series) == 42.0f);
        REQUIRE(series.getMinValue() == 42.0f);
        REQUIRE(calculate_max(series) == 42.0f);
        REQUIRE(calculate_std_dev(series) == 0.0f);
    }

    SECTION("Ranges outside bounds") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<size_t> times{10, 20, 30, 40, 50};
        AnalogTimeSeries series(data, times);

        // No data in range
        auto [empty_times, empty_values] = series.getDataVectorsInRange(60, 100);
        REQUIRE(empty_times.empty());
        REQUIRE(empty_values.empty());

        // Testing partial overlap
        auto [partial_times, partial_values] = series.getDataVectorsInRange(45, 100);
        REQUIRE(partial_times.size() == 1);
        REQUIRE(partial_values.size() == 1);
        REQUIRE(partial_times[0] == 50);
        REQUIRE(partial_values[0] == 5.0f);
    }

    SECTION("Overwriting with non-existent time points") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<size_t> times{10, 20, 30};
        AnalogTimeSeries series(data, times);

        // Attempt to overwrite at time points that don't exist
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<size_t> new_times{15, 25};

        // This should not change the original data
        series.overwriteAtTimes(new_data, new_times);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }

    SECTION("Overwriting with mismatched vector sizes") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<size_t> times{10, 20, 30};
        AnalogTimeSeries series(data, times);

        // Create vectors with mismatched sizes
        std::vector<float> new_data{9.0f, 8.0f};
        std::vector<size_t> new_times{20};

        // This should not change the original data
        series.overwriteAtTimes(new_data, new_times);

        auto const & stored_data = series.getAnalogTimeSeries();
        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f});
    }
}
