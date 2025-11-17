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


}
TEST_CASE("AnalogTimeSeries - getDataInTimeFrameIndexRange functionality", "[analog][timeseries][span_range]") {

    SECTION("Basic range extraction - sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact range [5, 20] - should include indices 1, 2, 3, 4 (values 20.0f, 30.0f, 40.0f, 50.0f)
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(5), TimeFrameIndex(20));
        REQUIRE(span.size() == 4);
        REQUIRE(span[0] == 20.0f); // TimeFrameIndex(5)
        REQUIRE(span[1] == 30.0f); // TimeFrameIndex(7)  
        REQUIRE(span[2] == 40.0f); // TimeFrameIndex(15)
        REQUIRE(span[3] == 50.0f); // TimeFrameIndex(20)

        // Test boundary approximation [3, 50] - should find >= 3 (starts at 5) and <= 50 (ends at 20)
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(3), TimeFrameIndex(50));
        REQUIRE(span.size() == 4);
        REQUIRE(span[0] == 20.0f); // TimeFrameIndex(5) - first >= 3
        REQUIRE(span[3] == 50.0f); // TimeFrameIndex(20) - last <= 50

        // Test single element range [100, 100]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(100), TimeFrameIndex(100));
        REQUIRE(span.size() == 1);
        REQUIRE(span[0] == 60.0f); // TimeFrameIndex(100)

        // Test larger range [200, 500]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(200), TimeFrameIndex(500));
        REQUIRE(span.size() == 4);
        REQUIRE(span[0] == 70.0f);  // TimeFrameIndex(200)
        REQUIRE(span[1] == 80.0f);  // TimeFrameIndex(250)
        REQUIRE(span[2] == 90.0f);  // TimeFrameIndex(300)
        REQUIRE(span[3] == 100.0f); // TimeFrameIndex(500)
    }

    SECTION("Range boundary testing - sparse data") {
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6), TimeFrameIndex(8), TimeFrameIndex(10)
        };
        
        AnalogTimeSeries series(data, times);

        // Test range that includes all data [0, 15]
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(15));
        REQUIRE(span.size() == 5);
        for (size_t i = 0; i < span.size(); ++i) {
            REQUIRE(span[i] == data[i]);
        }

        // Test range before all data [0, 1] - should be empty
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(1));
        REQUIRE(span.empty());

        // Test range after all data [11, 20] - should be empty
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(11), TimeFrameIndex(20));
        REQUIRE(span.empty());

        // Test inverted range [10, 2] - should be empty
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(10), TimeFrameIndex(2));
        REQUIRE(span.empty());

        // Test partial overlap at start [1, 5] - should get TimeFrameIndex(2) and TimeFrameIndex(4)
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(1), TimeFrameIndex(5));
        REQUIRE(span.size() == 2);
        REQUIRE(span[0] == 10.0f); // TimeFrameIndex(2)
        REQUIRE(span[1] == 20.0f); // TimeFrameIndex(4)

        // Test partial overlap at end [7, 15] - should get TimeFrameIndex(8) and TimeFrameIndex(10)
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(7), TimeFrameIndex(15));
        REQUIRE(span.size() == 2);
        REQUIRE(span[0] == 40.0f); // TimeFrameIndex(8)
        REQUIRE(span[1] == 50.0f); // TimeFrameIndex(10)
    }

    SECTION("Dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)};
        
        AnalogTimeSeries series(data, times);

        // Test exact range [101, 103]
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(101), TimeFrameIndex(103));
        REQUIRE(span.size() == 3);
        REQUIRE(span[0] == 2.2f); // TimeFrameIndex(101)
        REQUIRE(span[1] == 3.3f); // TimeFrameIndex(102)
        REQUIRE(span[2] == 4.4f); // TimeFrameIndex(103)

        // Test boundary approximation [99, 105] - should get all data
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(99), TimeFrameIndex(105));
        REQUIRE(span.size() == 5);
        for (size_t i = 0; i < span.size(); ++i) {
            REQUIRE(span[i] == data[i]);
        }

        // Test range within bounds [102, 102] - single element
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(102), TimeFrameIndex(102));
        REQUIRE(span.size() == 1);
        REQUIRE(span[0] == 3.3f);
    }

    SECTION("Dense storage starting from 0") {
        // Create data using the constructor that produces dense storage starting from 0
        std::vector<float> data{5.5f, 6.6f, 7.7f, 8.8f, 9.9f};
        AnalogTimeSeries series(data, data.size()); // Dense storage from TimeFrameIndex(0)

        // Test range [1, 3]
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(1), TimeFrameIndex(3));
        REQUIRE(span.size() == 3);
        REQUIRE(span[0] == 6.6f); // TimeFrameIndex(1)
        REQUIRE(span[1] == 7.7f); // TimeFrameIndex(2)
        REQUIRE(span[2] == 8.8f); // TimeFrameIndex(3)

        // Test range including start [0, 2]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(2));
        REQUIRE(span.size() == 3);
        REQUIRE(span[0] == 5.5f); // TimeFrameIndex(0)
        REQUIRE(span[1] == 6.6f); // TimeFrameIndex(1)
        REQUIRE(span[2] == 7.7f); // TimeFrameIndex(2)

        // Test all data [0, 4]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(4));
        REQUIRE(span.size() == 5);
        for (size_t i = 0; i < span.size(); ++i) {
            REQUIRE(span[i] == data[i]);
        }
    }

    SECTION("Single data point") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(50)};
        
        AnalogTimeSeries series(data, times);

        // Test exact match [50, 50]
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(50), TimeFrameIndex(50));
        REQUIRE(span.size() == 1);
        REQUIRE(span[0] == 42.0f);

        // Test range that includes the point [40, 60]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(40), TimeFrameIndex(60));
        REQUIRE(span.size() == 1);
        REQUIRE(span[0] == 42.0f);

        // Test range before the point [30, 40]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(30), TimeFrameIndex(40));
        REQUIRE(span.empty());

        // Test range after the point [60, 70]
        span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(60), TimeFrameIndex(70));
        REQUIRE(span.empty());
    }

    SECTION("Empty series") {
        AnalogTimeSeries series; // Default constructor creates empty series

        // Test any range on empty series
        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(0), TimeFrameIndex(10));
        REQUIRE(span.empty());
    }

    SECTION("Span properties and memory safety") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)
        };
        
        AnalogTimeSeries series(data, times);

        auto span = series.getDataInTimeFrameIndexRange(TimeFrameIndex(20), TimeFrameIndex(40));
        
        // Verify span properties
        REQUIRE(span.size() == 3);
        REQUIRE_FALSE(span.empty());
        
        // Verify data access
        REQUIRE(span.data() != nullptr);
        REQUIRE(span[0] == 2.0f);
        REQUIRE(span[1] == 3.0f);
        REQUIRE(span[2] == 4.0f);
        
        // Verify iterator access
        auto it = span.begin();
        REQUIRE(*it == 2.0f);
        ++it;
        REQUIRE(*it == 3.0f);
        ++it;
        REQUIRE(*it == 4.0f);
        ++it;
        REQUIRE(it == span.end());

        // Verify the span points to the same memory as the original data
        // (span should be a view, not a copy)
        auto const& original_data = series.getAnalogTimeSeries();
        REQUIRE(span.data() == &original_data[1]); // Should point to the second element (index 1)
    }
}

