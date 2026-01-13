#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/AnalogDataStorage.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
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

        REQUIRE(std::vector<float>(stored_data.begin(), stored_data.end()) == data);
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

        REQUIRE(std::vector<float>(stored_data.begin(), stored_data.end()) == std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
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
            collected_values.push_back(point.value());
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
            collected_points.emplace_back(point.time_frame_index.getValue(), point.value());
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
        REQUIRE(point.value() == 42.0f);
        
        ++it;
        REQUIRE(it == range.end());
    }

    SECTION("Range interface - iterator operations") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40)
        };
        
        AnalogTimeSeries series(data, times);

        // Range covers times 20 and 30 (Values 2.0f and 3.0f)
        auto range = series.getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(15), TimeFrameIndex(35));
        
        auto it = range.begin();
        auto end_it = range.end();
        
        REQUIRE(it != end_it);
        REQUIRE(range.size() == 2); // View_interface provides this now
        
        // 1. Test Dereference (Return by Value)
        // Note: (*it) creates a temporary TimeValuePoint
        REQUIRE((*it).time_frame_index == TimeFrameIndex(20));
        REQUIRE((*it).value() == 2.0f);
        
        // 2. Test Random Access Indexing (operator[])
        REQUIRE(range[0].value() == 2.0f);
        REQUIRE(range[1].value() == 3.0f);

        // 3. Test Iterator Arithmetic (operator+, operator-)
        auto second_it = it + 1;
        REQUIRE((*second_it).value() == 3.0f);
        REQUIRE((second_it - it) == 1); // Difference type

        // 4. Test Pre/Post Increment
        ++it;
        REQUIRE((*it).time_frame_index == TimeFrameIndex(30));
        REQUIRE((*it).value() == 3.0f);
        
        // 5. Test Bidirectional (operator--)
        --it; 
        REQUIRE((*it).value() == 2.0f); // Should be back at start
        
        // 6. Test Random Access assignments (+=, -=)
        it += 1;
        REQUIRE((*it).value() == 3.0f);
        it -= 1;
        REQUIRE((*it).value() == 2.0f);

        // 7. Test End iterator logic
        it += 2; 
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
            range_results.emplace_back(point.time_frame_index.getValue(), point.value());
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

