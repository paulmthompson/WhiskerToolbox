#include "DataAggregation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <vector>

using namespace DataAggregation;

TEST_CASE("DataAggregation - Basic interval operations", "[data_aggregation][intervals]") {
    SECTION("Calculate overlap duration between intervals") {
        Interval a{100, 200};
        Interval b{150, 250};
        Interval c{300, 400};

        REQUIRE(calculateOverlapDuration(a, b) == 51);// 150-200 inclusive = 51
        REQUIRE(calculateOverlapDuration(a, c) == 0); // No overlap
        REQUIRE(calculateOverlapDuration(b, a) == 51);// Commutative

        // Adjacent intervals
        Interval d{201, 300};
        REQUIRE(calculateOverlapDuration(a, d) == 0);// Adjacent but not overlapping

        // Touching intervals
        Interval e{200, 300};
        REQUIRE(calculateOverlapDuration(a, e) == 1);// Touch at point 200
    }

    SECTION("Check if intervals overlap using existing function") {
        Interval a{100, 200};
        Interval b{150, 250};
        Interval c{300, 400};
        Interval d{200, 300};

        REQUIRE(is_overlapping(a, b) == true);
        REQUIRE(is_overlapping(a, c) == false);
        REQUIRE(is_overlapping(a, d) == true);// Touch at point 200
        REQUIRE(is_overlapping(b, a) == true);// Commutative
    }
}

TEST_CASE("DataAggregation - User scenario test", "[data_aggregation][user_scenario]") {
    // Set up the data from the user's example
    std::vector<Interval> interval_foo = {
            {100, 200},
            {240, 500},
            {700, 900}};

    std::vector<Interval> interval_bar = {
            {40, 550},
            {650, 1000}};

    std::map<std::string, std::vector<Interval>> reference_data = {
            {"interval_bar", interval_bar}};

    SECTION("Start and end time transformations") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start_time"},
                {TransformationType::IntervalEnd, "end_time"}};

        auto result = aggregateData(interval_foo, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 3);   // 3 rows
        REQUIRE(result[0].size() == 2);// 2 columns

        // Row 1: interval (100, 200)
        REQUIRE(result[0][0] == 100.0);// start_time
        REQUIRE(result[0][1] == 200.0);// end_time

        // Row 2: interval (240, 500)
        REQUIRE(result[1][0] == 240.0);// start_time
        REQUIRE(result[1][1] == 500.0);// end_time

        // Row 3: interval (700, 900)
        REQUIRE(result[2][0] == 700.0);// start_time
        REQUIRE(result[2][1] == 900.0);// end_time
    }

    SECTION("Interval ID transformation with First strategy") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start_time"},
                {TransformationType::IntervalEnd, "end_time"},
                {TransformationType::IntervalID, "interval_bar_id", "interval_bar", OverlapStrategy::First}};

        auto result = aggregateData(interval_foo, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 3);   // 3 rows
        REQUIRE(result[0].size() == 3);// 3 columns

        // Expected IDs: interval_foo (100,200) overlaps with interval_bar[0] (40,550) -> ID 0
        // interval_foo (240,500) overlaps with interval_bar[0] (40,550) -> ID 0
        // interval_foo (700,900) overlaps with interval_bar[1] (650,1000) -> ID 1

        REQUIRE(result[0][2] == 0.0);// First interval_bar (0-based)
        REQUIRE(result[1][2] == 0.0);// First interval_bar (0-based)
        REQUIRE(result[2][2] == 1.0);// Second interval_bar (0-based)
    }

    SECTION("Duration transformation") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalDuration, "duration"}};

        auto result = aggregateData(interval_foo, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 1);

        // Duration = end - start + 1
        REQUIRE(result[0][0] == 101.0);// 200 - 100 + 1 = 101
        REQUIRE(result[1][0] == 261.0);// 500 - 240 + 1 = 261
        REQUIRE(result[2][0] == 201.0);// 900 - 700 + 1 = 201
    }
}

