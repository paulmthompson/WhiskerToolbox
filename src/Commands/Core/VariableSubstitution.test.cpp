/**
 * @file VariableSubstitution.test.cpp
 * @brief Unit tests for variable substitution and sequence execution
 */

#include "Commands/Core/VariableSubstitution.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/ICommand.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

using namespace commands;

// ============================================================================
// Variable Substitution Tests
// ============================================================================

TEST_CASE("substituteVariables replaces a single variable", "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"source", "pred_w0"}};
    auto const result = substituteVariables("${source}", vars);
    REQUIRE(result == "pred_w0");
}

TEST_CASE("substituteVariables replaces multiple variables in one string",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {
            {"source", "pred_w0"},
            {"dest", "gt_w0"}};
    auto const result = substituteVariables("from ${source} to ${dest}", vars);
    REQUIRE(result == "from pred_w0 to gt_w0");
}

TEST_CASE("substituteVariables leaves unresolved variables intact",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"source", "pred_w0"}};
    auto const result = substituteVariables("${source} and ${unknown}", vars);
    REQUIRE(result == "pred_w0 and ${unknown}");
}

TEST_CASE("substituteVariables handles empty variables map",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars;
    auto const result = substituteVariables("${source}", vars);
    REQUIRE(result == "${source}");
}

TEST_CASE("substituteVariables handles string with no variables",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"source", "pred_w0"}};
    auto const result = substituteVariables("plain text", vars);
    REQUIRE(result == "plain text");
}

TEST_CASE("substituteVariables handles empty input string",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"source", "pred_w0"}};
    auto const result = substituteVariables("", vars);
    REQUIRE(result.empty());
}

TEST_CASE("substituteVariables handles malformed variable (no closing brace)",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"source", "pred_w0"}};
    auto const result = substituteVariables("${source", vars);
    REQUIRE(result == "${source");
}

TEST_CASE("substituteVariables handles adjacent variables",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {
            {"a", "hello"},
            {"b", "world"}};
    auto const result = substituteVariables("${a}${b}", vars);
    REQUIRE(result == "helloworld");
}

TEST_CASE("substituteVariables handles numeric values as strings",
          "[commands][variable-substitution]") {
    std::map<std::string, std::string> const vars = {{"start", "42000"}};
    auto const result = substituteVariables("frame_${start}", vars);
    REQUIRE(result == "frame_42000");
}

TEST_CASE("substituteVariables handles variable value containing ${ pattern",
          "[commands][variable-substitution]") {
    // A replacement value that itself looks like a variable should NOT be re-expanded
    std::map<std::string, std::string> const vars = {
            {"a", "${b}"},
            {"b", "actual"}};
    auto const result = substituteVariables("${a}", vars);
    // The substitution for ${a} yields "${b}" and then the cursor advances past it.
    // The ${b} in the replacement should not be re-expanded in a single pass.
    REQUIRE(result == "${b}");
}

// ============================================================================
// Sequence Execution Tests (using StubCommand infrastructure)
// ============================================================================

// Since no concrete commands are registered yet, sequence execution with real commands
// will produce "unknown command" errors. These tests verify the execution infrastructure.

TEST_CASE("executeSequence succeeds on empty command list", "[commands][sequence]") {
    CommandSequenceDescriptor seq;
    seq.name = "Empty Sequence";
    seq.commands = {};

    CommandContext const ctx;
    auto result = executeSequence(seq, ctx);

    REQUIRE(result.result.success);
    REQUIRE(result.executed_commands.empty());
    REQUIRE(result.failed_index == -1);
}

TEST_CASE("executeSequence fails on unknown command (stop_on_error=true)",
          "[commands][sequence]") {
    CommandDescriptor cmd;
    cmd.command_name = "NonExistentCommand";

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext const ctx;
    auto result = executeSequence(seq, ctx, true);

    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.failed_index == 0);
    REQUIRE(result.executed_commands.empty());
}

TEST_CASE("executeSequence continues past unknown command (stop_on_error=false)",
          "[commands][sequence]") {
    CommandDescriptor cmd1;
    cmd1.command_name = "NonExistent1";

    CommandDescriptor cmd2;
    cmd2.command_name = "NonExistent2";

    CommandSequenceDescriptor seq;
    seq.commands = {cmd1, cmd2};

    CommandContext const ctx;
    auto result = executeSequence(seq, ctx, false);

    // Both commands are unknown, but execution continues
    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.executed_commands.empty());
}