TEST_CASE("AnalogTimeSeries - Memory-mapped storage", "[analog][timeseries][mmap]") {
    
    SECTION("Memory-mapped int16 data") {
        // Create a temporary binary file with int16 data
        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_int16.bin";
        
        // Write test data: 100 int16 values
        std::vector<int16_t> test_data;
        for (int i = 0; i < 100; ++i) {
            test_data.push_back(static_cast<int16_t>(i * 10));
        }
        
        std::ofstream out(temp_file, std::ios::binary);
        out.write(reinterpret_cast<char const*>(test_data.data()), test_data.size() * sizeof(int16_t));
        out.close();
        
        // Create memory-mapped analog time series
        MmapStorageConfig config;
        config.file_path = temp_file;
        config.header_size = 0;
        config.offset = 0;
        config.stride = 1;
        config.data_type = MmapDataType::Int16;
        config.scale_factor = 1.0f;
        config.offset_value = 0.0f;
        config.num_samples = 100;
        
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 100; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        
        auto series = AnalogTimeSeries::createMemoryMapped(config, times);
        
        // Verify data access
        REQUIRE(series->getNumSamples() == 100);
        
        // Check values through iterator
        auto samples = series->getAllSamples();
        int count = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(static_cast<float>(count * 10)));
            count++;
        }
        REQUIRE(count == 100);
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("Memory-mapped with stride (interleaved channels)") {
        // Create file with 3 interleaved channels: [ch0, ch1, ch2, ch0, ch1, ch2, ...]
        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_strided.bin";
        
        std::vector<int16_t> interleaved_data;
        for (int i = 0; i < 50; ++i) {
            interleaved_data.push_back(static_cast<int16_t>(i * 100));     // Channel 0
            interleaved_data.push_back(static_cast<int16_t>(i * 100 + 1)); // Channel 1
            interleaved_data.push_back(static_cast<int16_t>(i * 100 + 2)); // Channel 2
        }
        
        std::ofstream out(temp_file, std::ios::binary);
        out.write(reinterpret_cast<char const*>(interleaved_data.data()), 
                  interleaved_data.size() * sizeof(int16_t));
        out.close();
        
        // Access channel 1 (offset=1, stride=3)
        MmapStorageConfig config;
        config.file_path = temp_file;
        config.header_size = 0;
        config.offset = 1;  // Start at second element (channel 1)
        config.stride = 3;  // Skip 3 elements between samples
        config.data_type = MmapDataType::Int16;
        config.scale_factor = 1.0f;
        config.offset_value = 0.0f;
        config.num_samples = 0; // Auto-detect
        
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 50; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        
        auto series = AnalogTimeSeries::createMemoryMapped(config, times);
        
        REQUIRE(series->getNumSamples() == 50);
        
        // Verify we're reading channel 1 data
        auto samples = series->getAllSamples();
        int count = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(static_cast<float>(count * 100 + 1)));
            count++;
        }
        REQUIRE(count == 50);
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("Memory-mapped with header and scale/offset") {
        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_header.bin";
        
        // Write 256-byte header followed by int16 data
        std::ofstream out(temp_file, std::ios::binary);
        std::vector<char> header(256, 0xAA); // Dummy header
        out.write(header.data(), header.size());
        
        // Write actual data after header
        std::vector<int16_t> data;
        for (int i = 0; i < 100; ++i) {
            data.push_back(static_cast<int16_t>(i + 1000)); // Offset by 1000
        }
        out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(int16_t));
        out.close();
        
        // Configure with header, scale, and offset
        MmapStorageConfig config;
        config.file_path = temp_file;
        config.header_size = 256;
        config.offset = 0;
        config.stride = 1;
        config.data_type = MmapDataType::Int16;
        config.scale_factor = 0.1f;  // Scale down by 10x
        config.offset_value = -100.0f; // Subtract 100
        config.num_samples = 100;
        
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 100; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        
        auto series = AnalogTimeSeries::createMemoryMapped(config, times);
        
        REQUIRE(series->getNumSamples() == 100);
        
        // Verify scale and offset applied: value = (raw * scale) + offset
        // For i=0: raw=1000, result = 1000*0.1 - 100 = 0
        // For i=10: raw=1010, result = 1010*0.1 - 100 = 1
        auto samples = series->getAllSamples();
        int count = 0;
        for (auto const& [time, value] : samples) {
            float expected = static_cast<float>(count + 1000) * 0.1f - 100.0f;
            REQUIRE(value == Catch::Approx(expected).margin(0.01f));
            count++;
        }
        REQUIRE(count == 100);
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("Memory-mapped different data types") {
        // Test float32
        {
            std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_float32.bin";
            std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
            
            std::ofstream out(temp_file, std::ios::binary);
            out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(float));
            out.close();
            
            MmapStorageConfig config;
            config.file_path = temp_file;
            config.data_type = MmapDataType::Float32;
            config.num_samples = 5;
            
            std::vector<TimeFrameIndex> times{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
                                            TimeFrameIndex(3), TimeFrameIndex(4)};
            
            auto series = AnalogTimeSeries::createMemoryMapped(config, times);
            auto samples = series->getAllSamples();
            
            int idx = 0;
            for (auto const& [time, value] : samples) {
                REQUIRE(value == Catch::Approx(data[idx]).margin(0.001f));
                idx++;
            }
            
            std::filesystem::remove(temp_file);
        }
        
        // Test uint8
        {
            std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_uint8.bin";
            std::vector<uint8_t> data{10, 20, 30, 40, 50};
            
            std::ofstream out(temp_file, std::ios::binary);
            out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(uint8_t));
            out.close();
            
            MmapStorageConfig config;
            config.file_path = temp_file;
            config.data_type = MmapDataType::UInt8;
            config.num_samples = 5;
            
            std::vector<TimeFrameIndex> times{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
                                            TimeFrameIndex(3), TimeFrameIndex(4)};
            
            auto series = AnalogTimeSeries::createMemoryMapped(config, times);
            auto samples = series->getAllSamples();
            
            int idx = 0;
            for (auto const& [time, value] : samples) {
                REQUIRE(value == Catch::Approx(static_cast<float>(data[idx])));
                idx++;
            }
            
            std::filesystem::remove(temp_file);
        }
    }
    
    SECTION("Memory-mapped error handling") {
        // Non-existent file
        MmapStorageConfig config;
        config.file_path = "/nonexistent/path/to/file.bin";
        config.num_samples = 10;
        
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 10; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        
        REQUIRE_THROWS_AS(AnalogTimeSeries::createMemoryMapped(config, times), std::runtime_error);
        
        // Mismatched time vector size
        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_size_mismatch.bin";
        std::vector<float> data(100, 1.0f);
        
        std::ofstream out(temp_file, std::ios::binary);
        out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(float));
        out.close();
        
        config.file_path = temp_file;
        config.data_type = MmapDataType::Float32;
        config.num_samples = 100;
        
        std::vector<TimeFrameIndex> wrong_size_times; // Wrong size!
        for (int i = 0; i < 50; ++i) {
            wrong_size_times.push_back(TimeFrameIndex(i));
        }
        
        REQUIRE_THROWS_AS(AnalogTimeSeries::createMemoryMapped(config, wrong_size_times), std::runtime_error);
        
        std::filesystem::remove(temp_file);
    }
}