TEST_CASE("DataAggregation - Overlap strategies", "[data_aggregation][overlap_strategies]") {
    std::vector<Interval> row_intervals = {
            {100, 400}// This interval overlaps with multiple reference intervals
    };

    std::vector<Interval> reference_intervals = {
            {50, 150}, // ID 0: overlaps 50 units (100-150)
            {200, 300},// ID 1: overlaps 101 units (200-300)
            {350, 450} // ID 2: overlaps 51 units (350-400)
    };

    std::map<std::string, std::vector<Interval>> reference_data = {
            {"multi_overlap", reference_intervals}};

    SECTION("First overlap strategy") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalID, "first_id", "multi_overlap", OverlapStrategy::First}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(result[0][0] == 0.0);// First overlapping interval
    }

    SECTION("Last overlap strategy") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalID, "last_id", "multi_overlap", OverlapStrategy::Last}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(result[0][0] == 2.0);// Last overlapping interval
    }

    SECTION("Max overlap strategy") {
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalID, "max_id", "multi_overlap", OverlapStrategy::MaxOverlap}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(result[0][0] == 1.0);// Interval with maximum overlap (101 units)
    }
}

TEST_CASE("DataAggregation - Edge cases and error handling", "[data_aggregation][edge_cases]") {
    SECTION("No overlap - returns NaN") {
        std::vector<Interval> row_intervals = {
                {600, 649}// No overlap with reference intervals
        };

        std::vector<Interval> reference_intervals = {
                {40, 550},
                {650, 1000}};

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"no_overlap", reference_intervals}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalID, "id", "no_overlap"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(std::isnan(result[0][0]));
    }

    SECTION("Missing reference data - returns NaN") {
        std::vector<Interval> row_intervals = {
                {100, 200}};

        std::map<std::string, std::vector<Interval>> reference_data = {};// Empty

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalID, "missing_id", "nonexistent_key"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(std::isnan(result[0][0]));
    }

    SECTION("Empty intervals") {
        std::vector<Interval> empty_intervals = {};
        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"}};

        auto result = aggregateData(empty_intervals, transformations, {}, {}, {});
        REQUIRE(result.empty());
    }

    SECTION("Empty transformations") {
        std::vector<Interval> row_intervals = {
                {100, 200}};
        std::vector<TransformationConfig> empty_transformations = {};

        auto result = aggregateData(row_intervals, empty_transformations, {}, {}, {});
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].empty());
    }

    SECTION("Single point intervals") {
        std::vector<Interval> row_intervals = {
                {100, 100}// Single point interval
        };

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalEnd, "end"},
                {TransformationType::IntervalDuration, "duration"}};

        auto result = aggregateData(row_intervals, transformations, {}, {}, {});
        REQUIRE(result[0][0] == 100.0);// start
        REQUIRE(result[0][1] == 100.0);// end
        REQUIRE(result[0][2] == 1.0);  // duration (100-100+1=1)
    }
}

