/**
 * @file SynthesizeData.test.cpp
 * @brief Unit tests for the SynthesizeData command
 */

#include "Commands/SynthesizeData.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace commands;

// =============================================================================
// Helper
// =============================================================================
namespace {

CommandContext makeSynthContext() {
    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    return ctx;
}

}// namespace

// =============================================================================
// Factory creation
// =============================================================================

TEST_CASE("createCommandFromJson creates SynthesizeData from valid params",
          "[commands][SynthesizeData][factory]") {
    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "test_sine",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {
            "num_samples": 100,
            "amplitude": 1.0,
            "frequency": 0.01
        }
    })");
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "SynthesizeData");
    REQUIRE_FALSE(cmd->isUndoable());
}

TEST_CASE("createCommand creates SynthesizeData via rfl::Generic",
          "[commands][SynthesizeData][factory]") {
    auto const json = R"({
        "output_key": "out",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 50, "amplitude": 1.0, "frequency": 0.02}
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SynthesizeData", generic);
    REQUIRE(cmd != nullptr);
}

// =============================================================================
// Execution: SineWave
// =============================================================================

TEST_CASE("SynthesizeData command produces deterministic sine wave",
          "[commands][SynthesizeData]") {
    auto ctx = makeSynthContext();

    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "test_sine",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {
            "num_samples": 1000,
            "amplitude": 2.0,
            "frequency": 0.01,
            "phase": 0.0,
            "dc_offset": 0.0
        }
    })");
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);
    REQUIRE(result.affected_keys.size() == 1);
    REQUIRE(result.affected_keys[0] == "test_sine");

    auto ts = ctx.data_manager->getData<AnalogTimeSeries>("test_sine");
    REQUIRE(ts != nullptr);
    REQUIRE(ts->getNumSamples() == 1000);

    // sin(0) == 0
    auto val = ts->getAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    REQUIRE(val.value() == Catch::Approx(0.0f).margin(1e-5f)); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("SynthesizeData determinism: same params produce same output",
          "[commands][SynthesizeData]") {
    auto const json = R"({
        "output_key": "det",
        "generator_name": "GaussianNoise",
        "output_type": "AnalogTimeSeries",
        "parameters": {
            "num_samples": 500,
            "mean": 0.0,
            "stddev": 1.0,
            "seed": 12345
        }
    })";

    auto ctx1 = makeSynthContext();
    auto cmd1 = createCommandFromJson("SynthesizeData", json);
    REQUIRE(cmd1->execute(ctx1).success);

    auto ctx2 = makeSynthContext();
    auto cmd2 = createCommandFromJson("SynthesizeData", json);
    REQUIRE(cmd2->execute(ctx2).success);

    auto ts1 = ctx1.data_manager->getData<AnalogTimeSeries>("det");
    auto ts2 = ctx2.data_manager->getData<AnalogTimeSeries>("det");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    REQUIRE(v1.size() == v2.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        REQUIRE(v1[i] == v2[i]);
    }
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("SynthesizeData command returns error for unknown generator",
          "[commands][SynthesizeData]") {
    auto ctx = makeSynthContext();

    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "will_fail",
        "generator_name": "NonExistentGenerator",
        "output_type": "AnalogTimeSeries",
        "parameters": {}
    })");
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("NonExistentGenerator") != std::string::npos);
}

// =============================================================================
// Serialization round-trip
// =============================================================================

TEST_CASE("SynthesizeData toJson produces valid JSON",
          "[commands][SynthesizeData]") {
    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "out",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 100, "amplitude": 1.0, "frequency": 0.01}
    })");
    REQUIRE(cmd != nullptr);

    auto const json_str = cmd->toJson();
    REQUIRE_FALSE(json_str.empty());

    // Should be valid JSON that can be read back
    auto roundtrip = createCommandFromJson("SynthesizeData", json_str);
    REQUIRE(roundtrip != nullptr);
}

// =============================================================================
// Custom time_key
// =============================================================================

TEST_CASE("SynthesizeData respects custom time_key",
          "[commands][SynthesizeData]") {
    auto ctx = makeSynthContext();

    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "custom_time",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 50, "amplitude": 1.0, "frequency": 0.05},
        "time_key": "my_timeframe"
    })");
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);

    auto ts = ctx.data_manager->getData<AnalogTimeSeries>("custom_time");
    REQUIRE(ts != nullptr);
    REQUIRE(ts->getNumSamples() == 50);
}

// =============================================================================
// Introspection
// =============================================================================

TEST_CASE("SynthesizeData is listed in getAvailableCommands",
          "[commands][SynthesizeData]") {
    auto const all = getAvailableCommands();
    bool found = false;
    for (auto const & info: all) {
        if (info.name == "SynthesizeData") {
            found = true;
            REQUIRE(info.category == "data_generation");
            REQUIRE_FALSE(info.supports_undo);
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("isKnownCommandName recognizes SynthesizeData",
          "[commands][SynthesizeData]") {
    REQUIRE(isKnownCommandName("SynthesizeData"));
}