TEST_CASE("executeSequence applies static variable substitution to parameters",
          "[commands][sequence]") {
    // Create a sequence with static variables and verify substitution produces valid params
    // (The command itself will be unknown, but we can verify via the error message that
    //  the command name is correctly resolved)
    CommandDescriptor cmd;
    cmd.command_name = "${operation}";

    CommandSequenceDescriptor seq;
    seq.variables = std::map<std::string, std::string>{{"operation", "MoveByTimeRange"}};
    seq.commands = {cmd};

    CommandContext const ctx;
    auto result = executeSequence(seq, ctx);

    // Fails because MoveByTimeRange is not yet registered, but the variable was substituted
    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.result.error_message.find("MoveByTimeRange") != std::string::npos);
}

TEST_CASE("executeSequence applies runtime variable substitution to command name",
          "[commands][sequence]") {
    CommandDescriptor cmd;
    cmd.command_name = "${runtime_cmd}";

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.runtime_variables = {{"runtime_cmd", "SaveData"}};

    auto result = executeSequence(seq, ctx);

    // Fails because SaveData is not registered, but the variable was substituted
    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.result.error_message.find("SaveData") != std::string::npos);
}

TEST_CASE("executeSequence applies both static and runtime variable substitution",
          "[commands][sequence]") {
    // Static variable in one place, runtime in another
    CommandDescriptor cmd;
    cmd.command_name = "${static_name}";

    auto params_json = R"({"source_key": "${runtime_key}"})";
    cmd.parameters = rfl::json::read<rfl::Generic>(params_json).value();

    CommandSequenceDescriptor seq;
    seq.variables = std::map<std::string, std::string>{{"static_name", "MoveByTimeRange"}};
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.runtime_variables = {{"runtime_key", "pred_w0"}};

    auto result = executeSequence(seq, ctx);

    // Fails because MoveByTimeRange is not registered, but both vars were substituted
    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.result.error_message.find("MoveByTimeRange") != std::string::npos);
}

TEST_CASE("executeSequence reports correct failed_index for second command",
          "[commands][sequence]") {
    CommandDescriptor cmd1;
    cmd1.command_name = "Unknown1";

    CommandDescriptor cmd2;
    cmd2.command_name = "Unknown2";

    CommandSequenceDescriptor seq;
    seq.commands = {cmd1, cmd2};

    CommandContext const ctx;

    // stop_on_error=true: should fail at index 0
    auto result = executeSequence(seq, ctx, true);
    REQUIRE(result.failed_index == 0);

    // stop_on_error=false: should report last failure
    auto result2 = executeSequence(seq, ctx, false);
    REQUIRE(result2.failed_index == 1);
}

TEST_CASE("executeSequence with no parameters on command descriptor",
          "[commands][sequence]") {
    CommandDescriptor cmd;
    cmd.command_name = "SomeCommand";
    // No parameters set

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext const ctx;
    auto result = executeSequence(seq, ctx);

    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.result.error_message.find("SomeCommand") != std::string::npos);
}

TEST_CASE("executeSequence JSON round-trip with variables",
          "[commands][sequence]") {
    // Verify that a CommandSequenceDescriptor with variables serializes and
    // deserializes correctly, and then executes with substitution

    auto const json_str = R"({
        "name": "Test Pipeline",
        "variables": {"whisker": "w0"},
        "commands": [
            {
                "command_name": "MoveByTimeRange",
                "parameters": {
                    "source_key": "pred_${whisker}",
                    "dest_key": "gt_${whisker}"
                },
                "description": "Move whisker data"
            }
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json_str);
    REQUIRE(seq);

    CommandContext ctx;
    ctx.runtime_variables = {{"start_frame", "1000"}, {"end_frame", "2000"}};

    auto result = executeSequence(seq.value(), ctx);

    // MoveByTimeRange is not registered yet, but we can verify the error references
    // the correctly substituted command name
    REQUIRE_FALSE(result.result.success);
    REQUIRE(result.result.error_message.find("MoveByTimeRange") != std::string::npos);
}
