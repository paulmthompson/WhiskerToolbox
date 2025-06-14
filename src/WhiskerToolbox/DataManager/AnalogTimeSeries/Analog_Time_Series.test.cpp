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

TEST_CASE("AnalogTimeSeries - Boundary finding methods", "[analog][timeseries][boundary]") {

    SECTION("findDataArrayIndexGreaterOrEqual - sparse data with gaps") {
        // Create data with irregular TimeFrameIndex spacing: 1, 5, 7, 15, 20, 100, 200, 250, 300, 500
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact matches
        auto result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(5));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 1); // Index of TimeFrameIndex(5)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(100));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 5); // Index of TimeFrameIndex(100)

        // Test boundary cases - values between existing times
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(3)); // Between 1 and 5
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 1); // Should find TimeFrameIndex(5)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(50)); // Between 20 and 100
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 5); // Should find TimeFrameIndex(100)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(275)); // Between 250 and 300
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 8); // Should find TimeFrameIndex(300)

        // Test edge cases
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(0)); // Before all data
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0); // Should find TimeFrameIndex(1)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(500)); // Last value
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 9); // Should find TimeFrameIndex(500)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(1000)); // Beyond all data
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("findDataArrayIndexLessOrEqual - sparse data with gaps") {
        // Create data with irregular TimeFrameIndex spacing: 1, 5, 7, 15, 20, 100, 200, 250, 300, 500
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test exact matches
        auto result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(5));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 1); // Index of TimeFrameIndex(5)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(100));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 5); // Index of TimeFrameIndex(100)

        // Test boundary cases - values between existing times
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(3)); // Between 1 and 5
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0); // Should find TimeFrameIndex(1)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(50)); // Between 20 and 100
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 4); // Should find TimeFrameIndex(20)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(275)); // Between 250 and 300
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 7); // Should find TimeFrameIndex(250)

        // Test edge cases
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(0)); // Before all data
        REQUIRE_FALSE(result.has_value());
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(500)); // Last value
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 9); // Should find TimeFrameIndex(500)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(1000)); // Beyond all data
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 9); // Should find TimeFrameIndex(500)
    }

    SECTION("Boundary finding - dense consecutive storage") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103), TimeFrameIndex(104)};
        
        AnalogTimeSeries series(data, times);

        // Test findDataArrayIndexGreaterOrEqual
        auto result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(99)); // Before start
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0); // Should find TimeFrameIndex(100)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(102)); // Exact match
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 2); // Should find TimeFrameIndex(102)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(105)); // After end
        REQUIRE_FALSE(result.has_value());

        // Test findDataArrayIndexLessOrEqual
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(99)); // Before start
        REQUIRE_FALSE(result.has_value());
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(102)); // Exact match
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 2); // Should find TimeFrameIndex(102)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(105)); // After end
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 4); // Should find TimeFrameIndex(104)
    }

    SECTION("Boundary finding - dense storage starting from 0") {
        // Create data using the constructor that should produce dense storage starting from 0
        std::vector<float> data{5.5f, 6.6f, 7.7f, 8.8f, 9.9f};
        AnalogTimeSeries series(data, data.size()); // Dense storage from TimeFrameIndex(0)

        // Test findDataArrayIndexGreaterOrEqual
        auto result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(-1)); // Before start
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0); // Should find TimeFrameIndex(0)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(2)); // Middle
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 2); // Should find TimeFrameIndex(2)
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(5)); // After end
        REQUIRE_FALSE(result.has_value());

        // Test findDataArrayIndexLessOrEqual
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(-1)); // Before start
        REQUIRE_FALSE(result.has_value());
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(2)); // Middle
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 2); // Should find TimeFrameIndex(2)
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(5)); // After end
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 4); // Should find TimeFrameIndex(4)
    }

    SECTION("Boundary finding with single data point") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(50)};
        
        AnalogTimeSeries series(data, times);

        // Test findDataArrayIndexGreaterOrEqual
        auto result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(40)); // Before
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0);
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(50)); // Exact
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0);
        
        result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(60)); // After
        REQUIRE_FALSE(result.has_value());

        // Test findDataArrayIndexLessOrEqual
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(40)); // Before
        REQUIRE_FALSE(result.has_value());
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(50)); // Exact
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0);
        
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(60)); // After
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 0);
    }

    SECTION("Verify data values at found indices") {
        // Test that the boundary-finding methods return indices that give access to correct data
        std::vector<float> data{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6), TimeFrameIndex(8), TimeFrameIndex(10)
        };
        
        AnalogTimeSeries series(data, times);

        // Find index >= 5 (should find TimeFrameIndex(6) at DataArrayIndex(2))
        auto result = series.findDataArrayIndexGreaterOrEqual(TimeFrameIndex(5));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 2);
        REQUIRE(series.getDataAtDataArrayIndex(result.value()) == 30.0f);
        REQUIRE(series.getTimeFrameIndexAtDataArrayIndex(result.value()) == TimeFrameIndex(6));

        // Find index <= 5 (should find TimeFrameIndex(4) at DataArrayIndex(1))
        result = series.findDataArrayIndexLessOrEqual(TimeFrameIndex(5));
        REQUIRE(result.has_value());
        REQUIRE(result.value().getValue() == 1);
        REQUIRE(series.getDataAtDataArrayIndex(result.value()) == 20.0f);
        REQUIRE(series.getTimeFrameIndexAtDataArrayIndex(result.value()) == TimeFrameIndex(4));
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

