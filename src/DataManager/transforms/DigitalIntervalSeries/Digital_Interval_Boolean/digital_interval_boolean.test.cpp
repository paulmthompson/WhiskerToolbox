#include "digital_interval_boolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/data_transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

#include "fixtures/scenarios/digital/digital_interval_boolean_scenarios.hpp"

// ============================================================================
// Tests using scenarios (for V1/V2 parity)
// ============================================================================

TEST_CASE("V1 Transform: Digital Interval Boolean - AND Operation",
          "[transforms][v1][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    params.operation = BooleanParams::BooleanOperation::AND;

    SECTION("Basic AND - overlapping intervals") {
        auto [input, other] = and_overlapping();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 3);
        REQUIRE(result_intervals[0].value().end == 5);
        REQUIRE(result_intervals[1].value().start == 12);
        REQUIRE(result_intervals[1].value().end == 15);
    }

    SECTION("AND - no overlap") {
        auto [input, other] = and_no_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("AND - complete overlap") {
        auto [input, other] = and_complete_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 10);
    }

    SECTION("AND - one series subset of other") {
        auto [input, other] = and_subset();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 5);
        REQUIRE(result_intervals[0].value().end == 15);
    }
}

TEST_CASE("V1 Transform: Digital Interval Boolean - OR Operation",
          "[transforms][v1][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    params.operation = BooleanParams::BooleanOperation::OR;

    SECTION("Basic OR - separate intervals") {
        auto [input, other] = or_separate();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 5);
        REQUIRE(result_intervals[1].value().start == 10);
        REQUIRE(result_intervals[1].value().end == 15);
    }

    SECTION("OR - overlapping intervals merge") {
        auto [input, other] = or_overlapping_merge();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 15);
    }

    SECTION("OR - multiple intervals with gaps") {
        auto [input, other] = or_multiple_with_gaps();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 3);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 5);
        REQUIRE(result_intervals[1].value().start == 8);
        REQUIRE(result_intervals[1].value().end == 12);
        REQUIRE(result_intervals[2].value().start == 15);
        REQUIRE(result_intervals[2].value().end == 25);
    }
}

TEST_CASE("V1 Transform: Digital Interval Boolean - XOR Operation",
          "[transforms][v1][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    params.operation = BooleanParams::BooleanOperation::XOR;

    SECTION("Basic XOR - no overlap") {
        auto [input, other] = xor_no_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 5);
        REQUIRE(result_intervals[1].value().start == 10);
        REQUIRE(result_intervals[1].value().end == 15);
    }

    SECTION("XOR - partial overlap excludes overlap") {
        auto [input, other] = xor_partial_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 4);
        REQUIRE(result_intervals[1].value().start == 11);
        REQUIRE(result_intervals[1].value().end == 15);
    }

    SECTION("XOR - complete overlap results in nothing") {
        auto [input, other] = xor_complete_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("XOR - complex pattern") {
        auto [input, other] = xor_complex();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 3);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 2);
        REQUIRE(result_intervals[1].value().start == 6);
        REQUIRE(result_intervals[1].value().end == 9);
        REQUIRE(result_intervals[2].value().start == 13);
        REQUIRE(result_intervals[2].value().end == 15);
    }
}

TEST_CASE("V1 Transform: Digital Interval Boolean - NOT Operation",
          "[transforms][v1][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    params.operation = BooleanParams::BooleanOperation::NOT;

    SECTION("NOT - single interval") {
        auto input = not_single_interval();

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        // Within range (5,10), everything is true, so NOT gives empty
        REQUIRE(result->view().empty());
    }

    SECTION("NOT - intervals with gaps") {
        auto input = not_with_gaps();

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 6);
        REQUIRE(result_intervals[0].value().end == 9);
    }

    SECTION("NOT - multiple gaps") {
        auto input = not_multiple_gaps();

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 4);
        REQUIRE(result_intervals[0].value().end == 4);
        REQUIRE(result_intervals[1].value().start == 8);
        REQUIRE(result_intervals[1].value().end == 8);
    }
}

