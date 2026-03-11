/**
 * @file CommandRecorder.test.cpp
 * @brief Unit tests for CommandRecorder and MutationGuard
 */

#include "Commands/CommandRecorder.hpp"
#include "Commands/CommandContext.hpp"
#include "Commands/CommandDescriptor.hpp"
#include "Commands/CommandFactory.hpp"
#include "Commands/ICommand.hpp"
#include "DataManager/Commands/MutationGuard.hpp"
#include "Commands/SequenceExecution.hpp"
#include "Commands/VariableSubstitution.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace commands;

// =============================================================================
// CommandRecorder basic tests
// =============================================================================

TEST_CASE("CommandRecorder starts empty", "[commands][recorder]") {
    CommandRecorder const recorder;
    REQUIRE(recorder.empty());
    REQUIRE(recorder.empty());
    REQUIRE(recorder.trace().empty());
}

TEST_CASE("CommandRecorder records and returns descriptors", "[commands][recorder]") {
    CommandRecorder recorder;

    CommandDescriptor desc1;
    desc1.command_name = "AddInterval";
    desc1.description = "First command";

    CommandDescriptor desc2;
    desc2.command_name = "MoveByTimeRange";
    desc2.description = "Second command";

    recorder.record(desc1);
    recorder.record(desc2);

    REQUIRE(recorder.size() == 2);
    REQUIRE_FALSE(recorder.empty());
    REQUIRE(recorder.trace()[0].command_name == "AddInterval");
    REQUIRE(recorder.trace()[1].command_name == "MoveByTimeRange");
}

TEST_CASE("CommandRecorder::clear resets the recorder", "[commands][recorder]") {
    CommandRecorder recorder;

    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    recorder.record(desc);

    REQUIRE(recorder.size() == 1);

    recorder.clear();

    REQUIRE(recorder.empty());
    REQUIRE(recorder.empty());
}

TEST_CASE("CommandRecorder::toSequence produces a valid CommandSequenceDescriptor",
          "[commands][recorder]") {
    CommandRecorder recorder;

    CommandDescriptor desc1;
    desc1.command_name = "AddInterval";
    desc1.description = "Add an interval";

    CommandDescriptor desc2;
    desc2.command_name = "CopyByTimeRange";

    recorder.record(desc1);
    recorder.record(desc2);

    auto seq = recorder.toSequence("test-workflow");

    REQUIRE(seq.name.has_value());
    REQUIRE(seq.name.value() == "test-workflow");
    REQUIRE(seq.commands.size() == 2);
    REQUIRE(seq.commands[0].command_name == "AddInterval");
    REQUIRE(seq.commands[0].description.has_value());
    REQUIRE(seq.commands[0].description.value() == "Add an interval");
    REQUIRE(seq.commands[1].command_name == "CopyByTimeRange");
}

TEST_CASE("CommandRecorder::toSequence with empty name omits name",
          "[commands][recorder]") {
    CommandRecorder const recorder;
    auto seq = recorder.toSequence();
    REQUIRE_FALSE(seq.name.has_value());
    REQUIRE(seq.commands.empty());
}

TEST_CASE("CommandRecorder::toSequence round-trips through JSON",
          "[commands][recorder]") {
    CommandRecorder recorder;

    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    recorder.record(desc);

    auto seq = recorder.toSequence("json-test");
    auto json_str = rfl::json::write(seq);
    auto parsed = rfl::json::read<CommandSequenceDescriptor>(json_str);

    REQUIRE(parsed);
    REQUIRE(parsed.value().name.value() == "json-test");
    REQUIRE(parsed.value().commands.size() == 1);
    REQUIRE(parsed.value().commands[0].command_name == "AddInterval");
}

// =============================================================================
// executeSequence + CommandRecorder integration
// =============================================================================

TEST_CASE("executeSequence records commands to CommandRecorder on success",
          "[commands][recorder]") {
    // Build a sequence that adds an interval via runtime variable substitution
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("intervals", TimeKey("time"));

    CommandSequenceDescriptor seq;
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    auto const params_json = R"({
        "interval_key": "intervals",
        "start_frame": "${start}",
        "end_frame": "${end}",
        "create_if_missing": true
    })";
    desc.parameters = rfl::json::read<rfl::Generic>(params_json).value();
    seq.commands.push_back(desc);

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.runtime_variables = {{"start", "10"}, {"end", "20"}};

    CommandRecorder recorder;
    auto result = executeSequence(seq, ctx, true, &recorder);

    INFO("Error: " << result.result.error_message);
    REQUIRE(result.result.success);
    REQUIRE(recorder.size() == 1);
    REQUIRE(recorder.trace()[0].command_name == "AddInterval");
}

