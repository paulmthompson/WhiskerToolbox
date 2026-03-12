/**
 * @file CommandFactory.test.cpp
 * @brief Unit tests for the command factory and core types
 */

#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandResult.hpp"
#include "Commands/Core/ICommand.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

using namespace commands;

// -- CommandResult tests --

TEST_CASE("CommandResult::ok creates a successful result", "[commands][core-types]") {
    auto result = CommandResult::ok({"key1", "key2"});
    REQUIRE(result.success);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.affected_keys.size() == 2);
    REQUIRE(result.affected_keys[0] == "key1");
    REQUIRE(result.affected_keys[1] == "key2");
}

TEST_CASE("CommandResult::ok with no keys", "[commands][core-types]") {
    auto result = CommandResult::ok();
    REQUIRE(result.success);
    REQUIRE(result.affected_keys.empty());
}

TEST_CASE("CommandResult::error creates a failed result", "[commands][core-types]") {
    auto result = CommandResult::error("something went wrong");
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message == "something went wrong");
    REQUIRE(result.affected_keys.empty());
}

// -- CommandDescriptor serialization tests --

TEST_CASE("CommandDescriptor round-trips through JSON", "[commands][core-types]") {
    CommandDescriptor desc;
    desc.command_name = "MoveByTimeRange";
    desc.description = "Move whisker data";

    auto const json = rfl::json::write(desc);
    auto const result = rfl::json::read<CommandDescriptor>(json);
    REQUIRE(result);

    auto const & parsed = result.value();
    REQUIRE(parsed.command_name == "MoveByTimeRange");
    REQUIRE(parsed.description.has_value());
    CHECK(parsed.description.value() == "Move whisker data");// NOLINT(bugprone-unchecked-optional-access)
    REQUIRE_FALSE(parsed.parameters.has_value());
}

TEST_CASE("CommandSequenceDescriptor round-trips through JSON", "[commands][core-types]") {
    CommandDescriptor cmd1;
    cmd1.command_name = "AddInterval";
    cmd1.description = "Track region";

    CommandSequenceDescriptor seq;
    seq.name = "Test Pipeline";
    seq.version = "1.0";
    seq.variables = std::map<std::string, std::string>{{"source", "pred_w0"}};
    seq.commands = {cmd1};

    auto const json = rfl::json::write(seq);
    auto const result = rfl::json::read<CommandSequenceDescriptor>(json);
    REQUIRE(result);

    auto const & parsed = result.value();
    REQUIRE(parsed.name.has_value());
    CHECK(parsed.name.value() == "Test Pipeline");// NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(parsed.version.has_value());
    CHECK(parsed.version.value() == "1.0");// NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(parsed.variables.has_value());
    CHECK(parsed.variables->at("source") == "pred_w0");// NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(parsed.commands.size() == 1);
    REQUIRE(parsed.commands[0].command_name == "AddInterval");
}

// -- Factory tests --

TEST_CASE("createCommand returns nullptr for unknown command name", "[commands][factory]") {
    auto const params_json = R"({"key": "value"})";
    auto const generic = rfl::json::read<rfl::Generic>(params_json).value();

    auto cmd = createCommand("NonExistentCommand", generic);
    REQUIRE(cmd == nullptr);
}

TEST_CASE("createCommand returns nullptr for empty command name", "[commands][factory]") {
    auto const params_json = R"({})";
    auto const generic = rfl::json::read<rfl::Generic>(params_json).value();

    auto cmd = createCommand("", generic);
    REQUIRE(cmd == nullptr);
}

// -- ICommand default behavior tests --

namespace {

/// Minimal concrete command for testing the ICommand interface defaults
class StubCommand : public ICommand {
public:
    CommandResult execute(CommandContext const & /*ctx*/) override {
        return CommandResult::ok({"stub_key"});
    }

    [[nodiscard]] std::string commandName() const override { return "StubCommand"; }

    [[nodiscard]] std::string toJson() const override { return "{}"; }
};

}// namespace

TEST_CASE("ICommand defaults: not undoable", "[commands][icommand]") {
    StubCommand const cmd;
    REQUIRE_FALSE(cmd.isUndoable());
}

TEST_CASE("ICommand defaults: undo returns error", "[commands][icommand]") {
    StubCommand cmd;
    CommandContext const ctx;
    auto result = cmd.undo(ctx);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("ICommand defaults: mergeId is -1", "[commands][icommand]") {
    StubCommand const cmd;
    REQUIRE(cmd.mergeId() == -1);
}

TEST_CASE("ICommand defaults: mergeWith returns false", "[commands][icommand]") {
    StubCommand cmd;
    StubCommand const other;
    REQUIRE_FALSE(cmd.mergeWith(other));
}

TEST_CASE("StubCommand executes successfully", "[commands][icommand]") {
    StubCommand cmd;
    CommandContext const ctx;
    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(result.affected_keys.size() == 1);
    REQUIRE(result.affected_keys[0] == "stub_key");
}

TEST_CASE("StubCommand reports its name", "[commands][icommand]") {
    StubCommand const cmd;
    REQUIRE(cmd.commandName() == "StubCommand");
}