TEST_CASE("AnalogTimeSeries - Time-Value Range Interface", "[analog][timeseries][time_value_range]") {

    SECTION("Range interface - basic iteration with sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6), TimeFrameIndex(8), TimeFrameIndex(10)
        };
        
        AnalogTimeSeries series(data, times);

        // Test range-based for loop iteration
        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(3), TimeFrameIndex(9));
        
        std::vector<TimeFrameIndex> collected_times;
        std::vector<float> collected_values;
        
        for (auto const& point : range) {
            collected_times.push_back(point.time_frame_index);
            collected_values.push_back(point.value);
        }
        
        // Should get TimeFrameIndex 4, 6, 8 (data values 20.0f, 30.0f, 40.0f)
        REQUIRE(collected_times.size() == 3);
        REQUIRE(collected_values.size() == 3);
        
        REQUIRE(collected_times[0] == TimeFrameIndex(4));
        REQUIRE(collected_values[0] == 20.0f);
        
        REQUIRE(collected_times[1] == TimeFrameIndex(6));
        REQUIRE(collected_values[1] == 30.0f);
        
        REQUIRE(collected_times[2] == TimeFrameIndex(8));
        REQUIRE(collected_values[2] == 40.0f);
    }

    SECTION("Range interface - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)
        };
        
        AnalogTimeSeries series(data, times);

        // Test range iteration [101, 103]
        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(101), TimeFrameIndex(103));
        
        std::vector<std::pair<int64_t, float>> collected_points;
        
        for (auto const& point : range) {
            collected_points.emplace_back(point.time_frame_index.getValue(), point.value);
        }
        
        // Should get TimeFrameIndex 101, 102, 103 (values 2.2f, 3.3f, 4.4f)
        REQUIRE(collected_points.size() == 3);
        
        REQUIRE(collected_points[0].first == 101);
        REQUIRE(collected_points[0].second == 2.2f);
        
        REQUIRE(collected_points[1].first == 102);
        REQUIRE(collected_points[1].second == 3.3f);
        
        REQUIRE(collected_points[2].first == 103);
        REQUIRE(collected_points[2].second == 4.4f);
    }

    SECTION("Range interface - empty range") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        
        AnalogTimeSeries series(data, times);

        // Test range that doesn't overlap with data
        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(40), TimeFrameIndex(50));
        
        REQUIRE(range.empty());
        REQUIRE(range.size() == 0);
        
    }

    SECTION("Range interface - single point") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(50)};
        
        AnalogTimeSeries series(data, times);

        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(45), TimeFrameIndex(55));
        
        REQUIRE_FALSE(range.empty());
        REQUIRE(range.size() == 1);
        
        auto it = range.begin();
        REQUIRE(it != range.end());
        
        auto const& point = *it;
        REQUIRE(point.time_frame_index == TimeFrameIndex(50));
        REQUIRE(point.value == 42.0f);
        
        ++it;
        REQUIRE(it == range.end());
    }

    SECTION("Range interface - iterator operations") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40)
        };
        
        AnalogTimeSeries series(data, times);

        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(15), TimeFrameIndex(35));
        
        auto it = range.begin();
        auto end_it = range.end();
        
        REQUIRE(it != end_it);
        
        // Test dereference and arrow operator
        REQUIRE((*it).time_frame_index == TimeFrameIndex(20));
        REQUIRE(it->value == 2.0f);
        
        // Test pre-increment
        ++it;
        REQUIRE(it->time_frame_index == TimeFrameIndex(30));
        REQUIRE(it->value == 3.0f);
        
        // Test post-increment
        auto old_it = it++;
        REQUIRE(old_it->time_frame_index == TimeFrameIndex(30));
        REQUIRE(it == end_it);
    }
}

