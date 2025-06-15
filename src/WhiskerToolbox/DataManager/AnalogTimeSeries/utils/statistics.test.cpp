#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <ranges>
#include <random>
#include <vector>


TEST_CASE("AnalogTimeSeries - Statistics", "[analog][timeseries][statistics]") {

    SECTION("Statistical calculations") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        REQUIRE_THAT(calculate_mean(series), Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series), Catch::Matchers::WithinRel(1.41421f, 1e-3f));// 1.41421 for N, 1.5811 for N-1 denom
        REQUIRE(calculate_min(series) == 1.0f);
        REQUIRE(calculate_max(series) == 5.0f);

        // Test with range. 1 to 4 will reduce the data to {2.0f, 3.0f, 4.0f}
        REQUIRE_THAT(calculate_mean(series, 1, 4), Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series, 1, 4), Catch::Matchers::WithinRel(0.8165f, 1e-3f));// 0.8165 for N, 1.0 for N-1 denom
        REQUIRE(calculate_min(series, 1, 4) == 2.0f);
        REQUIRE(calculate_max(series, 1, 4) == 4.0f);
    }

}

TEST_CASE("AnalogTimeSeries - Approximate Statistics", "[analog][timeseries][approximate]") {
    SECTION("Approximate standard deviation with percentage sampling") {
        // Create a large dataset with known statistical properties
        std::vector<float> data;
        data.reserve(10000);
        //Create gaussian-like data
        std::default_random_engine generator;
        std::normal_distribution<float> distribution(50.0f, 10.0f);// Mean 50, StdDev 10
        for (int i = 0; i < 10000; ++i) {
            data.push_back(distribution(generator));
        }
        AnalogTimeSeries series(data, 10000);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 1.0f, 50);// Sample 1% with min threshold 50

        // The approximation should be reasonably close (within 10% for this regular pattern)
        float relative_error = std::abs(exact_std - approx_std) / exact_std;
        REQUIRE(relative_error < 0.1f);
    }

    SECTION("Single data point") {
        std::vector<float> data{42.0f};
        AnalogTimeSeries series(data, 1);

        REQUIRE(calculate_mean(series) == 42.0f);
        REQUIRE(calculate_min(series) == 42.0f);
        REQUIRE(calculate_max(series) == 42.0f);
        REQUIRE(calculate_std_dev(series) == 0.0f);
    }

    SECTION("Approximate standard deviation falls back to exact for small datasets") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 10.0f, 1000);// High percentage but min threshold 1000

        // Should fall back to exact calculation since 5 * 0.1 < 1000
        REQUIRE(exact_std == approx_std);
    }

    SECTION("Adaptive standard deviation convergence") {
        // Create a dataset with varying values but consistent distribution
        std::vector<float> data;
        data.reserve(50000);
        for (int i = 0; i < 50000; ++i) {
            data.push_back(static_cast<float>(std::sin(i * 0.1) * 10.0 + 50.0));
        }
        AnalogTimeSeries series(data, 50000);

        float exact_std = calculate_std_dev(series);
        float adaptive_std = calculate_std_dev_adaptive(series, 100, 5000, 0.02f);

        // The adaptive method should converge to a reasonable approximation
        float relative_error = std::abs(exact_std - adaptive_std) / exact_std;
        REQUIRE(relative_error < 0.05f);
    }

    SECTION("Adaptive standard deviation falls back to exact for small datasets") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        float exact_std = calculate_std_dev(series);
        float adaptive_std = calculate_std_dev_adaptive(series, 100, 1000, 0.01f);

        // Should fall back to exact calculation since data size (5) <= max_sample_size (1000)
        REQUIRE(exact_std == adaptive_std);
    }

    SECTION("Empty series handling for approximate methods") {
        AnalogTimeSeries empty_series;

        REQUIRE(std::isnan(calculate_std_dev_approximate(empty_series)));
        REQUIRE(std::isnan(calculate_std_dev_adaptive(empty_series)));
    }

    SECTION("Single value series for approximate methods") {
        std::vector<float> data{42.0f};
        AnalogTimeSeries series(data, 1);

        REQUIRE(calculate_std_dev_approximate(series) == 0.0f);
        REQUIRE(calculate_std_dev_adaptive(series) == 0.0f);
    }

    SECTION("Performance comparison scenario") {
        // Create a large dataset similar to neuroscience recordings
        std::vector<float> data;
        data.reserve(1000000);// 1M samples
        for (int i = 0; i < 1000000; ++i) {
            // Simulate noisy signal with some trend
            data.push_back(static_cast<float>(std::sin(i * 0.001) * 5.0 + (i * 0.00001) +
                                              (std::rand() % 100) * 0.01));
        }
        AnalogTimeSeries series(data, 1000000);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 0.1f, 1000);// Sample 0.1%
        float adaptive_std = calculate_std_dev_adaptive(series, 500, 5000, 0.01f);

        // Both approximations should be reasonably close to exact
        float approx_error = std::abs(exact_std - approx_std) / exact_std;
        float adaptive_error = std::abs(exact_std - adaptive_std) / exact_std;

        REQUIRE(approx_error < 0.05f);
        REQUIRE(adaptive_error < 0.05f);
    }

    SECTION("calculate_mean with span - basic functionality") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::span<const float> data_span(data);
        
        float mean = calculate_mean(data_span);
        REQUIRE(mean == Catch::Approx(3.0f));
        
        // Test partial span
        std::span<const float> partial_span(data.data() + 1, 3); // {2.0f, 3.0f, 4.0f}
        mean = calculate_mean(partial_span);
        REQUIRE(mean == Catch::Approx(3.0f));
        
        // Test empty span
        std::span<const float> empty_span;
        mean = calculate_mean(empty_span);
        REQUIRE(std::isnan(mean));
    }

    SECTION("calculate_mean_in_time_range - sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact range [5, 20] - includes values 20.0f, 30.0f, 40.0f, 50.0f
        float mean = calculate_mean_in_time_range(series, TimeFrameIndex(5), TimeFrameIndex(20));
        float expected_mean = (20.0f + 30.0f + 40.0f + 50.0f) / 4.0f; // = 35.0f
        REQUIRE(mean == Catch::Approx(expected_mean));

        // Test boundary approximation [3, 50] - should find >= 3 (starts at 5) and <= 50 (ends at 20)
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(3), TimeFrameIndex(50));
        REQUIRE(mean == Catch::Approx(expected_mean)); // Same range as above

        // Test single element range [100, 100]
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(100), TimeFrameIndex(100));
        REQUIRE(mean == Catch::Approx(60.0f)); // Value at TimeFrameIndex(100)

        // Test larger range [200, 500] - includes values 70.0f, 80.0f, 90.0f, 100.0f
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(200), TimeFrameIndex(500));
        expected_mean = (70.0f + 80.0f + 90.0f + 100.0f) / 4.0f; // = 85.0f
        REQUIRE(mean == Catch::Approx(expected_mean));

        // Test range with no data [600, 700]
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(600), TimeFrameIndex(700));
        REQUIRE(std::isnan(mean)); // Empty span should return 0
    }

    SECTION("calculate_mean_in_time_range - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)};
        
        AnalogTimeSeries series(data, times);

        // Test exact range [101, 103] - includes values 2.2f, 3.3f, 4.4f
        float mean = calculate_mean_in_time_range(series, TimeFrameIndex(101), TimeFrameIndex(103));
        float expected_mean = (2.2f + 3.3f + 4.4f) / 3.0f;
        REQUIRE(mean == Catch::Approx(expected_mean));

        // Test all data [99, 105]
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(99), TimeFrameIndex(105));
        expected_mean = (1.1f + 2.2f + 3.3f + 4.4f + 5.5f) / 5.0f; // = 3.3f
        REQUIRE(mean == Catch::Approx(expected_mean));

        // Test single element [102, 102]
        mean = calculate_mean_in_time_range(series, TimeFrameIndex(102), TimeFrameIndex(102));
        REQUIRE(mean == Catch::Approx(3.3f));
    }

    SECTION("Verify consistency between different mean calculation methods") {
        // Create test data
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4),
            TimeFrameIndex(5), TimeFrameIndex(6), TimeFrameIndex(7), TimeFrameIndex(8), TimeFrameIndex(9)
        };
        
        AnalogTimeSeries series(data, times);

        // Calculate mean using different methods and verify they match
        
        // Method 1: Traditional AnalogTimeSeries function (entire series)
        float mean1 = calculate_mean(series);
        
        // Method 2: TimeFrameIndex range method (entire series)
        float mean2 = calculate_mean_in_time_range(series, TimeFrameIndex(0), TimeFrameIndex(9));
        
        // Method 3: Direct span method (entire series)
        auto data_span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(9));
        float mean3 = calculate_mean(data_span);
        
        // Method 4: Array index range method (entire series)
        float mean4 = calculate_mean(series, 0, static_cast<int64_t>(data.size()));
        
        // All methods should give the same result for the entire series
        REQUIRE(mean1 == Catch::Approx(mean2));
        REQUIRE(mean1 == Catch::Approx(mean3));
        REQUIRE(mean1 == Catch::Approx(mean4));
        REQUIRE(mean1 == Catch::Approx(5.5f)); // Expected mean of 1-10

        // Test partial range consistency
        // Method 1: Array index range [2, 7) - indices 2,3,4,5,6 -> values 3,4,5,6,7
        float partial_mean1 = calculate_mean(series, 2, 7);
        
        // Method 2: TimeFrameIndex range [2, 6] - TimeFrameIndex 2,3,4,5,6 -> values 3,4,5,6,7
        float partial_mean2 = calculate_mean_in_time_range(series, TimeFrameIndex(2), TimeFrameIndex(6));
        
        REQUIRE(partial_mean1 == Catch::Approx(partial_mean2));
        REQUIRE(partial_mean1 == Catch::Approx(5.0f)); // Expected mean of 3,4,5,6,7
    }

    SECTION("calculate_mean_impl with vector indices") {
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        
        // Test normal range
        float mean = calculate_mean_impl(data, 1, 4); // indices 1,2,3 -> values 20,30,40
        REQUIRE(mean == Catch::Approx(30.0f));
        
        // Test full range
        mean = calculate_mean_impl(data, 0, data.size());
        REQUIRE(mean == Catch::Approx(30.0f)); // (10+20+30+40+50)/5 = 30
        
        // Test single element
        mean = calculate_mean_impl(data, 2, 3); // index 2 -> value 30
        REQUIRE(mean == Catch::Approx(30.0f));
        
        // Test invalid ranges
        mean = calculate_mean_impl(data, 3, 2); // start > end
        REQUIRE(std::isnan(mean));
        
        mean = calculate_mean_impl(data, 5, 6); // start >= size
        REQUIRE(std::isnan(mean));
        
        mean = calculate_mean_impl(data, 0, 10); // end > size  
        REQUIRE(std::isnan(mean));
    }

    SECTION("calculate_min with span - basic functionality") {
        std::vector<float> data{5.0f, 2.0f, 8.0f, 1.0f, 9.0f};
        std::span<const float> data_span(data);
        
        float min_val = calculate_min(data_span);
        REQUIRE(min_val == Catch::Approx(1.0f));
        
        // Test partial span
        std::span<const float> partial_span(data.data() + 1, 3); // {2.0f, 8.0f, 1.0f}
        min_val = calculate_min(partial_span);
        REQUIRE(min_val == Catch::Approx(1.0f));
        
        // Test empty span
        std::span<const float> empty_span;
        min_val = calculate_min(empty_span);
        REQUIRE(std::isnan(min_val));
    }

    SECTION("calculate_max with span - basic functionality") {
        std::vector<float> data{5.0f, 2.0f, 8.0f, 1.0f, 9.0f};
        std::span<const float> data_span(data);
        
        float max_val = calculate_max(data_span);
        REQUIRE(max_val == Catch::Approx(9.0f));
        
        // Test partial span
        std::span<const float> partial_span(data.data() + 1, 3); // {2.0f, 8.0f, 1.0f}
        max_val = calculate_max(partial_span);
        REQUIRE(max_val == Catch::Approx(8.0f));
        
        // Test empty span
        std::span<const float> empty_span;
        max_val = calculate_max(empty_span);
        REQUIRE(std::isnan(max_val));
    }

    SECTION("calculate_min_in_time_range - sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact range [5, 20] - includes values 20.0f, 30.0f, 40.0f, 50.0f
        float min_val = calculate_min_in_time_range(series, TimeFrameIndex(5), TimeFrameIndex(20));
        REQUIRE(min_val == Catch::Approx(20.0f)); // Minimum of 20.0f, 30.0f, 40.0f, 50.0f

        // Test boundary approximation [3, 50] - should find >= 3 (starts at 5) and <= 50 (ends at 20)
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(3), TimeFrameIndex(50));
        REQUIRE(min_val == Catch::Approx(20.0f)); // Same range as above

        // Test single element range [100, 100]
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(100), TimeFrameIndex(100));
        REQUIRE(min_val == Catch::Approx(60.0f)); // Value at TimeFrameIndex(100)

        // Test larger range [200, 500] - includes values 70.0f, 80.0f, 90.0f, 100.0f
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(200), TimeFrameIndex(500));
        REQUIRE(min_val == Catch::Approx(70.0f)); // Minimum of 70.0f, 80.0f, 90.0f, 100.0f

        // Test range with no data [600, 700]
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(600), TimeFrameIndex(700));
        REQUIRE(std::isnan(min_val)); // Empty span should return NaN
    }

    SECTION("calculate_max_in_time_range - sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact range [5, 20] - includes values 20.0f, 30.0f, 40.0f, 50.0f
        float max_val = calculate_max_in_time_range(series, TimeFrameIndex(5), TimeFrameIndex(20));
        REQUIRE(max_val == Catch::Approx(50.0f)); // Maximum of 20.0f, 30.0f, 40.0f, 50.0f

        // Test boundary approximation [3, 50] - should find >= 3 (starts at 5) and <= 50 (ends at 20)
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(3), TimeFrameIndex(50));
        REQUIRE(max_val == Catch::Approx(50.0f)); // Same range as above

        // Test single element range [100, 100]
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(100), TimeFrameIndex(100));
        REQUIRE(max_val == Catch::Approx(60.0f)); // Value at TimeFrameIndex(100)

        // Test larger range [200, 500] - includes values 70.0f, 80.0f, 90.0f, 100.0f
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(200), TimeFrameIndex(500));
        REQUIRE(max_val == Catch::Approx(100.0f)); // Maximum of 70.0f, 80.0f, 90.0f, 100.0f

        // Test range with no data [600, 700]
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(600), TimeFrameIndex(700));
        REQUIRE(std::isnan(max_val)); // Empty span should return NaN
    }

    SECTION("calculate_min_in_time_range - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)};
        
        AnalogTimeSeries series(data, times);

        // Test exact range [101, 103] - includes values 2.2f, 3.3f, 4.4f
        float min_val = calculate_min_in_time_range(series, TimeFrameIndex(101), TimeFrameIndex(103));
        REQUIRE(min_val == Catch::Approx(2.2f));

        // Test all data [99, 105]
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(99), TimeFrameIndex(105));
        REQUIRE(min_val == Catch::Approx(1.1f)); // Minimum of all data

        // Test single element [102, 102]
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(102), TimeFrameIndex(102));
        REQUIRE(min_val == Catch::Approx(3.3f));
    }

    SECTION("calculate_max_in_time_range - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)};
        
        AnalogTimeSeries series(data, times);

        // Test exact range [101, 103] - includes values 2.2f, 3.3f, 4.4f
        float max_val = calculate_max_in_time_range(series, TimeFrameIndex(101), TimeFrameIndex(103));
        REQUIRE(max_val == Catch::Approx(4.4f));

        // Test all data [99, 105]
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(99), TimeFrameIndex(105));
        REQUIRE(max_val == Catch::Approx(5.5f)); // Maximum of all data

        // Test single element [102, 102]
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(102), TimeFrameIndex(102));
        REQUIRE(max_val == Catch::Approx(3.3f));
    }

    SECTION("Verify consistency between different min/max calculation methods") {
        // Create test data
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4),
            TimeFrameIndex(5), TimeFrameIndex(6), TimeFrameIndex(7), TimeFrameIndex(8), TimeFrameIndex(9)
        };
        
        AnalogTimeSeries series(data, times);

        // Calculate min/max using different methods and verify they match
        
        // Method 1: Traditional AnalogTimeSeries function (entire series)
        float min1 = calculate_min(series);
        float max1 = calculate_max(series);
        
        // Method 2: TimeFrameIndex range method (entire series)
        float min2 = calculate_min_in_time_range(series, TimeFrameIndex(0), TimeFrameIndex(9));
        float max2 = calculate_max_in_time_range(series, TimeFrameIndex(0), TimeFrameIndex(9));
        
        // Method 3: Direct span method (entire series)
        auto data_span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(9));
        float min3 = calculate_min(data_span);
        float max3 = calculate_max(data_span);
        
        // Method 4: Array index range method (entire series)
        float min4 = calculate_min(series, 0, static_cast<int64_t>(data.size()));
        float max4 = calculate_max(series, 0, static_cast<int64_t>(data.size()));
        
        // All methods should give the same result for the entire series
        REQUIRE(min1 == Catch::Approx(min2));
        REQUIRE(min1 == Catch::Approx(min3));
        REQUIRE(min1 == Catch::Approx(min4));
        REQUIRE(min1 == Catch::Approx(1.0f)); // Expected min of 1-10
        
        REQUIRE(max1 == Catch::Approx(max2));
        REQUIRE(max1 == Catch::Approx(max3));
        REQUIRE(max1 == Catch::Approx(max4));
        REQUIRE(max1 == Catch::Approx(10.0f)); // Expected max of 1-10

        // Test partial range consistency
        // Method 1: Array index range [2, 7) - indices 2,3,4,5,6 -> values 3,4,5,6,7
        float partial_min1 = calculate_min(series, 2, 7);
        float partial_max1 = calculate_max(series, 2, 7);
        
        // Method 2: TimeFrameIndex range [2, 6] - TimeFrameIndex 2,3,4,5,6 -> values 3,4,5,6,7
        float partial_min2 = calculate_min_in_time_range(series, TimeFrameIndex(2), TimeFrameIndex(6));
        float partial_max2 = calculate_max_in_time_range(series, TimeFrameIndex(2), TimeFrameIndex(6));
        
        REQUIRE(partial_min1 == Catch::Approx(partial_min2));
        REQUIRE(partial_min1 == Catch::Approx(3.0f)); // Expected min of 3,4,5,6,7
        
        REQUIRE(partial_max1 == Catch::Approx(partial_max2));
        REQUIRE(partial_max1 == Catch::Approx(7.0f)); // Expected max of 3,4,5,6,7
    }

    SECTION("calculate_min_impl with vector indices") {
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        
        // Test normal range
        float min_val = calculate_min_impl(data, 1, 4); // indices 1,2,3 -> values 20,30,40
        REQUIRE(min_val == Catch::Approx(20.0f));
        
        // Test full range
        min_val = calculate_min_impl(data, 0, data.size());
        REQUIRE(min_val == Catch::Approx(10.0f)); // Min of 10,20,30,40,50
        
        // Test single element
        min_val = calculate_min_impl(data, 2, 3); // index 2 -> value 30
        REQUIRE(min_val == Catch::Approx(30.0f));
        
        // Test invalid ranges
        min_val = calculate_min_impl(data, 3, 2); // start > end
        REQUIRE(std::isnan(min_val));
        
        min_val = calculate_min_impl(data, 5, 6); // start >= size
        REQUIRE(std::isnan(min_val));
        
        min_val = calculate_min_impl(data, 0, 10); // end > size  
        REQUIRE(std::isnan(min_val));
    }

    SECTION("calculate_max_impl with vector indices") {
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        
        // Test normal range
        float max_val = calculate_max_impl(data, 1, 4); // indices 1,2,3 -> values 20,30,40
        REQUIRE(max_val == Catch::Approx(40.0f));
        
        // Test full range
        max_val = calculate_max_impl(data, 0, data.size());
        REQUIRE(max_val == Catch::Approx(50.0f)); // Max of 10,20,30,40,50
        
        // Test single element
        max_val = calculate_max_impl(data, 2, 3); // index 2 -> value 30
        REQUIRE(max_val == Catch::Approx(30.0f));
        
        // Test invalid ranges
        max_val = calculate_max_impl(data, 3, 2); // start > end
        REQUIRE(std::isnan(max_val));
        
        max_val = calculate_max_impl(data, 5, 6); // start >= size
        REQUIRE(std::isnan(max_val));
        
        max_val = calculate_max_impl(data, 0, 10); // end > size  
        REQUIRE(std::isnan(max_val));
    }

    SECTION("Min/Max with negative values") {
        std::vector<float> data{-5.0f, -2.0f, 3.0f, -8.0f, 1.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)
        };
        
        AnalogTimeSeries series(data, times);

        // Test entire series
        float min_val = calculate_min(series);
        float max_val = calculate_max(series);
        REQUIRE(min_val == Catch::Approx(-8.0f));
        REQUIRE(max_val == Catch::Approx(3.0f));

        // Test time range [20, 40] - includes values -2.0f, 3.0f, -8.0f
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(20), TimeFrameIndex(40));
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(20), TimeFrameIndex(40));
        REQUIRE(min_val == Catch::Approx(-8.0f));
        REQUIRE(max_val == Catch::Approx(3.0f));
    }

    SECTION("Min/Max with identical values") {
        std::vector<float> data{42.0f, 42.0f, 42.0f, 42.0f, 42.0f};
        AnalogTimeSeries series(data, data.size());

        float min_val = calculate_min(series);
        float max_val = calculate_max(series);
        REQUIRE(min_val == Catch::Approx(42.0f));
        REQUIRE(max_val == Catch::Approx(42.0f));

        // Test time range
        min_val = calculate_min_in_time_range(series, TimeFrameIndex(1), TimeFrameIndex(3));
        max_val = calculate_max_in_time_range(series, TimeFrameIndex(1), TimeFrameIndex(3));
        REQUIRE(min_val == Catch::Approx(42.0f));
        REQUIRE(max_val == Catch::Approx(42.0f));
    }
}