TEST_CASE("AnalogTimeSeries - Lazy View Storage", "[analog][timeseries][lazy][view]") {
    
    SECTION("Basic lazy transform - z-score normalization") {
        // Create base series
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Compute statistics
        float sum = 0.0f, sum_sq = 0.0f;
        size_t n = base_series->getNumSamples();
        for (auto const& sample : base_series->view()) {
            sum += sample.value();
            sum_sq += sample.value() * sample.value();
        }
        float mean = sum / n;  // 30.0
        float variance = (sum_sq / n) - (mean * mean);  // 200.0
        float std = std::sqrt(variance);  // 14.142...
        
        // Create lazy z-score transform
        auto z_score_view = base_series->view()
            | std::views::transform([mean, std](auto tv) {
                float z = (tv.value() - mean) / std;
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, z};
            });
        
        // Wrap in AnalogTimeSeries
        auto normalized_series = AnalogTimeSeries::createFromView(
            z_score_view,
            base_series->getTimeStorage()
        );
        
        // Verify lazy computation
        REQUIRE(normalized_series->getNumSamples() == 5);
        REQUIRE(normalized_series->getTimeStorage() == base_series->getTimeStorage());
        
        // Check z-scores are computed correctly on access
        auto samples = normalized_series->getAllSamples();
        std::vector<float> expected_z_scores{
            (10.0f - 30.0f) / std,  // -1.414...
            (20.0f - 30.0f) / std,  // -0.707...
            (30.0f - 30.0f) / std,  // 0.0
            (40.0f - 30.0f) / std,  // 0.707...
            (50.0f - 30.0f) / std   // 1.414...
        };
        
        int idx = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(expected_z_scores[idx]).margin(0.001f));
            idx++;
        }
        REQUIRE(idx == 5);
    }
    
    SECTION("Lazy transform with std::pair interface") {
        // Create base series
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create lazy transform that doubles values (using std::pair)
        auto doubled_view = base_series->view()
            | std::views::transform([](auto tv) {
                return std::pair{tv.time_frame_index, tv.value() * 2.0f};
            });
        
        auto doubled_series = AnalogTimeSeries::createFromView(
            doubled_view,
            base_series->getTimeStorage()
        );
        
        // Verify doubled values
        auto samples = doubled_series->getAllSamples();
        std::vector<float> expected{2.0f, 4.0f, 6.0f, 8.0f};
        
        int idx = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(expected[idx]));
            idx++;
        }
        REQUIRE(idx == 4);
    }
    
    SECTION("Lazy transform - chained transforms") {
        // Create base series
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Chain: square, then add 10
        auto transformed_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() * tv.value()};
            })
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() + 10.0f};
            });
        
        auto transformed_series = AnalogTimeSeries::createFromView(
            transformed_view,
            base_series->getTimeStorage()
        );
        
        // Verify: (x^2) + 10
        auto samples = transformed_series->getAllSamples();
        std::vector<float> expected{11.0f, 14.0f, 19.0f, 26.0f, 35.0f};
        
        int idx = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(expected[idx]));
            idx++;
        }
    }
    
    SECTION("Lazy transform - storage type verification") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        auto lazy_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() * 2.0f};
            });
        
        auto lazy_series = AnalogTimeSeries::createFromView(lazy_view, base_series->getTimeStorage());
        
        // Verify storage type
        REQUIRE(lazy_series->getAnalogTimeSeries().empty());  // No contiguous span available
    }
    
    SECTION("Lazy transform - getDataInTimeFrameIndexRange") {
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20), 
            TimeFrameIndex(30), TimeFrameIndex(40)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        auto scaled_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() * 0.1f};
            });
        
        auto scaled_series = AnalogTimeSeries::createFromView(scaled_view, base_series->getTimeStorage());
        
        // Range queries should work but return empty span (non-contiguous)
        auto span = scaled_series->getDataInTimeFrameIndexRange(TimeFrameIndex(10), TimeFrameIndex(30));
        REQUIRE(span.empty());  // Lazy storage doesn't provide contiguous spans
        
        // But iteration should work
        auto range = scaled_series->getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex(10), TimeFrameIndex(30));
        std::vector<float> collected;
        for (auto const& sample : range) {
            collected.push_back(sample.value());
        }
        
        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0] == Catch::Approx(2.0f));  // 20 * 0.1
        REQUIRE(collected[1] == Catch::Approx(3.0f));  // 30 * 0.1
        REQUIRE(collected[2] == Catch::Approx(4.0f));  // 40 * 0.1
    }
    
    SECTION("Lazy transform - getAtTime") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
            TimeFrameIndex(400), TimeFrameIndex(500)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        auto cubed_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() * tv.value() * tv.value()};
            });
        
        auto cubed_series = AnalogTimeSeries::createFromView(cubed_view, base_series->getTimeStorage());
        
        // Random access should work
        auto value = cubed_series->getAtTime(TimeFrameIndex(300));
        REQUIRE(value.has_value());
        REQUIRE(value.value() == Catch::Approx(27.0f));  // 3^3
        
        value = cubed_series->getAtTime(TimeFrameIndex(500));
        REQUIRE(value.has_value());
        REQUIRE(value.value() == Catch::Approx(125.0f));  // 5^3
        
        value = cubed_series->getAtTime(TimeFrameIndex(999));
        REQUIRE_FALSE(value.has_value());
    }
    
    SECTION("Lazy transform - error handling") {
        std::vector<float> data{1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create view with wrong size
        auto view_subset = base_series->view() | std::views::take(2);
        
        // Create different time storage with wrong size
        std::vector<TimeFrameIndex> wrong_times{TimeFrameIndex(0), TimeFrameIndex(1)};
        auto wrong_time_storage = TimeIndexStorageFactory::createFromTimeIndices(std::move(wrong_times));
        
        // Should throw due to size mismatch
        REQUIRE_THROWS_AS(
            AnalogTimeSeries::createFromView(view_subset, base_series->getTimeStorage()),
            std::runtime_error
        );
    }
}

