/**
 * @file SynthesizeData.test.cpp
 * @brief Unit tests for the SynthesizeData command
 */

#include "Commands/SynthesizeData.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

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
            "num_cycles": 1
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
        "parameters": {"num_samples": 50, "amplitude": 1.0, "num_cycles": 1}
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
            "num_cycles": 10,
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
    REQUIRE(val.value() == Catch::Approx(0.0f).margin(1e-5f));// NOLINT(bugprone-unchecked-optional-access)
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
        "parameters": {"num_samples": 100, "amplitude": 1.0, "num_cycles": 1}
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
        "parameters": {"num_samples": 50, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "my_timeframe",
        "time_frame_mode": "create_new"
    })");
    REQUIRE(cmd != nullptr);

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);

    auto ts = ctx.data_manager->getData<AnalogTimeSeries>("custom_time");
    REQUIRE(ts != nullptr);
    REQUIRE(ts->getNumSamples() == 50);
}

// =============================================================================
// TimeFrame modes
// =============================================================================

TEST_CASE("SynthesizeData create_new fails when TimeFrame already exists",
          "[commands][SynthesizeData][timeframe]") {
    auto ctx = makeSynthContext();

    // First create succeeds
    auto cmd1 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig1",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 100, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "shared_tf",
        "time_frame_mode": "create_new"
    })");
    auto r1 = cmd1->execute(ctx);
    REQUIRE(r1.success);

    // Second create with same time_key should fail
    auto cmd2 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig2",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 100, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "shared_tf",
        "time_frame_mode": "create_new"
    })");
    auto r2 = cmd2->execute(ctx);
    REQUIRE_FALSE(r2.success);
    REQUIRE(r2.error_message.find("already exists") != std::string::npos);
}

TEST_CASE("SynthesizeData use_existing succeeds with compatible TimeFrame",
          "[commands][SynthesizeData][timeframe]") {
    auto ctx = makeSynthContext();

    // Create a TimeFrame first via create_new
    auto cmd1 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig1",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 200, "amplitude": 1.0, "num_cycles": 2},
        "time_key": "shared_tf",
        "time_frame_mode": "create_new"
    })");
    REQUIRE(cmd1->execute(ctx).success);

    // use_existing with same size should succeed
    auto cmd2 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig2",
        "generator_name": "SquareWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 200, "amplitude": 1.0, "num_cycles": 2},
        "time_key": "shared_tf",
        "time_frame_mode": "use_existing"
    })");
    auto r2 = cmd2->execute(ctx);
    REQUIRE(r2.success);
}

TEST_CASE("SynthesizeData use_existing fails when TimeFrame is missing",
          "[commands][SynthesizeData][timeframe]") {
    auto ctx = makeSynthContext();

    auto cmd = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig1",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 100, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "nonexistent",
        "time_frame_mode": "use_existing"
    })");
    auto result = cmd->execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("does not exist") != std::string::npos);
}

TEST_CASE("SynthesizeData use_existing fails when TimeFrame is too small",
          "[commands][SynthesizeData][timeframe]") {
    auto ctx = makeSynthContext();

    // Create a small TimeFrame
    auto cmd1 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig1",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 50, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "small_tf",
        "time_frame_mode": "create_new"
    })");
    REQUIRE(cmd1->execute(ctx).success);

    // Try to use it with a larger signal
    auto cmd2 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig2",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 200, "amplitude": 1.0, "num_cycles": 2},
        "time_key": "small_tf",
        "time_frame_mode": "use_existing"
    })");
    auto r2 = cmd2->execute(ctx);
    REQUIRE_FALSE(r2.success);
    REQUIRE(r2.error_message.find("frames but data requires") != std::string::npos);
}

TEST_CASE("SynthesizeData overwrite replaces existing TimeFrame",
          "[commands][SynthesizeData][timeframe]") {
    auto ctx = makeSynthContext();

    // Create initial
    auto cmd1 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig1",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 50, "amplitude": 1.0, "num_cycles": 1},
        "time_key": "my_tf",
        "time_frame_mode": "create_new"
    })");
    REQUIRE(cmd1->execute(ctx).success);

    auto tf_before = ctx.data_manager->getTime(TimeKey("my_tf"));
    REQUIRE(tf_before->getTotalFrameCount() == 50);

    // Overwrite with larger signal
    auto cmd2 = createCommandFromJson("SynthesizeData", R"({
        "output_key": "sig2",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {"num_samples": 300, "amplitude": 1.0, "num_cycles": 3},
        "time_key": "my_tf",
        "time_frame_mode": "overwrite"
    })");
    auto r2 = cmd2->execute(ctx);
    REQUIRE(r2.success);

    auto tf_after = ctx.data_manager->getTime(TimeKey("my_tf"));
    REQUIRE(tf_after->getTotalFrameCount() == 300);
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
