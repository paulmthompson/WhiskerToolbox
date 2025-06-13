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
                {50, 5.0f}};

        AnalogTimeSeries series(data_map);

        auto const & stored_data = series.getAnalogTimeSeries();
        auto const & time_data = series.getTimeSeries();

        REQUIRE(stored_data.size() == 5);
        REQUIRE(time_data.size() == 5);

        REQUIRE(stored_data == std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
        REQUIRE(time_data == std::vector<size_t>{10, 20, 30, 40, 50});
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