TEST_CASE("AnalogTimeSeries - findDataArrayIndexForTimeFrameIndex functionality", "[analog][timeseries][index_lookup]") {

    SECTION("Regular spacing - even TimeFrameIndex values") {
        // Create 10 values (0-9) with even TimeFrameIndex values (2, 4, 6, 8, 10, 12, 14, 16, 18, 20)
        std::vector<float> data{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 10; ++i) {
            times.push_back(TimeFrameIndex(2 * (i + 1))); // 2, 4, 6, 8, 10, 12, 14, 16, 18, 20
        }
        
        AnalogTimeSeries series(data, times);

        // Test finding DataArrayIndex for each TimeFrameIndex
        for (size_t i = 0; i < times.size(); ++i) {
            auto result = series.findDataArrayIndexForTimeFrameIndex(times[i]);
            REQUIRE(result.has_value());
            REQUIRE(result.value().getValue() == i);
            
            // Verify the data value matches
            REQUIRE(series.getDataAtDataArrayIndex(result.value()) == data[i]);
        }

        // Test non-existent TimeFrameIndex values
        auto result_odd = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(3)); // Should not exist
        REQUIRE_FALSE(result_odd.has_value());
        
        auto result_too_high = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(22)); // Should not exist
        REQUIRE_FALSE(result_too_high.has_value());
        
        auto result_too_low = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(1)); // Should not exist
        REQUIRE_FALSE(result_too_low.has_value());
    }

    SECTION("Irregular spacing - scattered TimeFrameIndex values") {
        // Create 10 values with irregular TimeFrameIndex spacing
        std::vector<float> data{10.5f, 20.3f, 30.1f, 40.7f, 50.2f, 60.9f, 70.4f, 80.8f, 90.6f, 100.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(1), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(15), 
            TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(250), 
            TimeFrameIndex(300), TimeFrameIndex(500)
        };
        
        AnalogTimeSeries series(data, times);

        // Test finding DataArrayIndex for each TimeFrameIndex
        for (size_t i = 0; i < times.size(); ++i) {
            auto result = series.findDataArrayIndexForTimeFrameIndex(times[i]);
            REQUIRE(result.has_value());
            REQUIRE(result.value().getValue() == i);
            
            // Verify the data value matches
            REQUIRE(series.getDataAtDataArrayIndex(result.value()) == Catch::Approx(data[i]));
        }

        // Test non-existent TimeFrameIndex values in the gaps
        auto result_gap1 = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(3)); // Between 1 and 5
        REQUIRE_FALSE(result_gap1.has_value());
        
        auto result_gap2 = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(50)); // Between 20 and 100
        REQUIRE_FALSE(result_gap2.has_value());
        
        auto result_gap3 = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(275)); // Between 250 and 300
        REQUIRE_FALSE(result_gap3.has_value());
        
        auto result_beyond = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(1000)); // Beyond all values
        REQUIRE_FALSE(result_beyond.has_value());
        
        auto result_before = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(0)); // Before all values
        REQUIRE_FALSE(result_before.has_value());
    }

    SECTION("Dense time storage - consecutive TimeFrameIndex values starting from non-zero") {
        // Create data using the constructor that should produce dense storage
        std::vector<float> data{5.5f, 6.6f, 7.7f, 8.8f, 9.9f};
        // This should create DenseTimeRange starting from TimeFrameIndex(0)
        AnalogTimeSeries series(data, data.size());

        // Test finding DataArrayIndex for consecutive TimeFrameIndex values starting from 0
        for (size_t i = 0; i < data.size(); ++i) {
            auto result = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(static_cast<int64_t>(i)));
            REQUIRE(result.has_value());
            REQUIRE(result.value().getValue() == i);
            
            // Verify the data value matches
            REQUIRE(series.getDataAtDataArrayIndex(result.value()) == Catch::Approx(data[i]));
        }

        // Test non-existent TimeFrameIndex values outside the range
        auto result_negative = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(-1));
        REQUIRE_FALSE(result_negative.has_value());
        
        auto result_too_high = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(static_cast<int64_t>(data.size())));
        REQUIRE_FALSE(result_too_high.has_value());
    }

    SECTION("Dense time storage - consecutive TimeFrameIndex values with custom start") {
        // Create data with consecutive TimeFrameIndex values starting from 100
        std::vector<float> data{1.1f, 2.2f, 3.3f, 4.4f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(100), TimeFrameIndex(101), TimeFrameIndex(102), TimeFrameIndex(103)};
        
        AnalogTimeSeries series(data, times);

        // This should optimize to dense storage since the times are consecutive
        // Test finding DataArrayIndex for consecutive TimeFrameIndex values
        for (size_t i = 0; i < data.size(); ++i) {
            TimeFrameIndex target_time(100 + static_cast<int64_t>(i));
            auto result = series.findDataArrayIndexForTimeFrameIndex(target_time);
            REQUIRE(result.has_value());
            REQUIRE(result.value().getValue() == i);
            
            // Verify the data value matches
            REQUIRE(series.getDataAtDataArrayIndex(result.value()) == Catch::Approx(data[i]));
        }

        // Test non-existent TimeFrameIndex values outside the range
        auto result_before = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(99));
        REQUIRE_FALSE(result_before.has_value());
        
        auto result_after = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(104));
        REQUIRE_FALSE(result_after.has_value());
        
        auto result_gap = series.findDataArrayIndexForTimeFrameIndex(TimeFrameIndex(50));
        REQUIRE_FALSE(result_gap.has_value());
    }
}