TEST_CASE("DataAggregation - Complex scenario", "[data_aggregation][complex]") {
    SECTION("Multiple reference datasets and mixed transformations") {
        std::vector<Interval> row_intervals = {
                {100, 200},
                {300, 400},
                {500, 600}};

        std::vector<Interval> ref_data_1 = {
                {50, 150}, // ID 0
                {250, 350},// ID 1
                {450, 550} // ID 2
        };

        std::vector<Interval> ref_data_2 = {
                {75, 125}, // ID 0
                {275, 325},// ID 1
                {475, 525} // ID 2
        };

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"ref1", ref_data_1},
                {"ref2", ref_data_2}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalEnd, "end"},
                {TransformationType::IntervalDuration, "duration"},
                {TransformationType::IntervalID, "ref1_id", "ref1", OverlapStrategy::First},
                {TransformationType::IntervalID, "ref2_id", "ref2", OverlapStrategy::MaxOverlap}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 5);

        // Row 1: (100, 200)
        REQUIRE(result[0][0] == 100.0);// start
        REQUIRE(result[0][1] == 200.0);// end
        REQUIRE(result[0][2] == 101.0);// duration
        REQUIRE(result[0][3] == 0.0);  // ref1_id (overlaps with ref_data_1[0])
        REQUIRE(result[0][4] == 0.0);  // ref2_id (overlaps with ref_data_2[0])

        // Row 2: (300, 400)
        REQUIRE(result[1][0] == 300.0);// start
        REQUIRE(result[1][1] == 400.0);// end
        REQUIRE(result[1][2] == 101.0);// duration
        REQUIRE(result[1][3] == 1.0);  // ref1_id (overlaps with ref_data_1[1])
        REQUIRE(result[1][4] == 1.0);  // ref2_id (overlaps with ref_data_2[1])

        // Row 3: (500, 600)
        REQUIRE(result[2][0] == 500.0);// start
        REQUIRE(result[2][1] == 600.0);// end
        REQUIRE(result[2][2] == 101.0);// duration
        REQUIRE(result[2][3] == 2.0);  // ref1_id (overlaps with ref_data_1[2])
        REQUIRE(result[2][4] == 2.0);  // ref2_id (overlaps with ref_data_2[2])
    }
}

TEST_CASE("DataAggregation - IntervalCount transformation", "[data_aggregation][interval_count]") {
    SECTION("Count overlapping intervals - user scenario") {
        // Extended user scenario to test counting
        std::vector<Interval> row_intervals = {
                {100, 200},// Should overlap with 1 interval from ref_data
                {50, 350}, // Should overlap with 2 intervals from ref_data
                {600, 700} // Should overlap with 0 intervals from ref_data
        };

        std::vector<Interval> ref_data = {
                {80, 150}, // Overlaps with intervals 1 and 2
                {300, 400},// Overlaps with interval 2 only
                {500, 550} // No overlap with any row interval
        };

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"test_ref", ref_data}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalCount, "overlap_count", "test_ref"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 2);

        // Row 1: (100, 200) overlaps with ref_data[0] (80, 150) -> count = 1
        REQUIRE(result[0][0] == 100.0);// start
        REQUIRE(result[0][1] == 1.0);  // count

        // Row 2: (50, 350) overlaps with ref_data[0] (80, 150) and ref_data[1] (300, 400) -> count = 2
        REQUIRE(result[1][0] == 50.0);// start
        REQUIRE(result[1][1] == 2.0); // count

        // Row 3: (600, 700) overlaps with no intervals -> count = 0
        REQUIRE(result[2][0] == 600.0);// start
        REQUIRE(result[2][1] == 0.0);  // count
    }

    SECTION("Count with multiple overlapping intervals") {
        std::vector<Interval> row_intervals = {
                {100, 400}// This should overlap with multiple reference intervals
        };

        std::vector<Interval> reference_intervals = {
                {50, 150}, // Overlaps
                {200, 300},// Overlaps
                {350, 450},// Overlaps
                {500, 600} // No overlap
        };

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"multi_ref", reference_intervals}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalCount, "count", "multi_ref"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(result[0][0] == 3.0);// Should count 3 overlapping intervals
    }

    SECTION("Count with no overlaps") {
        std::vector<Interval> row_intervals = {
                {100, 200}};

        std::vector<Interval> reference_intervals = {
                {300, 400},
                {500, 600}};

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"no_overlap_ref", reference_intervals}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalCount, "count", "no_overlap_ref"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(result[0][0] == 0.0);// Should count 0 overlapping intervals
    }

    SECTION("Count with missing reference data") {
        std::vector<Interval> row_intervals = {
                {100, 200}};

        std::map<std::string, std::vector<Interval>> reference_data = {};// Empty

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalCount, "count", "missing_ref"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});
        REQUIRE(std::isnan(result[0][0]));// Should return NaN for missing reference
    }
}