TEST_CASE("AnalogTimeSeries - Materialization", "[analog][timeseries][materialize]") {
    
    SECTION("Materialize lazy view storage") {
        // Create lazy series
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        auto squared_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() * tv.value()};
            });
        
        auto lazy_series = AnalogTimeSeries::createFromView(squared_view, base_series->getTimeStorage());
        
        // Materialize
        auto materialized = lazy_series->materialize();
        
        // Verify materialized data
        REQUIRE(materialized->getNumSamples() == 5);
        
        auto span = materialized->getAnalogTimeSeries();
        REQUIRE_FALSE(span.empty());  // Should now have contiguous storage
        REQUIRE(span.size() == 5);
        
        REQUIRE(span[0] == Catch::Approx(1.0f));   // 1^2
        REQUIRE(span[1] == Catch::Approx(4.0f));   // 2^2
        REQUIRE(span[2] == Catch::Approx(9.0f));   // 3^2
        REQUIRE(span[3] == Catch::Approx(16.0f));  // 4^2
        REQUIRE(span[4] == Catch::Approx(25.0f));  // 5^2
    }
    
    SECTION("Materialize vector storage (deep copy)") {
        std::vector<float> data{10.0f, 20.0f, 30.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Materialize already-materialized series (should deep copy)
        auto materialized = base_series->materialize();
        
        REQUIRE(materialized->getNumSamples() == 3);
        
        auto span = materialized->getAnalogTimeSeries();
        REQUIRE(span.size() == 3);
        REQUIRE(span[0] == 10.0f);
        REQUIRE(span[1] == 20.0f);
        REQUIRE(span[2] == 30.0f);
        
        // Verify it's a different object
        REQUIRE(span.data() != base_series->getAnalogTimeSeries().data());
    }
    
    SECTION("Materialize memory-mapped storage") {
        // Create temporary file
        std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_materialize_mmap.bin";
        
        std::vector<float> data{1.5f, 2.5f, 3.5f, 4.5f, 5.5f};
        std::ofstream out(temp_file, std::ios::binary);
        out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(float));
        out.close();
        
        // Create memory-mapped series
        MmapStorageConfig config;
        config.file_path = temp_file;
        config.data_type = MmapDataType::Float32;
        config.num_samples = 5;
        
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        
        auto mmap_series = AnalogTimeSeries::createMemoryMapped(config, times);
        
        // Materialize
        auto materialized = mmap_series->materialize();
        
        // Verify data is now in vector storage
        auto span = materialized->getAnalogTimeSeries();
        REQUIRE_FALSE(span.empty());
        REQUIRE(span.size() == 5);
        
        for (size_t i = 0; i < 5; ++i) {
            REQUIRE(span[i] == Catch::Approx(data[i]));
        }
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("Materialize enables efficient random access") {
        // Create lazy series with expensive computation
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 1000; ++i) {
            data.push_back(static_cast<float>(i));
            times.push_back(TimeFrameIndex(i));
        }
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Expensive transform (for demonstration)
        auto expensive_view = base_series->view()
            | std::views::transform([](auto tv) {
                float result = tv.value();
                for (int i = 0; i < 10; ++i) {
                    result = std::sqrt(result + 1.0f);
                }
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, result};
            });
        
        auto lazy_series = AnalogTimeSeries::createFromView(expensive_view, base_series->getTimeStorage());
        
        // Materialize for efficient repeated access
        auto materialized = lazy_series->materialize();
        
        // Random access pattern (would be expensive with lazy evaluation)
        std::vector<int> random_indices{100, 500, 200, 800, 50, 900, 300};
        for (auto idx : random_indices) {
            auto value = materialized->getAtTime(TimeFrameIndex(idx));
            REQUIRE(value.has_value());
        }
        
        // Verify span access works
        auto span = materialized->getAnalogTimeSeries();
        REQUIRE(span.size() == 1000);
    }
    
    SECTION("Materialize preserves time indices") {
        // Create lazy series with sparse time indices
        std::vector<float> data{10.0f, 20.0f, 30.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(5), TimeFrameIndex(100), TimeFrameIndex(500)};
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        auto transformed_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() + 1.0f};
            });
        
        auto lazy_series = AnalogTimeSeries::createFromView(transformed_view, base_series->getTimeStorage());
        auto materialized = lazy_series->materialize();
        
        // Verify time indices preserved
        auto time_vec = materialized->getTimeSeries();
        REQUIRE(time_vec.size() == 3);
        REQUIRE(time_vec[0] == TimeFrameIndex(5));
        REQUIRE(time_vec[1] == TimeFrameIndex(100));
        REQUIRE(time_vec[2] == TimeFrameIndex(500));
        
        // Verify values
        auto span = materialized->getAnalogTimeSeries();
        REQUIRE(span[0] == Catch::Approx(11.0f));
        REQUIRE(span[1] == Catch::Approx(21.0f));
        REQUIRE(span[2] == Catch::Approx(31.0f));
    }
}