TEST_CASE("AnalogTimeSeries - Time-Value Span Interface", "[analog][timeseries][time_value_span]") {

    SECTION("Span interface - basic zero-copy access with sparse data") {
        // Create data with irregular TimeFrameIndex spacing
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6), TimeFrameIndex(8), TimeFrameIndex(10)
        };
        
        AnalogTimeSeries series(data, times);

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(3), TimeFrameIndex(9));
        
        // Verify the data span
        REQUIRE(span_pair.values.size() == 3);
        REQUIRE(span_pair.values[0] == 20.0f); // TimeFrameIndex(4)
        REQUIRE(span_pair.values[1] == 30.0f); // TimeFrameIndex(6)
        REQUIRE(span_pair.values[2] == 40.0f); // TimeFrameIndex(8)
        
        // Verify span points to original data memory (zero-copy)
        auto const& original_data = series.getAnalogTimeSeries();
        REQUIRE(span_pair.values.data() == &original_data[1]); // Should point to index 1 in original data
        
        // Verify the time iterator
        REQUIRE(span_pair.time_indices.size() == 3);
        REQUIRE_FALSE(span_pair.time_indices.empty());
        
        // Test basic time iterator functionality
        auto time_begin = span_pair.time_indices.begin();
        REQUIRE(**time_begin == TimeFrameIndex(4));
        
        ++(*time_begin);
        REQUIRE(**time_begin == TimeFrameIndex(6));
        
        ++(*time_begin);
        REQUIRE(**time_begin == TimeFrameIndex(8));
    }

    SECTION("Span interface - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)
        };
        
        AnalogTimeSeries series(data, times);

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(101), TimeFrameIndex(103));
        
        // Verify the data span (zero-copy)
        REQUIRE(span_pair.values.size() == 3);
        REQUIRE(span_pair.values[0] == 2.2f); // TimeFrameIndex(101)
        REQUIRE(span_pair.values[1] == 3.3f); // TimeFrameIndex(102)
        REQUIRE(span_pair.values[2] == 4.4f); // TimeFrameIndex(103)
        
        // Verify span points to original data memory
        auto const& original_data = series.getAnalogTimeSeries();
        REQUIRE(span_pair.values.data() == &original_data[1]); // Should point to index 1 in original data
        
        // Verify the time iterator works for dense storage
        REQUIRE(span_pair.time_indices.size() == 3);
        
        // Test dense time iterator manually (should generate consecutive values)
        auto time_it = span_pair.time_indices.begin();
        REQUIRE(**time_it == TimeFrameIndex(101));
        
        ++(*time_it);
        REQUIRE(**time_it == TimeFrameIndex(102));
        
        ++(*time_it);
        REQUIRE(**time_it == TimeFrameIndex(103));
    }

    SECTION("Span interface - empty range") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        
        AnalogTimeSeries series(data, times);

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(40), TimeFrameIndex(50));
        
        REQUIRE(span_pair.values.empty());
        REQUIRE(span_pair.values.size() == 0);
        REQUIRE(span_pair.time_indices.empty());
        REQUIRE(span_pair.time_indices.size() == 0);
    }

    SECTION("Span interface - single point") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(50)};
        
        AnalogTimeSeries series(data, times);

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(45), TimeFrameIndex(55));
        
        REQUIRE(span_pair.values.size() == 1);
        REQUIRE(span_pair.values[0] == 42.0f);
        
        REQUIRE(span_pair.time_indices.size() == 1);
        REQUIRE_FALSE(span_pair.time_indices.empty());
        
        auto time_it = span_pair.time_indices.begin();
        REQUIRE(**time_it == TimeFrameIndex(50));
    }

    SECTION("Span interface - boundary approximation") {
        // Test data with gaps to verify boundary approximation logic
        std::vector<float> data{100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), TimeFrameIndex(20)
        };
        
        AnalogTimeSeries series(data, times);

        // Request range [3, 18] - should get [5, 15] due to boundary approximation
        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(3), TimeFrameIndex(18));
        
        REQUIRE(span_pair.values.size() == 3);
        REQUIRE(span_pair.values[0] == 200.0f); // TimeFrameIndex(5)
        REQUIRE(span_pair.values[1] == 300.0f); // TimeFrameIndex(7)
        REQUIRE(span_pair.values[2] == 400.0f); // TimeFrameIndex(15)
        
        REQUIRE(span_pair.time_indices.size() == 3);
    }

    SECTION("Span interface - consistency with existing methods") {
        // Verify that the span interface returns the same data as existing methods
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(15), TimeFrameIndex(20), TimeFrameIndex(25), TimeFrameIndex(30), TimeFrameIndex(35)
        };
        
        AnalogTimeSeries series(data, times);

        TimeFrameIndex start_time(18);
        TimeFrameIndex end_time(32);

        // Get span from new interface
        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(start_time, end_time);
        
        // Get span from existing method for comparison
        auto existing_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
        
        // Should be identical
        REQUIRE(span_pair.values.size() == existing_span.size());
        REQUIRE(span_pair.values.data() == existing_span.data());
        
        for (size_t i = 0; i < span_pair.values.size(); ++i) {
            REQUIRE(span_pair.values[i] == existing_span[i]);
        }
    }

    SECTION("Span interface - dense storage from constructor") {
        // Test with dense storage created by the num_samples constructor
        std::vector<float> data{5.5f, 6.6f, 7.7f, 8.8f, 9.9f};
        AnalogTimeSeries series(data, data.size()); // Dense storage from TimeFrameIndex(0)

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(1), TimeFrameIndex(3));
        
        REQUIRE(span_pair.values.size() == 3);
        REQUIRE(span_pair.values[0] == 6.6f); // TimeFrameIndex(1)
        REQUIRE(span_pair.values[1] == 7.7f); // TimeFrameIndex(2)
        REQUIRE(span_pair.values[2] == 8.8f); // TimeFrameIndex(3)
        
        // Verify zero-copy property
        auto const& original_data = series.getAnalogTimeSeries();
        REQUIRE(span_pair.values.data() == &original_data[1]);
        
        // Verify time indices work for dense storage starting from 0
        REQUIRE(span_pair.time_indices.size() == 3);
        auto time_it = span_pair.time_indices.begin();
        REQUIRE(**time_it == TimeFrameIndex(1));
    }
}