TEST_CASE("DataAggregation - Combined transformations with IntervalCount", "[data_aggregation][combined]") {
    SECTION("Mix IntervalID and IntervalCount") {
        std::vector<Interval> row_intervals = {
                {100, 300}// Should overlap with 2 intervals, first one has ID 0
        };

        std::vector<Interval> reference_intervals = {
                {80, 150},// ID 0: overlaps
                {250, 350}// ID 1: overlaps
        };

        std::map<std::string, std::vector<Interval>> reference_data = {
                {"combined_ref", reference_intervals}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalEnd, "end"},
                {TransformationType::IntervalDuration, "duration"},
                {TransformationType::IntervalID, "first_id", "combined_ref", OverlapStrategy::First},
                {TransformationType::IntervalCount, "total_count", "combined_ref"}};

        auto result = aggregateData(row_intervals, transformations, reference_data, {}, {});

        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 5);

        REQUIRE(result[0][0] == 100.0);// start
        REQUIRE(result[0][1] == 300.0);// end
        REQUIRE(result[0][2] == 201.0);// duration (300-100+1)
        REQUIRE(result[0][3] == 0.0);  // first_id (first overlapping interval)
        REQUIRE(result[0][4] == 2.0);  // total_count (2 overlapping intervals)
    }
}

TEST_CASE("DataAggregation - Analog time series transformations", "[data_aggregation][analog]") {
    SECTION("Basic analog transformations") {
        // Create test analog data: values 1.0, 2.0, 3.0, 4.0, 5.0 at times 0, 1, 2, 3, 4
        std::vector<float> analog_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_data, analog_data.size());

        std::vector<Interval> row_intervals = {
                {1, 3}// Should include values 2.0, 3.0, 4.0 (indices 1, 2, 3)
        };

        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
                {"test_analog", analog_series}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::AnalogMean, "mean", "test_analog"},
                {TransformationType::AnalogMin, "min", "test_analog"},
                {TransformationType::AnalogMax, "max", "test_analog"},
                {TransformationType::AnalogStdDev, "std", "test_analog"}};

        auto result = aggregateData(row_intervals, transformations, {}, reference_analog, {});

        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 4);

        // Expected values for data [2.0, 3.0, 4.0]:
        REQUIRE_THAT(result[0][0], Catch::Matchers::WithinRel(3.0, 1e-3));   // mean = (2+3+4)/3 = 3.0
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(2.0, 1e-3));   // min = 2.0
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(4.0, 1e-3));   // max = 4.0
        REQUIRE_THAT(result[0][3], Catch::Matchers::WithinRel(0.8165, 1e-3));// std dev ≈ 0.8165
    }

    SECTION("Multiple intervals with same analog data") {
        std::vector<float> analog_data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_data, analog_data.size());

        std::vector<Interval> row_intervals = {
                {0, 1},// Values: 10.0, 20.0
                {2, 4},// Values: 30.0, 40.0, 50.0
                {5, 5} // Values: 60.0
        };

        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
                {"multi_analog", analog_series}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::AnalogMean, "mean", "multi_analog"},
                {TransformationType::AnalogMax, "max", "multi_analog"}};

        auto result = aggregateData(row_intervals, transformations, {}, reference_analog, {});

        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 3);

        // Row 1: interval (0,1), values [10.0, 20.0]
        REQUIRE(result[0][0] == 0.0);                                      // start
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(15.0, 1e-3));// mean = (10+20)/2 = 15
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(20.0, 1e-3));// max = 20

        // Row 2: interval (2,4), values [30.0, 40.0, 50.0]
        REQUIRE(result[1][0] == 2.0);                                      // start
        REQUIRE_THAT(result[1][1], Catch::Matchers::WithinRel(40.0, 1e-3));// mean = (30+40+50)/3 = 40
        REQUIRE_THAT(result[1][2], Catch::Matchers::WithinRel(50.0, 1e-3));// max = 50

        // Row 3: interval (5,5), values [60.0]
        REQUIRE(result[2][0] == 5.0);                                      // start
        REQUIRE_THAT(result[2][1], Catch::Matchers::WithinRel(60.0, 1e-3));// mean = 60
        REQUIRE_THAT(result[2][2], Catch::Matchers::WithinRel(60.0, 1e-3));// max = 60
    }

    SECTION("Mixed interval and analog transformations") {
        // Create both interval and analog reference data
        std::vector<Interval> ref_intervals = {{0, 5}, {250, 350}};
        std::vector<float> analog_data = {1.0f, 4.0f, 2.0f, 8.0f, 3.0f, 6.0f};
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_data, analog_data.size());

        std::vector<Interval> row_intervals = {
                {1, 3}// Will overlap with ref_intervals[0] and include analog values [4.0, 2.0, 8.0]
        };

        std::map<std::string, std::vector<Interval>> reference_intervals = {
                {"intervals", ref_intervals}};
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
                {"analog", analog_series}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalID, "interval_id", "intervals"},
                {TransformationType::AnalogMean, "analog_mean", "analog"},
                {TransformationType::AnalogMin, "analog_min", "analog"}};

        auto result = aggregateData(row_intervals, transformations, reference_intervals, reference_analog, {});

        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 4);

        REQUIRE(result[0][0] == 1.0);                                       // start
        REQUIRE(result[0][1] == 0.0);                                       // interval_id (first interval)
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(4.667, 1e-3));// analog mean ≈ (4+2+8)/3
        REQUIRE_THAT(result[0][3], Catch::Matchers::WithinRel(2.0, 1e-3));  // analog min = 2.0
    }

    SECTION("Analog transformations - error cases") {
        std::vector<Interval> row_intervals = {{0, 2}};

        SECTION("Missing analog reference data") {
            std::vector<TransformationConfig> transformations = {
                    {TransformationType::AnalogMean, "mean", "nonexistent"}};

            auto result = aggregateData(row_intervals, transformations, {}, {}, {});
            REQUIRE(std::isnan(result[0][0]));
        }

        SECTION("Null analog data pointer") {
            std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
                    {"null_data", nullptr}};

            std::vector<TransformationConfig> transformations = {
                    {TransformationType::AnalogMean, "mean", "null_data"}};

            auto result = aggregateData(row_intervals, transformations, {}, reference_analog, {});
            REQUIRE(std::isnan(result[0][0]));
        }
    }
}