TEST_CASE("AnalogTimeSeries - Lazy View Integration with Statistics", "[analog][timeseries][lazy][statistics]") {
    
    SECTION("Compute statistics on lazy view") {
        // Create base series
        std::vector<float> data{2.0f, 4.0f, 6.0f, 8.0f, 10.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create lazy log transform
        auto log_view = base_series->view()
            | std::views::transform([](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, std::log(tv.value())};
            });
        
        auto log_series = AnalogTimeSeries::createFromView(log_view, base_series->getTimeStorage());
        
        // Compute mean on lazy series
        float sum = 0.0f;
        int count = 0;
        for (auto const& sample : log_series->getAllSamples()) {
            sum += sample.value();
            count++;
        }
        float mean = sum / count;
        
        // Expected: mean(log(2), log(4), log(6), log(8), log(10))
        float expected_mean = (std::log(2.0f) + std::log(4.0f) + std::log(6.0f) + 
                              std::log(8.0f) + std::log(10.0f)) / 5.0f;
        
        REQUIRE(mean == Catch::Approx(expected_mean).margin(0.001f));
    }
    
    SECTION("Normalize then compute standard deviation") {
        std::vector<float> data{5.0f, 10.0f, 15.0f, 20.0f, 25.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto base_series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // First pass: compute mean
        float mean = 15.0f;  // Known mean
        
        // Create centered view (subtract mean)
        auto centered_view = base_series->view()
            | std::views::transform([mean](auto tv) {
                return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, tv.value() - mean};
            });
        
        auto centered_series = AnalogTimeSeries::createFromView(centered_view, base_series->getTimeStorage());
        
        // Compute variance
        float sum_sq = 0.0f;
        int count = 0;
        for (auto const& sample : centered_series->getAllSamples()) {
            sum_sq += sample.value() * sample.value();
            count++;
        }
        float variance = sum_sq / count;
        float std_dev = std::sqrt(variance);
        
        // Expected std dev for {5, 10, 15, 20, 25} is sqrt(50) ≈ 7.071
        REQUIRE(std_dev == Catch::Approx(7.071f).margin(0.01f));
    }
}