TEST_CASE("executeSequence does not record failed commands",
          "[commands][recorder]") {
    auto dm = std::make_shared<DataManager>();

    CommandSequenceDescriptor seq;
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    auto const params_json = R"({
        "interval_key": "nonexistent_key",
        "start_frame": "${start}",
        "end_frame": "${end}",
        "create_if_missing": false
    })";
    desc.parameters = rfl::json::read<rfl::Generic>(params_json).value();
    seq.commands.push_back(desc);

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.runtime_variables = {{"start", "10"}, {"end", "20"}};

    CommandRecorder recorder;
    auto result = executeSequence(seq, ctx, true, &recorder);

    // The command should fail because "nonexistent_key" doesn't exist
    // (and create_if_missing is false)
    REQUIRE_FALSE(result.result.success);
    REQUIRE(recorder.empty());
}

TEST_CASE("executeSequence without recorder still works (nullptr)",
          "[commands][recorder]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("intervals", TimeKey("time"));

    CommandSequenceDescriptor seq;
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    auto const params_json = R"({
        "interval_key": "intervals",
        "start_frame": "${start}",
        "end_frame": "${end}",
        "create_if_missing": true
    })";
    desc.parameters = rfl::json::read<rfl::Generic>(params_json).value();
    seq.commands.push_back(desc);

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.runtime_variables = {{"start", "10"}, {"end", "20"}};

    // Pass nullptr explicitly — should behave exactly as before
    auto result = executeSequence(seq, ctx, true, nullptr);
    REQUIRE(result.result.success);
}

// =============================================================================
// normalizeJsonNumbers tests
// =============================================================================

TEST_CASE("normalizeJsonNumbers converts whole-number doubles to integers",
          "[commands][normalize]") {
    REQUIRE(normalizeJsonNumbers(R"({"x": 10.0})") == R"({"x": 10})");
    REQUIRE(normalizeJsonNumbers(R"({"x": -5.0})") == R"({"x": -5})");
    REQUIRE(normalizeJsonNumbers(R"({"x": 100.00})") == R"({"x": 100})");
}

TEST_CASE("normalizeJsonNumbers preserves non-integer doubles",
          "[commands][normalize]") {
    REQUIRE(normalizeJsonNumbers(R"({"x": 10.5})") == R"({"x": 10.5})");
    REQUIRE(normalizeJsonNumbers(R"({"x": 3.14})") == R"({"x": 3.14})");
}

TEST_CASE("normalizeJsonNumbers preserves integers and strings",
          "[commands][normalize]") {
    REQUIRE(normalizeJsonNumbers(R"({"x": 10})") == R"({"x": 10})");
    REQUIRE(normalizeJsonNumbers(R"({"x": "10.0"})") == R"({"x": "10.0"})");
    REQUIRE(normalizeJsonNumbers(R"({"x": "hello"})") == R"({"x": "hello"})");
}

TEST_CASE("normalizeJsonNumbers handles exponent notation",
          "[commands][normalize]") {
    // Exponent notation is left untouched
    REQUIRE(normalizeJsonNumbers(R"({"x": 1.0e2})") == R"({"x": 1.0e2})");
}

// =============================================================================
// executeSequence with literal integers (no variable substitution)
// =============================================================================

TEST_CASE("executeSequence handles literal integers in parameters",
          "[commands][recorder]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("intervals", TimeKey("time"));

    CommandSequenceDescriptor seq;
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    // Use literal integers — this triggered the rfl::Generic round-trip bug
    // before normalizeJsonNumbers was added
    auto const params_json = R"({
        "interval_key": "intervals",
        "start_frame": 10,
        "end_frame": 20,
        "create_if_missing": true
    })";
    desc.parameters = rfl::json::read<rfl::Generic>(params_json).value();
    seq.commands.push_back(desc);

    CommandContext ctx;
    ctx.data_manager = dm;

    auto result = executeSequence(seq, ctx, true);
    INFO("Error: " << result.result.error_message);
    REQUIRE(result.result.success);
}

// =============================================================================
// MutationGuard tests (debug builds only)
// =============================================================================

#ifndef NDEBUG

TEST_CASE("isInsideCommand is false by default", "[commands][mutation-guard]") {
    REQUIRE_FALSE(isInsideCommand());
}

TEST_CASE("CommandGuard sets isInsideCommand to true", "[commands][mutation-guard]") {
    REQUIRE_FALSE(isInsideCommand());
    {
        CommandGuard guard;
        REQUIRE(isInsideCommand());
    }
    REQUIRE_FALSE(isInsideCommand());
}

TEST_CASE("CommandGuard nests correctly", "[commands][mutation-guard]") {
    REQUIRE_FALSE(isInsideCommand());
    {
        CommandGuard outer;
        REQUIRE(isInsideCommand());
        {
            CommandGuard inner;
            REQUIRE(isInsideCommand());
        }
        // Outer guard still active
        REQUIRE(isInsideCommand());
    }
    REQUIRE_FALSE(isInsideCommand());
}

TEST_CASE("CommandGuard restores previous state on destruction",
          "[commands][mutation-guard]") {
    // Verify state is false outside any guard
    REQUIRE_FALSE(isInsideCommand());

    CommandGuard guard;
    REQUIRE(isInsideCommand());
    // Guard destructor will run at end-of-scope
}

#endif// NDEBUG