TEST_CASE("DataAggregation - Point data transformations", "[data_aggregation][points]") {
    SECTION("Basic point mean transformations") {
        // Create test point data
        auto point_data = std::make_shared<PointData>();
        
        // Add points at different times within the interval
        point_data->addPointAtTime(1, {10.0f, 20.0f}, false);  // Time 1: (10, 20)
        point_data->addPointAtTime(2, {30.0f, 40.0f}, false);  // Time 2: (30, 40)
        point_data->addPointAtTime(3, {50.0f, 60.0f}, false);  // Time 3: (50, 60)
        
        std::vector<Interval> row_intervals = {
            {1, 3}  // Should include all three points
        };
        
        std::map<std::string, std::shared_ptr<PointData>> reference_points = {
            {"test_points", point_data}
        };
        
        std::vector<TransformationConfig> transformations = {
            {TransformationType::PointMeanX, "mean_x", "test_points"},
            {TransformationType::PointMeanY, "mean_y", "test_points"}
        };
        
        auto result = aggregateData(row_intervals, transformations, {}, {}, reference_points);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 2);
        
        // Expected mean X = (10 + 30 + 50) / 3 = 30.0
        // Expected mean Y = (20 + 40 + 60) / 3 = 40.0
        REQUIRE_THAT(result[0][0], Catch::Matchers::WithinRel(30.0, 1e-3));  // mean_x
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(40.0, 1e-3));  // mean_y
    }
    
    SECTION("Multiple points at same time") {
        auto point_data = std::make_shared<PointData>();
        
        // Add multiple points at the same time
        std::vector<Point2D<float>> points_at_time_1 = {{10.0f, 20.0f}, {30.0f, 40.0f}};
        std::vector<Point2D<float>> points_at_time_2 = {{50.0f, 60.0f}, {70.0f, 80.0f}};
        
        point_data->addPointsAtTime(1, points_at_time_1, false);
        point_data->addPointsAtTime(2, points_at_time_2, false);
        
        std::vector<Interval> row_intervals = {
            {1, 2}  // Should include all four points
        };
        
        std::map<std::string, std::shared_ptr<PointData>> reference_points = {
            {"multi_points", point_data}
        };
        
        std::vector<TransformationConfig> transformations = {
            {TransformationType::IntervalStart, "start"},
            {TransformationType::PointMeanX, "mean_x", "multi_points"},
            {TransformationType::PointMeanY, "mean_y", "multi_points"}
        };
        
        auto result = aggregateData(row_intervals, transformations, {}, {}, reference_points);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 3);
        
        REQUIRE(result[0][0] == 1.0);  // start
        
        // Expected mean X = (10 + 30 + 50 + 70) / 4 = 40.0
        // Expected mean Y = (20 + 40 + 60 + 80) / 4 = 50.0
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(40.0, 1e-3));  // mean_x
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(50.0, 1e-3));  // mean_y
    }
    
    SECTION("Mixed interval, analog, and point transformations") {
        // Create interval reference data
        std::vector<Interval> ref_intervals = {{0, 5}, {250, 350}};
        
        // Create analog reference data
        std::vector<float> analog_data = {1.0f, 4.0f, 2.0f, 8.0f, 3.0f, 6.0f};
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_data, analog_data.size());
        
        // Create point reference data
        auto point_data = std::make_shared<PointData>();
        point_data->addPointAtTime(1, {100.0f, 200.0f}, false);
        point_data->addPointAtTime(2, {300.0f, 400.0f}, false);
        point_data->addPointAtTime(3, {500.0f, 600.0f}, false);
        
        std::vector<Interval> row_intervals = {
            {1, 3}  // Will overlap with ref_intervals[0], include analog values, and include 3 points
        };
        
        std::map<std::string, std::vector<Interval>> reference_intervals = {
            {"intervals", ref_intervals}
        };
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
            {"analog", analog_series}
        };
        std::map<std::string, std::shared_ptr<PointData>> reference_points = {
            {"points", point_data}
        };
        
        std::vector<TransformationConfig> transformations = {
            {TransformationType::IntervalStart, "start"},
            {TransformationType::IntervalID, "interval_id", "intervals"},
            {TransformationType::AnalogMean, "analog_mean", "analog"},
            {TransformationType::PointMeanX, "point_mean_x", "points"},
            {TransformationType::PointMeanY, "point_mean_y", "points"}
        };
        
        auto result = aggregateData(row_intervals, transformations, reference_intervals, reference_analog, reference_points);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 5);
        
        REQUIRE(result[0][0] == 1.0);     // start
        REQUIRE(result[0][1] == 0.0);     // interval_id (first interval)
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(4.667, 1e-3));  // analog mean ≈ (4+2+8)/3
        
        // Point means: X = (100+300+500)/3 = 300.0, Y = (200+400+600)/3 = 400.0
        REQUIRE_THAT(result[0][3], Catch::Matchers::WithinRel(300.0, 1e-3));  // point_mean_x
        REQUIRE_THAT(result[0][4], Catch::Matchers::WithinRel(400.0, 1e-3));  // point_mean_y
    }
    
    SECTION("Point transformations - error cases") {
        std::vector<Interval> row_intervals = {{0, 2}};
        
        SECTION("Missing point reference data") {
            std::vector<TransformationConfig> transformations = {
                {TransformationType::PointMeanX, "mean_x", "nonexistent"}
            };
            
            auto result = aggregateData(row_intervals, transformations, {}, {}, {});
            REQUIRE(std::isnan(result[0][0]));
        }
        
        SECTION("Null point data pointer") {
            std::map<std::string, std::shared_ptr<PointData>> reference_points = {
                {"null_data", nullptr}
            };
            
            std::vector<TransformationConfig> transformations = {
                {TransformationType::PointMeanX, "mean_x", "null_data"}
            };
            
            auto result = aggregateData(row_intervals, transformations, {}, {}, reference_points);
            REQUIRE(std::isnan(result[0][0]));
        }
        
        SECTION("No points in interval") {
            auto point_data = std::make_shared<PointData>();
            // Add points outside the interval
            point_data->addPointAtTime(10, {100.0f, 200.0f}, false);
            
            std::map<std::string, std::shared_ptr<PointData>> reference_points = {
                {"empty_interval", point_data}
            };
            
            std::vector<TransformationConfig> transformations = {
                {TransformationType::PointMeanX, "mean_x", "empty_interval"},
                {TransformationType::PointMeanY, "mean_y", "empty_interval"}
            };
            
            auto result = aggregateData(row_intervals, transformations, {}, {}, reference_points);
            REQUIRE(std::isnan(result[0][0]));  // mean_x should be NaN
            REQUIRE(std::isnan(result[0][1]));  // mean_y should be NaN
        }
    }
}