TEST_CASE("AnalogTimeSeries - Time-Value Interface Comparison", "[analog][timeseries][time_value_comparison]") {

    SECTION("Range vs Span interface - equivalent results") {
        // Test that both interfaces return the same time-value pairs
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(2), TimeFrameIndex(5), TimeFrameIndex(8), TimeFrameIndex(12), TimeFrameIndex(15), TimeFrameIndex(20)
        };
        
        AnalogTimeSeries series(data, times);

        TimeFrameIndex start_time(6);
        TimeFrameIndex end_time(16);

        // Get data using range interface
        auto range = series.getTimeValueRangeInTimeFrameIndexRange(start_time, end_time);
        std::vector<std::pair<int64_t, float>> range_results;
        
        for (auto const& point : range) {
            range_results.emplace_back(point.time_frame_index.getValue(), point.value);
        }

        // Get data using span interface - simple test just checking values match
        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(start_time, end_time);
        
        // Results should have same size and values
        REQUIRE(range_results.size() == span_pair.values.size());
        
        for (size_t i = 0; i < range_results.size(); ++i) {
            REQUIRE(range_results[i].second == span_pair.values[i]); // Values should match
        }
    }

    SECTION("Performance characteristics - zero-copy verification") {
        // Verify that span interface provides true zero-copy access
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        
        // Create larger dataset for meaningful test
        for (int i = 0; i < 1000; ++i) {
            data.push_back(static_cast<float>(i) * 1.5f);
            times.push_back(TimeFrameIndex(i * 2)); // Even indices
        }
        
        AnalogTimeSeries series(data, times);

        auto span_pair = series.getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex(100), TimeFrameIndex(500));
        
        // Verify that span points to original memory
        auto const& original_data = series.getAnalogTimeSeries();
        bool points_to_original = (span_pair.values.data() >= original_data.data()) && 
                                 (span_pair.values.data() < original_data.data() + original_data.size());
        
        REQUIRE(points_to_original);
        
        // Verify span size is reasonable
        REQUIRE(span_pair.values.size() > 100); // Should capture many points
        REQUIRE(span_pair.values.size() <= 1000); // But not more than total data
    }
}
