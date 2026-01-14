#include "DigitalIntervalBoolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "DataManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include "fixtures/scenarios/digital/digital_interval_boolean_scenarios.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration: Uses singleton from RegisteredTransforms.cpp (compile-time)
// ============================================================================
// DigitalIntervalBoolean is registered at compile-time via
// RegisterBinaryContainerTransform RAII helper in RegisteredTransforms.cpp.
// The ElementRegistry::instance() singleton already has this transform
// available when tests run.

// ============================================================================
// Tests: Algorithm Correctness using scenarios
// ============================================================================

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - AND Operation",
          "[transforms][v2][binary_container][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    DigitalIntervalBooleanParams params;
    params.operation = "and";
    ComputeContext ctx;

    SECTION("Basic AND - overlapping intervals") {
        auto [input, other] = and_overlapping();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("AND - complete overlap") {
        auto [input, other] = and_complete_overlap();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 10);
    }

    SECTION("AND - one series subset of other") {
        auto [input, other] = and_subset();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 5);
        REQUIRE(result_intervals[0].value().end == 15);
    }
}

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - OR Operation",
          "[transforms][v2][binary_container][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    DigitalIntervalBooleanParams params;
    params.operation = "or";
    ComputeContext ctx;

    SECTION("Basic OR - separate intervals") {
        auto [input, other] = or_separate();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 15);
    }

    SECTION("OR - multiple intervals with gaps") {
        auto [input, other] = or_multiple_with_gaps();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - XOR Operation",
          "[transforms][v2][binary_container][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    DigitalIntervalBooleanParams params;
    params.operation = "xor";
    ComputeContext ctx;

    SECTION("Basic XOR - no overlap") {
        auto [input, other] = xor_no_overlap();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("XOR - complex pattern") {
        auto [input, other] = xor_complex();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - NOT Operation",
          "[transforms][v2][binary_container][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    DigitalIntervalBooleanParams params;
    params.operation = "not";
    ComputeContext ctx;

    SECTION("NOT - single interval") {
        auto input = not_single_interval();
        auto other = DigitalIntervalSeriesBuilder().build(); // Ignored for NOT

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("NOT - intervals with gaps") {
        auto input = not_with_gaps();
        auto other = DigitalIntervalSeriesBuilder().build(); // Ignored for NOT

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 6);
        REQUIRE(result_intervals[0].value().end == 9);
    }

    SECTION("NOT - multiple gaps") {
        auto input = not_multiple_gaps();
        auto other = DigitalIntervalSeriesBuilder().build(); // Ignored for NOT

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 4);
        REQUIRE(result_intervals[0].value().end == 4);
        REQUIRE(result_intervals[1].value().start == 8);
        REQUIRE(result_intervals[1].value().end == 8);
    }
}

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - AND_NOT Operation",
          "[transforms][v2][binary_container][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    DigitalIntervalBooleanParams params;
    params.operation = "and_not";
    ComputeContext ctx;

    SECTION("AND_NOT - subtract overlapping portion") {
        auto [input, other] = and_not_subtract_overlap();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 4);
    }

    SECTION("AND_NOT - no overlap keeps input") {
        auto [input, other] = and_not_no_overlap();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 5);
    }

    SECTION("AND_NOT - complete overlap removes everything") {
        auto [input, other] = and_not_complete_overlap();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("AND_NOT - punch holes in input") {
        auto [input, other] = and_not_punch_holes();

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

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

TEST_CASE("V2 Binary Container Transform: Digital Interval Boolean - Edge Cases",
          "[transforms][v2][binary_container][digital_interval_boolean][edge_cases]") {
    using namespace digital_interval_boolean_scenarios;
    
    auto& registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Empty input series") {
        auto [input, other] = empty_input();

        DigitalIntervalBooleanParams params;
        params.operation = "or";

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        // OR with empty input and non-empty other should give the other
        REQUIRE(!result->view().empty());
    }

    SECTION("Both series empty") {
        auto [input, other] = both_empty();

        DigitalIntervalBooleanParams params;
        params.operation = "and";

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("NOT with empty series") {
        auto input = not_empty();
        auto other = DigitalIntervalSeriesBuilder().build();

        DigitalIntervalBooleanParams params;
        params.operation = "not";

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("Progress callback is invoked") {
        auto [input, other] = large_intervals();

        DigitalIntervalBooleanParams params;
        params.operation = "and";

        int progress_val = -1;
        ctx.progress = [&](int p) { progress_val = p; };

        auto result = registry.executeBinaryContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            DigitalIntervalBooleanParams>(
            "DigitalIntervalBoolean",
            *input,
            *other,
            params,
            ctx);

        REQUIRE(result != nullptr);
        REQUIRE(progress_val == 100);
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: DigitalIntervalBooleanParams - JSON Loading",
          "[transforms][v2][params][json]") {

    SECTION("Load valid JSON with operation") {
        std::string json = R"({
            "operation": "xor"
        })";

        auto result = loadParametersFromJson<DigitalIntervalBooleanParams>(json);

        REQUIRE(result);
        auto params = result.value();
        REQUIRE(params.getOperation() == "xor");
        REQUIRE(params.isValidOperation());
    }

    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";

        auto result = loadParametersFromJson<DigitalIntervalBooleanParams>(json);

        REQUIRE(result);
        auto params = result.value();
        REQUIRE(params.getOperation() == "and");  // Default
        REQUIRE(params.isValidOperation());
    }

    SECTION("All valid operations") {
        std::vector<std::string> valid_ops = {"and", "or", "xor", "not", "and_not"};

        for (auto const& op : valid_ops) {
            std::string json = R"({"operation": ")" + op + R"("})";

            auto result = loadParametersFromJson<DigitalIntervalBooleanParams>(json);
            REQUIRE(result);
            auto params = result.value();
            REQUIRE(params.getOperation() == op);
            REQUIRE(params.isValidOperation());
        }
    }

    SECTION("Invalid operation name - validation fails") {
        DigitalIntervalBooleanParams params;
        params.operation = "invalid_op";

        REQUIRE_FALSE(params.isValidOperation());
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: Digital Interval Boolean via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][digital_interval_boolean]") {
    using namespace digital_interval_boolean_scenarios;
    
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate DataManager with test data
    auto [and_input, and_other] = and_overlapping();
    dm.setData("and_overlapping_input", and_input, TimeKey("default"));
    dm.setData("and_overlapping_other", and_other, TimeKey("default"));
    
    auto [or_input, or_other] = or_overlapping_merge();
    dm.setData("or_overlapping_merge_input", or_input, TimeKey("default"));
    dm.setData("or_overlapping_merge_other", or_other, TimeKey("default"));

    SECTION("AND operation through JSON pipeline") {
        // Create temporary JSON config file
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "v2_boolean_test";
        std::filesystem::create_directories(test_dir);
        std::filesystem::path json_filepath = test_dir / "and_pipeline.json";

        // Uses additional_input_keys for multi-input (binary) transforms
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Boolean AND Test Pipeline",
                    "version": "1.0"
                },
                "steps": [
                    {
                        "step_id": "boolean_and",
                        "transform_name": "DigitalIntervalBoolean",
                        "input_key": "and_overlapping_input",
                        "additional_input_keys": ["and_overlapping_other"],
                        "output_key": "and_result",
                        "parameters": {
                            "operation": "and"
                        }
                    }
                ]
            }
        }
        ])";

        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }

        // Execute the V2 pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        // Verify result was stored
        auto result = dm.getData<DigitalIntervalSeries>("and_result");
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(result_intervals[0].value().start == 3);
        REQUIRE(result_intervals[0].value().end == 5);
        REQUIRE(result_intervals[1].value().start == 12);
        REQUIRE(result_intervals[1].value().end == 15);

        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }

    SECTION("OR operation through JSON pipeline") {
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "v2_boolean_or_test";
        std::filesystem::create_directories(test_dir);
        std::filesystem::path json_filepath = test_dir / "or_pipeline.json";

        // Uses additional_input_keys for multi-input (binary) transforms
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Boolean OR Test Pipeline",
                    "version": "1.0"
                },
                "steps": [
                    {
                        "step_id": "boolean_or",
                        "transform_name": "DigitalIntervalBoolean",
                        "input_key": "or_overlapping_merge_input",
                        "additional_input_keys": ["or_overlapping_merge_other"],
                        "output_key": "or_result",
                        "parameters": {
                            "operation": "or"
                        }
                    }
                ]
            }
        }
        ])";

        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }

        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());

        auto result = dm.getData<DigitalIntervalSeries>("or_result");
        REQUIRE(result != nullptr);

        auto const & result_intervals = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(result_intervals[0].value().start == 1);
        REQUIRE(result_intervals[0].value().end == 15);

        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