TEST_CASE("DataAggregation - Time index vs array index bug test", "[data_aggregation][time_index_bug]") {
    SECTION("Analog data with non-sequential time indices") {
        // Create analog data where time indices don't match array positions
        // This tests the bug where we were using array positions instead of time indices
        std::vector<float> analog_values = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        std::vector<TimeFrameIndex> analog_times = {TimeFrameIndex(100), 
                 TimeFrameIndex(200), 
                 TimeFrameIndex(300), 
                 TimeFrameIndex(400), 
                 TimeFrameIndex(500)}; // Non-sequential, sparse times
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, analog_times);
        
        // Test interval that should include times 200, 300, 400 (values 20.0, 30.0, 40.0)
        std::vector<Interval> row_intervals = {
            {200, 400}  // Should capture analog data at times 200, 300, 400
        };
        
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
            {"sparse_analog", analog_series}
        };
        
        std::vector<TransformationConfig> transformations = {
            {TransformationType::AnalogMean, "mean", "sparse_analog"},
            {TransformationType::AnalogMin, "min", "sparse_analog"},
            {TransformationType::AnalogMax, "max", "sparse_analog"}
        };
        
        auto result = aggregateData(row_intervals, transformations, {}, reference_analog, {});
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 3);
        
        // Expected: values 20.0, 30.0, 40.0 from times 200, 300, 400
        // Mean = (20 + 30 + 40) / 3 = 30.0
        // Min = 20.0
        // Max = 40.0
        REQUIRE_THAT(result[0][0], Catch::Matchers::WithinRel(30.0, 1e-3));  // mean
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(20.0, 1e-3));  // min  
        REQUIRE_THAT(result[0][2], Catch::Matchers::WithinRel(40.0, 1e-3));  // max
    }
    
    SECTION("Analog data with gaps - test edge case") {
        // Test case where interval spans a gap in time indices
        std::vector<float> analog_values = {100.0f, 200.0f, 300.0f};
        std::vector<TimeFrameIndex> analog_times = {TimeFrameIndex(10), 
                 TimeFrameIndex(50), 
                 TimeFrameIndex(90)}; // Large gaps between samples
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, analog_times);
        
        std::vector<Interval> row_intervals = {
            {0, 20},   // Should only capture first sample (value 100.0 at time 10)
            {45, 95},  // Should capture second and third samples (values 200.0, 300.0 at times 50, 90)
            {100, 200} // Should capture no samples
        };
        
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
            {"gapped_analog", analog_series}
        };
        
        std::vector<TransformationConfig> transformations = {
            {TransformationType::AnalogMean, "mean", "gapped_analog"},
            {TransformationType::AnalogMin, "min", "gapped_analog"}
        };
        
        auto result = aggregateData(row_intervals, transformations, {}, reference_analog, {});
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 2);
        
        // Row 1: interval [0, 20] - should capture value 100.0 at time 10
        REQUIRE_THAT(result[0][0], Catch::Matchers::WithinRel(100.0, 1e-3));  // mean = 100.0
        REQUIRE_THAT(result[0][1], Catch::Matchers::WithinRel(100.0, 1e-3));  // min = 100.0
        
        // Row 2: interval [45, 95] - should capture values 200.0, 300.0 at times 50, 90
        REQUIRE_THAT(result[1][0], Catch::Matchers::WithinRel(250.0, 1e-3));  // mean = (200+300)/2 = 250.0
        REQUIRE_THAT(result[1][1], Catch::Matchers::WithinRel(200.0, 1e-3));  // min = 200.0
        
        // Row 3: interval [100, 200] - should capture no samples
        REQUIRE(std::isnan(result[2][0]));  // mean should be NaN
        REQUIRE(std::isnan(result[2][1]));  // min should be NaN
    }
}

