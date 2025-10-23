#include "digital_interval_boolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/data_transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

TEST_CASE("Data Transform: Digital Interval Boolean Transform - AND Operation", "[transforms][digital_interval_boolean]") {
    BooleanParams params;
    BooleanOperation operation;

    SECTION("Basic AND - overlapping intervals") {
        // Input: (1,5), (10,15)
        // Other: (3,7), (12,20)
        // AND: (3,5), (12,15)
        std::vector<Interval> input_intervals = {{1, 5}, {10, 15}};
        std::vector<Interval> other_intervals = {{3, 7}, {12, 20}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 2);

        REQUIRE(result_intervals[0].start == 3);
        REQUIRE(result_intervals[0].end == 5);
        REQUIRE(result_intervals[1].start == 12);
        REQUIRE(result_intervals[1].end == 15);
    }

    SECTION("AND - no overlap") {
        // Input: (1,5)
        // Other: (10,15)
        // AND: empty
        std::vector<Interval> input_intervals = {{1, 5}};
        std::vector<Interval> other_intervals = {{10, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.empty());
    }

    SECTION("AND - complete overlap") {
        // Input: (1,10)
        // Other: (1,10)
        // AND: (1,10)
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{1, 10}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 10);
    }

    SECTION("AND - one series subset of other") {
        // Input: (5,15)
        // Other: (1,20)
        // AND: (5,15)
        std::vector<Interval> input_intervals = {{5, 15}};
        std::vector<Interval> other_intervals = {{1, 20}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 5);
        REQUIRE(result_intervals[0].end == 15);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - OR Operation", "[transforms][digital_interval_boolean]") {
    BooleanParams params;

    SECTION("Basic OR - separate intervals") {
        // Input: (1,5)
        // Other: (10,15)
        // OR: (1,5), (10,15)
        std::vector<Interval> input_intervals = {{1, 5}};
        std::vector<Interval> other_intervals = {{10, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 2);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 5);
        REQUIRE(result_intervals[1].start == 10);
        REQUIRE(result_intervals[1].end == 15);
    }

    SECTION("OR - overlapping intervals merge") {
        // Input: (1,10)
        // Other: (5,15)
        // OR: (1,15)
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{5, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 15);
    }

    SECTION("OR - multiple intervals with gaps") {
        // Input: (1,5), (15,20)
        // Other: (8,12), (18,25)
        // OR: (1,5), (8,12), (15,25)
        std::vector<Interval> input_intervals = {{1, 5}, {15, 20}};
        std::vector<Interval> other_intervals = {{8, 12}, {18, 25}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 3);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 5);
        REQUIRE(result_intervals[1].start == 8);
        REQUIRE(result_intervals[1].end == 12);
        REQUIRE(result_intervals[2].start == 15);
        REQUIRE(result_intervals[2].end == 25);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - XOR Operation", "[transforms][digital_interval_boolean]") {
    BooleanParams params;

    SECTION("Basic XOR - no overlap") {
        // Input: (1,5)
        // Other: (10,15)
        // XOR: (1,5), (10,15) - both present, not overlapping
        std::vector<Interval> input_intervals = {{1, 5}};
        std::vector<Interval> other_intervals = {{10, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::XOR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 2);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 5);
        REQUIRE(result_intervals[1].start == 10);
        REQUIRE(result_intervals[1].end == 15);
    }

    SECTION("XOR - partial overlap excludes overlap") {
        // Input: (1,10)
        // Other: (5,15)
        // XOR: (1,4), (11,15) - excludes (5,10) where both are true
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{5, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::XOR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 2);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 4);
        REQUIRE(result_intervals[1].start == 11);
        REQUIRE(result_intervals[1].end == 15);
    }

    SECTION("XOR - complete overlap results in nothing") {
        // Input: (1,10)
        // Other: (1,10)
        // XOR: empty - both true everywhere
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{1, 10}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::XOR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.empty());
    }

    SECTION("XOR - complex pattern") {
        // Input: (1,5), (10,15)
        // Other: (3,12)
        // XOR: (1,2), (6,9), (13,15)
        std::vector<Interval> input_intervals = {{1, 5}, {10, 15}};
        std::vector<Interval> other_intervals = {{3, 12}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::XOR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 3);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 2);
        REQUIRE(result_intervals[1].start == 6);
        REQUIRE(result_intervals[1].end == 9);
        REQUIRE(result_intervals[2].start == 13);
        REQUIRE(result_intervals[2].end == 15);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - NOT Operation", "[transforms][digital_interval_boolean]") {
    BooleanParams params;

    SECTION("NOT - single interval") {
        // Input: (5,10)
        // NOT: (5,10) inverted within its own range - empty (all covered)
        // Actually, NOT should invert within the defined range
        // If interval is (5,10), NOT gives nothing in that range
        // This is tricky - we need to define the "universe"
        // Let's assume NOT inverts within the min-max range of the input
        std::vector<Interval> input_intervals = {{5, 10}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::NOT;
        // other_series is ignored for NOT

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        // Within range (5,10), everything is true, so NOT gives empty
        REQUIRE(result_intervals.empty());
    }

    SECTION("NOT - intervals with gaps") {
        // Input: (1,5), (10,15)
        // Range is (1,15)
        // NOT: (6,9) - the gap between intervals
        std::vector<Interval> input_intervals = {{1, 5}, {10, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::NOT;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 6);
        REQUIRE(result_intervals[0].end == 9);
    }

    SECTION("NOT - multiple gaps") {
        // Input: (1,3), (5,7), (9,11)
        // Range is (1,11)
        // NOT: (4,4), (8,8)
        std::vector<Interval> input_intervals = {{1, 3}, {5, 7}, {9, 11}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::NOT;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 2);

        REQUIRE(result_intervals[0].start == 4);
        REQUIRE(result_intervals[0].end == 4);
        REQUIRE(result_intervals[1].start == 8);
        REQUIRE(result_intervals[1].end == 8);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - AND_NOT Operation", "[transforms][digital_interval_boolean]") {
    BooleanParams params;

    SECTION("AND_NOT - subtract overlapping portion") {
        // Input: (1,10)
        // Other: (5,15)
        // AND_NOT: (1,4) - input where other is false
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{5, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND_NOT;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 4);
    }

    SECTION("AND_NOT - no overlap keeps input") {
        // Input: (1,5)
        // Other: (10,15)
        // AND_NOT: (1,5) - input unchanged
        std::vector<Interval> input_intervals = {{1, 5}};
        std::vector<Interval> other_intervals = {{10, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND_NOT;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 5);
    }

    SECTION("AND_NOT - complete overlap removes everything") {
        // Input: (5,10)
        // Other: (1,15)
        // AND_NOT: empty
        std::vector<Interval> input_intervals = {{5, 10}};
        std::vector<Interval> other_intervals = {{1, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND_NOT;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.empty());
    }

    SECTION("AND_NOT - punch holes in input") {
        // Input: (1,20)
        // Other: (5,8), (12,15)
        // AND_NOT: (1,4), (9,11), (16,20)
        std::vector<Interval> input_intervals = {{1, 20}};
        std::vector<Interval> other_intervals = {{5, 8}, {12, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND_NOT;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 3);

        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 4);
        REQUIRE(result_intervals[1].start == 9);
        REQUIRE(result_intervals[1].end == 11);
        REQUIRE(result_intervals[2].start == 16);
        REQUIRE(result_intervals[2].end == 20);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - Class Tests", "[transforms][digital_interval_boolean][operation]") {
    BooleanOperation operation;
    BooleanParams params;
    DataTypeVariant variant;

    SECTION("Operation name and type info") {
        REQUIRE(operation.getName() == "Boolean Operation");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<DigitalIntervalSeries>));
    }

    SECTION("Execute with valid data - AND operation") {
        std::vector<Interval> input_intervals = {{1, 10}};
        std::vector<Interval> other_intervals = {{5, 15}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        variant = input_dis;
        auto result_variant = operation.execute(variant, &params);

        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalIntervalSeries>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        REQUIRE(result_intervals[0].start == 5);
        REQUIRE(result_intervals[0].end == 10);
    }

    SECTION("Execute with default parameters") {
        std::vector<Interval> input_intervals = {{1, 10}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        
        variant = input_dis;
        
        // Execute with nullptr parameters - should use defaults
        // Default is AND, but with no other_series it should return empty
        auto result_variant = operation.execute(variant, nullptr);
        
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalIntervalSeries>>(result_variant);
        REQUIRE(result != nullptr);
        // Without other_series for AND, result should be empty
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("getDefaultParameters") {
        auto default_params = operation.getDefaultParameters();
        REQUIRE(default_params != nullptr);
        
        auto * bool_params = dynamic_cast<BooleanParams *>(default_params.get());
        REQUIRE(bool_params != nullptr);
        REQUIRE(bool_params->operation == BooleanParams::BooleanOperation::AND);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - Edge Cases and Error Handling", "[transforms][digital_interval_boolean][edge_cases]") {
    BooleanParams params;

    SECTION("Empty input series") {
        std::vector<Interval> input_intervals = {};
        std::vector<Interval> other_intervals = {{1, 10}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        // OR with empty input and non-empty other should give the other
        REQUIRE(result->getDigitalIntervalSeries().empty() == false);
    }

    SECTION("Both series empty") {
        std::vector<Interval> input_intervals = {};
        std::vector<Interval> other_intervals = {};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("Null input pointer") {
        params.operation = BooleanParams::BooleanOperation::AND;
        
        auto result = apply_boolean_operation(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("Null other_series for AND") {
        std::vector<Interval> input_intervals = {{1, 10}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = nullptr;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("NOT with empty series") {
        std::vector<Interval> input_intervals = {};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::NOT;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }
}

TEST_CASE("Progress Callback", "[transforms][digital_interval_boolean][progress]") {
    BooleanParams params;
    
    SECTION("Progress callback is invoked") {
        std::vector<Interval> input_intervals = {{1, 100}};
        std::vector<Interval> other_intervals = {{50, 150}};
        
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;

        std::vector<int> progress_values;
        auto callback = [&progress_values](int progress) {
            progress_values.push_back(progress);
        };

        auto result = apply_boolean_operation(input_dis.get(), params, callback);
        REQUIRE(result != nullptr);
        
        // Verify that progress was reported
        REQUIRE(!progress_values.empty());
        REQUIRE(progress_values.back() == 100);
    }
}

TEST_CASE("Data Transform: Digital Interval Boolean Transform - TimeFrame Conversion", "[transforms][digital_interval_boolean][timeframe]") {
    BooleanParams params;
    
    SECTION("AND operation with upsampling (input has higher sampling rate)") {
        // Input TimeFrame: 1ms sampling (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
        std::vector<int> input_times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto input_timeframe = std::make_shared<TimeFrame>(input_times);
        
        // Other TimeFrame: 2ms sampling (0, 2, 4, 6, 8, 10)
        std::vector<int> other_times = {0, 2, 4, 6, 8, 10};
        auto other_timeframe = std::make_shared<TimeFrame>(other_times);
        
        // Input intervals in indices: (2,5) means times 2-5ms
        std::vector<Interval> input_intervals = {{2, 5}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(input_timeframe);
        
        // Other intervals in indices: (1,3) means indices 1-3, which are times 2-6ms
        std::vector<Interval> other_intervals = {{1, 3}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        other_dis->setTimeFrame(other_timeframe);
        
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == input_timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        
        // Input (2,5) AND Other (2,6) in input timeframe = (2,5)
        REQUIRE(result_intervals[0].start == 2);
        REQUIRE(result_intervals[0].end == 5);
    }
    
    SECTION("AND operation with downsampling (input has lower sampling rate)") {
        // Input TimeFrame: 2ms sampling (0, 2, 4, 6, 8, 10)
        std::vector<int> input_times = {0, 2, 4, 6, 8, 10};
        auto input_timeframe = std::make_shared<TimeFrame>(input_times);
        
        // Other TimeFrame: 1ms sampling (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
        std::vector<int> other_times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto other_timeframe = std::make_shared<TimeFrame>(other_times);
        
        // Input intervals in indices: (1,3) means times 2-6ms
        std::vector<Interval> input_intervals = {{1, 3}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(input_timeframe);
        
        // Other intervals in indices: (3,7) means times 3-7ms
        std::vector<Interval> other_intervals = {{3, 7}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        other_dis->setTimeFrame(other_timeframe);
        
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == input_timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        
        // Input (1,3) in input timeframe = times [2, 4, 6]ms
        // Other (3,7) in other timeframe = times [3, 4, 5, 6, 7]ms
        // After conversion, other becomes (2,3) in input timeframe = times [4, 6]ms
        // (because 3ms converts to index 2 at time 4ms, and 7ms converts to index 3 at time 6ms)
        // AND result: overlap of [2, 4, 6]ms and [4, 6]ms = [4, 6]ms = indices (2,3)
        REQUIRE(result_intervals[0].start == 2);
        REQUIRE(result_intervals[0].end == 3);
    }
    
    SECTION("OR operation with different sampling rates") {
        // Input TimeFrame: 1ms sampling
        std::vector<int> input_times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto input_timeframe = std::make_shared<TimeFrame>(input_times);
        
        // Other TimeFrame: 3ms sampling
        std::vector<int> other_times = {0, 3, 6, 9};
        auto other_timeframe = std::make_shared<TimeFrame>(other_times);
        
        // Input: (1,3) = times 1-3ms
        std::vector<Interval> input_intervals = {{1, 3}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(input_timeframe);
        
        // Other: (2,3) = indices 2-3 = times 6-9ms
        std::vector<Interval> other_intervals = {{2, 3}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        other_dis->setTimeFrame(other_timeframe);
        
        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == input_timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        // Should have two separate intervals since they don't overlap
        REQUIRE(result_intervals.size() == 2);
        
        REQUIRE(result_intervals[0].start == 1);
        REQUIRE(result_intervals[0].end == 3);
        REQUIRE(result_intervals[1].start == 6);
        REQUIRE(result_intervals[1].end == 9);
    }
    
    SECTION("XOR operation with different sampling rates") {
        // Input TimeFrame: 1ms sampling
        std::vector<int> input_times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto input_timeframe = std::make_shared<TimeFrame>(input_times);
        
        // Other TimeFrame: 2ms sampling
        std::vector<int> other_times = {0, 2, 4, 6, 8, 10};
        auto other_timeframe = std::make_shared<TimeFrame>(other_times);
        
        // Input: (2,7) = times 2-7ms
        std::vector<Interval> input_intervals = {{2, 7}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(input_timeframe);
        
        // Other: (2,4) = indices 2-4 = times 4-8ms
        std::vector<Interval> other_intervals = {{2, 4}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        other_dis->setTimeFrame(other_timeframe);
        
        params.operation = BooleanParams::BooleanOperation::XOR;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == input_timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        // XOR should exclude the overlap
        REQUIRE(result_intervals.size() == 2);
        
        // (2,7) XOR (4,8) = (2,3) and (8,8)
        REQUIRE(result_intervals[0].start == 2);
        REQUIRE(result_intervals[0].end == 3);
        REQUIRE(result_intervals[1].start == 8);
        REQUIRE(result_intervals[1].end == 8);
    }
    
    SECTION("Same TimeFrame object - no conversion needed") {
        std::vector<int> times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeframe = std::make_shared<TimeFrame>(times);
        
        std::vector<Interval> input_intervals = {{2, 5}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(timeframe);
        
        std::vector<Interval> other_intervals = {{4, 7}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        other_dis->setTimeFrame(timeframe);
        
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        
        REQUIRE(result_intervals[0].start == 4);
        REQUIRE(result_intervals[0].end == 5);
    }
    
    SECTION("No TimeFrame - indices used directly") {
        std::vector<Interval> input_intervals = {{2, 5}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        
        std::vector<Interval> other_intervals = {{4, 7}};
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other_dis;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == nullptr);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        
        REQUIRE(result_intervals[0].start == 4);
        REQUIRE(result_intervals[0].end == 5);
    }
    
    SECTION("NOT operation preserves input TimeFrame") {
        std::vector<int> times = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeframe = std::make_shared<TimeFrame>(times);
        
        std::vector<Interval> input_intervals = {{2, 4}, {7, 9}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        input_dis->setTimeFrame(timeframe);
        
        params.operation = BooleanParams::BooleanOperation::NOT;
        
        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == timeframe);
        
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 1);
        
        REQUIRE(result_intervals[0].start == 5);
        REQUIRE(result_intervals[0].end == 6);
    }
}
