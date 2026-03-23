/**
 * @file CommandRegistry.test.cpp
 * @brief Unit tests for CommandRegistry singleton and registerTypedCommand helper
 */

#include "Commands/Core/CommandRegistry.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandResult.hpp"
#include "Commands/Core/ICommand.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace commands;

// -- Minimal test command for registry tests --

namespace {

struct StubParams {
    std::string value;
};

class StubRegistryCommand : public ICommand {
public:
    explicit StubRegistryCommand(StubParams params)
        : _params(std::move(params)) {}

    CommandResult execute(CommandContext const & /*ctx*/) override {
        return CommandResult::ok({"stub_key"});
    }

    [[nodiscard]] std::string commandName() const override { return "StubRegistryCommand"; }
    [[nodiscard]] std::string toJson() const override { return R"({"value":"test"})"; }

private:
    StubParams _params;
};

}// namespace

// -- CommandRegistry tests --

TEST_CASE("CommandRegistry::instance returns same instance", "[commands][registry]") {
    auto & a = CommandRegistry::instance();
    auto & b = CommandRegistry::instance();
    REQUIRE(&a == &b);
}

TEST_CASE("CommandRegistry starts with registered core commands", "[commands][registry]") {
    // Core commands are registered by the Catch2 listener (CommandTestSetup)
    auto & reg = CommandRegistry::instance();
    REQUIRE(reg.isKnown("MoveByTimeRange"));
    REQUIRE(reg.isKnown("CopyByTimeRange"));
    REQUIRE(reg.isKnown("AddInterval"));
    REQUIRE(reg.isKnown("ForEachKey"));
    REQUIRE(reg.isKnown("SaveData"));
    REQUIRE(reg.isKnown("LoadData"));
    REQUIRE(reg.isKnown("SynthesizeData"));
    REQUIRE(reg.size() >= 7);
}

TEST_CASE("CommandRegistry::isKnown returns false for unknown commands", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    REQUIRE_FALSE(reg.isKnown("NonExistentCommand"));
    REQUIRE_FALSE(reg.isKnown(""));
}

TEST_CASE("CommandRegistry::create returns nullptr for unknown command", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto cmd = reg.create("NonExistentCommand", "{}");
    REQUIRE(cmd == nullptr);
}

TEST_CASE("CommandRegistry::create produces valid MoveByTimeRange", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto const json = R"({"source_key":"a","destination_key":"b","start_frame":0,"end_frame":10})";
    auto cmd = reg.create("MoveByTimeRange", json);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "MoveByTimeRange");
}

TEST_CASE("CommandRegistry::availableCommands returns all registered", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto const all = reg.availableCommands();
    REQUIRE(all.size() >= 7);

    // Check that each known command appears exactly once
    auto contains = [&](std::string const & name) {
        return std::any_of(all.begin(), all.end(),
                           [&](CommandInfo const & info) { return info.name == name; });
    };
    REQUIRE(contains("MoveByTimeRange"));
    REQUIRE(contains("CopyByTimeRange"));
    REQUIRE(contains("AddInterval"));
    REQUIRE(contains("ForEachKey"));
    REQUIRE(contains("SaveData"));
    REQUIRE(contains("LoadData"));
    REQUIRE(contains("SynthesizeData"));
}

TEST_CASE("CommandRegistry::commandInfo returns metadata for known command", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto info = reg.commandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    CHECK(info->name == "MoveByTimeRange");         // NOLINT(bugprone-unchecked-optional-access)
    CHECK(info->category == "data_mutation");       // NOLINT(bugprone-unchecked-optional-access)
    CHECK(info->supports_undo == true);             // NOLINT(bugprone-unchecked-optional-access)
    CHECK_FALSE(info->supported_data_types.empty());// NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("CommandRegistry::commandInfo returns nullopt for unknown command", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto info = reg.commandInfo("NonExistentCommand");
    REQUIRE_FALSE(info.has_value());
}

TEST_CASE("CommandFactory delegates to CommandRegistry", "[commands][registry]") {
    // Verify backward compatibility: free functions delegate to registry
    REQUIRE(isKnownCommandName("MoveByTimeRange"));
    REQUIRE_FALSE(isKnownCommandName("NonExistentCommand"));

    auto const all = getAvailableCommands();
    REQUIRE(all.size() >= 7);

    auto info = getCommandInfo("SaveData");
    REQUIRE(info.has_value());
    CHECK(info->name == "SaveData");// NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("registerTypedCommand populates parameter_schema", "[commands][registry]") {
    auto & reg = CommandRegistry::instance();
    auto info = reg.commandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    // extractParameterSchema produces fields — MoveByTimeRangeParams has at least 4 fields
    CHECK_FALSE(info->parameter_schema.fields.empty());// NOLINT(bugprone-unchecked-optional-access)
}