TEST_CASE("DataAggregation - Complex mixed transformations", "[data_aggregation][complex_mixed]") {
    SECTION("All transformation types together") {
        // Set up complex test scenario with multiple data types
        std::vector<Interval> row_intervals = {
                {100, 200},
                {300, 400}};

        // Interval reference data
        std::vector<Interval> ref_intervals = {
                {50, 150},// Overlaps with first row interval
                {350, 450}// Overlaps with second row interval
        };

        // Analog reference data with values at indices corresponding to time
        std::vector<float> analog_values;
        std::vector<TimeFrameIndex> analog_times;
        for (int i = 0; i <= 500; ++i) {
            analog_values.push_back(static_cast<float>(std::sin(i * 0.1) + i * 0.01));
            analog_times.push_back(TimeFrameIndex(i));
        }
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, analog_times);

        std::map<std::string, std::vector<Interval>> reference_intervals = {
                {"test_intervals", ref_intervals}};
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {
                {"test_analog", analog_series}};

        std::vector<TransformationConfig> transformations = {
                {TransformationType::IntervalStart, "start"},
                {TransformationType::IntervalEnd, "end"},
                {TransformationType::IntervalDuration, "duration"},
                {TransformationType::IntervalID, "ref_id", "test_intervals"},
                {TransformationType::IntervalCount, "ref_count", "test_intervals"},
                {TransformationType::AnalogMean, "analog_mean", "test_analog"},
                {TransformationType::AnalogMin, "analog_min", "test_analog"},
                {TransformationType::AnalogMax, "analog_max", "test_analog"},
                {TransformationType::AnalogStdDev, "analog_std", "test_analog"}};

        auto result = aggregateData(row_intervals, transformations, reference_intervals, reference_analog, {});

        REQUIRE(result.size() == 2);
        REQUIRE(result[0].size() == 9);

        // Verify basic interval properties
        REQUIRE(result[0][0] == 100.0);// start
        REQUIRE(result[0][1] == 200.0);// end
        REQUIRE(result[0][2] == 101.0);// duration
        REQUIRE(result[0][3] == 0.0);  // ref_id (first interval)
        REQUIRE(result[0][4] == 1.0);  // ref_count (one overlapping interval)

        // Analog values should be reasonable (not NaN)
        REQUIRE_FALSE(std::isnan(result[0][5]));// analog_mean
        REQUIRE_FALSE(std::isnan(result[0][6]));// analog_min
        REQUIRE_FALSE(std::isnan(result[0][7]));// analog_max
        REQUIRE_FALSE(std::isnan(result[0][8]));// analog_std

        // Second row
        REQUIRE(result[1][0] == 300.0);// start
        REQUIRE(result[1][1] == 400.0);// end
        REQUIRE(result[1][2] == 101.0);// duration
        REQUIRE(result[1][3] == 1.0);  // ref_id (second interval)
        REQUIRE(result[1][4] == 1.0);  // ref_count (one overlapping interval)

        // Analog values should be reasonable (not NaN) and different from first row
        REQUIRE_FALSE(std::isnan(result[1][5]));// analog_mean
        REQUIRE_FALSE(std::isnan(result[1][6]));// analog_min
        REQUIRE_FALSE(std::isnan(result[1][7]));// analog_max
        REQUIRE_FALSE(std::isnan(result[1][8]));// analog_std
    }
}