TEST_CASE("V1 Transform: Digital Interval Boolean - AND_NOT Operation",
          "[transforms][v1][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    params.operation = BooleanParams::BooleanOperation::AND_NOT;

    SECTION("AND_NOT - subtract overlapping portion") {
        auto [input, other] = and_not_subtract_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 4);
    }

    SECTION("AND_NOT - no overlap keeps input") {
        auto [input, other] = and_not_no_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 5);
    }

    SECTION("AND_NOT - complete overlap removes everything") {
        auto [input, other] = and_not_complete_overlap();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("AND_NOT - punch holes in input") {
        auto [input, other] = and_not_punch_holes();
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 3);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 4);
        REQUIRE(result_intervals[1].value().start == 9);
        REQUIRE(result_intervals[1].value().end == 11);
        REQUIRE(result_intervals[2].value().start == 16);
        REQUIRE(result_intervals[2].value().end == 20);
    }
}

TEST_CASE("V1 Transform: Digital Interval Boolean - Edge Cases",
          "[transforms][v1][digital_interval_boolean][edge_cases]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;

    SECTION("Empty input series") {
        auto [input, other] = empty_input();
        params.operation = BooleanParams::BooleanOperation::OR;
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        // OR with empty input and non-empty other should give the other
        REQUIRE(!result->view().empty());
    }

    SECTION("Both series empty") {
        auto [input, other] = both_empty();
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("Null input pointer") {
        params.operation = BooleanParams::BooleanOperation::AND;

        auto result = apply_boolean_operation(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("NOT with empty series") {
        auto input = not_empty();
        params.operation = BooleanParams::BooleanOperation::NOT;

        auto result = apply_boolean_operation(input.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }
}

TEST_CASE("V1 Transform: Progress Callback",
          "[transforms][v1][digital_interval_boolean][progress]") {
    using namespace digital_interval_boolean_scenarios;
    BooleanParams params;
    
    SECTION("Progress callback is invoked") {
        auto [input, other] = large_intervals();
        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = other;

        std::vector<int> progress_values;
        auto callback = [&progress_values](int progress) {
            progress_values.push_back(progress);
        };

        auto result = apply_boolean_operation(input.get(), params, callback);
        REQUIRE(result != nullptr);
        
        // Verify that progress was reported
        REQUIRE(!progress_values.empty());
        REQUIRE(progress_values.back() == 100);
    }
}

// ============================================================================
// V1-specific tests (Class API, JSON pipeline, parameters)
// ============================================================================

TEST_CASE("Data Transform: Digital Interval Boolean Transform - Class Tests", "[transforms][v1][digital_interval_boolean][operation]") {
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

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 5);
        REQUIRE(result_intervals[0].value().end == 10);
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
        REQUIRE(result->view().empty());
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
        REQUIRE(result->view().empty() == false);
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
        REQUIRE(result->view().empty());
    }

    SECTION("Null input pointer") {
        params.operation = BooleanParams::BooleanOperation::AND;
        
        auto result = apply_boolean_operation(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("Null other_series for AND") {
        std::vector<Interval> input_intervals = {{1, 10}};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::AND;
        params.other_series = nullptr;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("NOT with empty series") {
        std::vector<Interval> input_intervals = {};
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.operation = BooleanParams::BooleanOperation::NOT;

        auto result = apply_boolean_operation(input_dis.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
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
        
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        
        // Input (2,5) AND Other (2,6) in input timeframe = (2,5)
        REQUIRE(result_intervals[0].value().start == 2);
        REQUIRE(result_intervals[0].value().end == 5);
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
        
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        
        // Input (1,3) in input timeframe = times [2, 4, 6]ms
        // Other (3,7) in other timeframe = times [3, 4, 5, 6, 7]ms
        // After conversion, other becomes (2,3) in input timeframe = times [4, 6]ms
        // (because 3ms converts to index 2 at time 4ms, and 7ms converts to index 3 at time 6ms)
        // AND result: overlap of [2, 4, 6]ms and [4, 6]ms = [4, 6]ms = indices (2,3)
        REQUIRE(result_intervals[0].value().start == 2);
        REQUIRE(result_intervals[0].value().end == 3);
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
        
        auto const & result_intervals = result->view();
        // Should have two separate intervals since they don't overlap
        REQUIRE(result->size() == 2);
        
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 3);
        REQUIRE(result_intervals[1].value().start == 6);
        REQUIRE(result_intervals[1].value().end == 9);
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
        
        auto const & result_intervals = result->view();
        // XOR should exclude the overlap
        REQUIRE(result->size() == 2);
        
        // (2,7) XOR (4,8) = (2,3) and (8,8)
        REQUIRE(result_intervals[0].value().start == 2);
        REQUIRE(result_intervals[0].value().end == 3);
        REQUIRE(result_intervals[1].value().start == 8);
        REQUIRE(result_intervals[1].value().end == 8);
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
        
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        
        REQUIRE(result_intervals[0].value().start == 4);
        REQUIRE(result_intervals[0].value().end == 5);
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
        
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        
        REQUIRE(result_intervals[0].value().start == 4);
        REQUIRE(result_intervals[0].value().end == 5);
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
        
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        
        REQUIRE(result_intervals[0].value().start == 5);
        REQUIRE(result_intervals[0].value().end == 6);
    }
}

#include "DataManager.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Digital Interval Boolean - JSON pipeline", "[transforms][digital_interval_boolean][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "boolean_step_1"},
            {"transform_name", "Boolean Operation"},
            {"input_key", "TestIntervals.input"},
            {"output_key", "BooleanResult"},
            {"parameters", {
                {"operation", "AND"},
                {"other_series", "TestIntervals.other"}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test intervals:
    // Input: (1,10), (20,30)
    // Other: (5,15), (25,35)
    // Expected AND result: (5,10), (25,30)
    std::vector<Interval> input_intervals = {{1, 10}, {20, 30}};
    std::vector<Interval> other_intervals = {{5, 15}, {25, 35}};
    
    auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
    input_dis->setTimeFrame(time_frame);
    auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
    other_dis->setTimeFrame(time_frame);
    
    dm.setData("TestIntervals.input", input_dis, TimeKey("default"));
    dm.setData("TestIntervals.other", other_dis, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto result_series = dm.getData<DigitalIntervalSeries>("BooleanResult");
    REQUIRE(result_series != nullptr);

    auto const & result_intervals = result_series->view();
    REQUIRE(result_series->size() == 2);

    // Verify AND operation result
    REQUIRE(result_intervals[0].value().start == 5);
    REQUIRE(result_intervals[0].value().end == 10);
    REQUIRE(result_intervals[1].value().start == 25);
    REQUIRE(result_intervals[1].value().end == 30);
}

TEST_CASE("Data Transform: Digital Interval Boolean - load_data_from_json_config", "[transforms][digital_interval_boolean][json_config]") {
    // Create DataManager and populate it with DigitalIntervalSeries in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test interval data in code
    // Input: (1,10), (20,30)
    // Other: (5,15), (25,35)
    std::vector<Interval> input_intervals = {{1, 10}, {20, 30}};
    std::vector<Interval> other_intervals = {{5, 15}, {25, 35}};
    
    auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
    auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
    
    // Store the interval data in DataManager with known keys
    dm.setData("input_intervals", input_dis, TimeKey("default"));
    dm.setData("other_intervals", other_dis, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Boolean Operations Pipeline\",\n"
        "            \"description\": \"Test boolean operations on digital interval series\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"analysis\",\n"
        "                \"transform_name\": \"Boolean Operation\",\n"
        "                \"phase\": \"1\",\n"
        "                \"input_key\": \"input_intervals\",\n"
        "                \"output_key\": \"and_result\",\n"
        "                \"parameters\": {\n"
        "                    \"operation\": \"AND\",\n"
        "                    \"other_series\": \"other_intervals\"\n"
        "                }\n"
        "            },\n"
        "            {\n"
        "                \"step_id\": \"2\",\n"
        "                \"transform_name\": \"Boolean Operation\",\n"
        "                \"phase\": \"2\",\n"
        "                \"input_key\": \"input_intervals\",\n"
        "                \"output_key\": \"or_result\",\n"
        "                \"parameters\": {\n"
        "                    \"operation\": \"OR\",\n"
        "                    \"other_series\": \"other_intervals\"\n"
        "                }\n"
        "            },\n"
        "            {\n"
        "                \"step_id\": \"3\",\n"
        "                \"transform_name\": \"Boolean Operation\",\n"
        "                \"phase\": \"3\",\n"
        "                \"input_key\": \"input_intervals\",\n"
        "                \"output_key\": \"xor_result\",\n"
        "                \"parameters\": {\n"
        "                    \"operation\": \"XOR\",\n"
        "                    \"other_series\": \"other_intervals\"\n"
        "                }\n"
        "            },\n"
        "            {\n"
        "                \"step_id\": \"4\",\n"
        "                \"transform_name\": \"Boolean Operation\",\n"
        "                \"phase\": \"4\",\n"
        "                \"input_key\": \"input_intervals\",\n"
        "                \"output_key\": \"not_result\",\n"
        "                \"parameters\": {\n"
        "                    \"operation\": \"NOT\"\n"
        "                }\n"
        "            },\n"
        "            {\n"
        "                \"step_id\": \"5\",\n"
        "                \"transform_name\": \"Boolean Operation\",\n"
        "                \"phase\": \"5\",\n"
        "                \"input_key\": \"input_intervals\",\n"
        "                \"output_key\": \"and_not_result\",\n"
        "                \"parameters\": {\n"
        "                    \"operation\": \"AND_NOT\",\n"
        "                    \"other_series\": \"other_intervals\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "digital_interval_boolean_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify AND result: (5,10), (25,30)
    auto and_result = dm.getData<DigitalIntervalSeries>("and_result");
    REQUIRE(and_result != nullptr);
    auto const & and_intervals = and_result->view();
    REQUIRE(and_result->size() == 2);
    REQUIRE(and_intervals[0].value().start == 5);
    REQUIRE(and_intervals[0].value().end == 10);
    REQUIRE(and_intervals[1].value().start == 25);
    REQUIRE(and_intervals[1].value().end == 30);
    
    // Verify OR result: (1,15), (20,35)
    auto or_result = dm.getData<DigitalIntervalSeries>("or_result");
    REQUIRE(or_result != nullptr);
    auto const & or_intervals = or_result->view();
    REQUIRE(or_result->size() == 2);
    REQUIRE(or_intervals[0].value().start == 1);
    REQUIRE(or_intervals[0].value().end == 15);
    REQUIRE(or_intervals[1].value().start == 20);
    REQUIRE(or_intervals[1].value().end == 35);
    
    // Verify XOR result: (1,4), (11,15), (20,24), (31,35)
    auto xor_result = dm.getData<DigitalIntervalSeries>("xor_result");
    REQUIRE(xor_result != nullptr);
    auto const & xor_intervals = xor_result->view();
    REQUIRE(xor_result->size() == 4);
    REQUIRE(xor_intervals[0].value().start == 1);
    REQUIRE(xor_intervals[0].value().end == 4);
    REQUIRE(xor_intervals[1].value().start == 11);
    REQUIRE(xor_intervals[1].value().end == 15);
    REQUIRE(xor_intervals[2].value().start == 20);
    REQUIRE(xor_intervals[2].value().end == 24);
    REQUIRE(xor_intervals[3].value().start == 31);
    REQUIRE(xor_intervals[3].value().end == 35);
    
    // Verify NOT result: (11,19)
    auto not_result = dm.getData<DigitalIntervalSeries>("not_result");
    REQUIRE(not_result != nullptr);
    auto const & not_intervals = not_result->view();
    REQUIRE(not_result->size() == 1);
    REQUIRE(not_intervals[0].value().start == 11);
    REQUIRE(not_intervals[0].value().end == 19);
    
    // Verify AND_NOT result: (1,4), (20,24)
    auto and_not_result = dm.getData<DigitalIntervalSeries>("and_not_result");
    REQUIRE(and_not_result != nullptr);
    auto const & and_not_intervals = and_not_result->view();
    REQUIRE(and_not_result->size() == 2);
    REQUIRE(and_not_intervals[0].value().start == 1);
    REQUIRE(and_not_intervals[0].value().end == 4);
    REQUIRE(and_not_intervals[1].value().start == 20);
    REQUIRE(and_not_intervals[1].value().end == 24);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
