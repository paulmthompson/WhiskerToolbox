/**
 * @file Validation.test.cpp
 * @brief Unit tests for command sequence validation
 */

#include "Commands/Validation.hpp"
#include "Commands/CommandContext.hpp"
#include "Commands/CommandDescriptor.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace commands;

// =============================================================================
// findUnresolvedVariables tests
// =============================================================================

TEST_CASE("findUnresolvedVariables returns empty for string with no variables",
          "[commands][validation]") {
    auto const result = findUnresolvedVariables("plain text");
    REQUIRE(result.empty());
}

TEST_CASE("findUnresolvedVariables finds a single variable",
          "[commands][validation]") {
    auto const result = findUnresolvedVariables("key_${item}_suffix");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "item");
}

TEST_CASE("findUnresolvedVariables finds multiple variables",
          "[commands][validation]") {
    auto const result =
            findUnresolvedVariables("${source} to ${destination}");
    REQUIRE(result.size() == 2);
    CHECK(result[0] == "source");
    CHECK(result[1] == "destination");
}

TEST_CASE("findUnresolvedVariables handles malformed variable (no closing brace)",
          "[commands][validation]") {
    auto const result = findUnresolvedVariables("${unclosed");
    REQUIRE(result.empty());
}

TEST_CASE("findUnresolvedVariables returns empty for empty string",
          "[commands][validation]") {
    auto const result = findUnresolvedVariables("");
    REQUIRE(result.empty());
}

// =============================================================================
// Check 1: Structural — unknown command names
// =============================================================================

TEST_CASE("validateSequence reports error for unknown command name",
          "[commands][validation]") {
    CommandSequenceDescriptor seq;
    CommandDescriptor cmd;
    cmd.command_name = "NonExistentCommand";
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].find("unknown command name") != std::string::npos);
}

TEST_CASE("validateSequence passes structural check for all known commands",
          "[commands][validation]") {

    auto makeDesc = [](std::string const & name, std::string const & json) {
        CommandDescriptor d;
        d.command_name = name;
        d.parameters = rfl::json::read<rfl::Generic>(json).value();
        return d;
    };

    CommandSequenceDescriptor seq;
    seq.commands = {
            makeDesc("MoveByTimeRange",
                     R"({"source_key":"a","destination_key":"b","start_frame":0,"end_frame":10})"),
            makeDesc("CopyByTimeRange",
                     R"({"source_key":"a","destination_key":"b","start_frame":0,"end_frame":10})"),
            makeDesc("AddInterval",
                     R"({"interval_key":"k","start_frame":0,"end_frame":10,"create_if_missing":true})"),
            makeDesc("ForEachKey",
                     R"({"items":["a"],"variable":"item","commands":[]})")};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

TEST_CASE("validateSequence resolves command name via variable substitution before structural check",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "${cmd_name}";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"interval_key":"k","start_frame":0,"end_frame":10,"create_if_missing":true})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.variables = std::map<std::string, std::string>{
            {"cmd_name", "AddInterval"}};
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

// =============================================================================
// Check 2: Deserialization — invalid parameter structs
// =============================================================================

TEST_CASE("validateSequence reports error for invalid params on known command",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    // Missing required fields
    cmd.parameters =
            rfl::json::read<rfl::Generic>(R"({"wrong_field":"value"})").value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].find("deserialize") != std::string::npos);
}

TEST_CASE("validateSequence passes deserialization for valid MoveByTimeRange params",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"src","destination_key":"dst","start_frame":10,"end_frame":20})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

// =============================================================================
// Check 3: Variable resolution — unresolved ${variable} references
// =============================================================================

TEST_CASE("validateSequence reports unresolved variables in parameters",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"${src}","destination_key":"dst","start_frame":10,"end_frame":20})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    CHECK_FALSE(result.errors.empty());
    // Should mention the unresolved variable name
    bool found = false;
    for (auto const & err: result.errors) {
        if (err.find("${src}") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("validateSequence resolves static variables before checking resolution",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"${src}","destination_key":"dst","start_frame":10,"end_frame":20})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.variables =
            std::map<std::string, std::string>{{"src", "actual_key"}};
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

TEST_CASE("validateSequence resolves runtime variables before checking resolution",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"${src}","destination_key":"${dst}","start_frame":10,"end_frame":20})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.runtime_variables = {
            {"src", "source_lines"},
            {"dst", "dest_lines"}};

    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

// =============================================================================
// Check 4: Data key existence (requires DataManager)
// =============================================================================

TEST_CASE("validateSequence warns when source key not found in DataManager",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("dst_lines", TimeKey("time"));

    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"missing","destination_key":"dst_lines","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    // Key existence is a warning, not an error
    CHECK(result.valid);
    REQUIRE(result.warnings.size() >= 1);
    CHECK(result.warnings[0].find("source key") != std::string::npos);
    CHECK(result.warnings[0].find("missing") != std::string::npos);
}

TEST_CASE("validateSequence warns when destination key not found in DataManager",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src_lines", TimeKey("time"));

    CommandDescriptor cmd;
    cmd.command_name = "CopyByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"src_lines","destination_key":"missing","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    REQUIRE(result.warnings.size() >= 1);
    CHECK(result.warnings[0].find("destination key") != std::string::npos);
}

TEST_CASE("validateSequence no warnings when keys exist in DataManager",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src", TimeKey("time"));
    dm->setData<LineData>("dst", TimeKey("time"));

    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"src","destination_key":"dst","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.warnings.empty());
    CHECK(result.errors.empty());
}

TEST_CASE("validateSequence warns for AddInterval with create_if_missing=false and missing key",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();

    CommandDescriptor cmd;
    cmd.command_name = "AddInterval";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"interval_key":"missing","start_frame":0,"end_frame":10,"create_if_missing":false})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].find("create_if_missing") != std::string::npos);
}

TEST_CASE("validateSequence no warning for AddInterval with create_if_missing=true",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();

    CommandDescriptor cmd;
    cmd.command_name = "AddInterval";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"interval_key":"new_key","start_frame":0,"end_frame":10,"create_if_missing":true})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.warnings.empty());
}

TEST_CASE("validateSequence skips data key checks when no DataManager",
          "[commands][validation]") {
    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"nonexistent_src","destination_key":"nonexistent_dst","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    // No data_manager set
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.warnings.empty());
    CHECK(result.errors.empty());
}

// =============================================================================
// Check 5: Type compatibility — source/destination type mismatch
// =============================================================================

TEST_CASE("validateSequence reports error for type mismatch between source and destination",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("line_obj", TimeKey("time"));
    dm->setData<PointData>("point_obj", TimeKey("time"));

    CommandDescriptor cmd;
    cmd.command_name = "MoveByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"line_obj","destination_key":"point_obj","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].find("incompatible types") != std::string::npos);
}

TEST_CASE("validateSequence no error for matching types",
          "[commands][validation]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src", TimeKey("time"));
    dm->setData<LineData>("dst", TimeKey("time"));

    CommandDescriptor cmd;
    cmd.command_name = "CopyByTimeRange";
    cmd.parameters = rfl::json::read<rfl::Generic>(
                             R"({"source_key":"src","destination_key":"dst","start_frame":0,"end_frame":10})")
                             .value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    ctx.data_manager = dm;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

// =============================================================================
// ForEachKey recursive validation
// =============================================================================

TEST_CASE("validateSequence recursively validates ForEachKey sub-commands",
          "[commands][validation]") {
    // ForEachKey with an unknown sub-command should report an error
    auto const params_json = R"({
        "items": ["w0", "w1"],
        "variable": "item",
        "commands": [
            {
                "command_name": "UnknownSubCommand",
                "parameters": {}
            }
        ]
    })";

    CommandDescriptor cmd;
    cmd.command_name = "ForEachKey";
    cmd.parameters = rfl::json::read<rfl::Generic>(params_json).value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    REQUIRE_FALSE(result.errors.empty());
    // Error should be prefixed with "ForEachKey →"
    CHECK(result.errors[0].find("ForEachKey") != std::string::npos);
    CHECK(result.errors[0].find("unknown command name") != std::string::npos);
}

TEST_CASE("validateSequence ForEachKey iteration variable counts as resolved",
          "[commands][validation]") {
    // Sub-command uses ${item} which is the ForEachKey variable — should be ok
    auto const params_json = R"({
        "items": ["w0", "w1"],
        "variable": "item",
        "commands": [
            {
                "command_name": "MoveByTimeRange",
                "parameters": {
                    "source_key": "pred_${item}",
                    "destination_key": "gt_${item}",
                    "start_frame": 0,
                    "end_frame": 100
                }
            }
        ]
    })";

    CommandDescriptor cmd;
    cmd.command_name = "ForEachKey";
    cmd.parameters = rfl::json::read<rfl::Generic>(params_json).value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
}

TEST_CASE("validateSequence ForEachKey reports unresolved variables not covered by iteration variable",
          "[commands][validation]") {
    auto const params_json = R"({
        "items": ["w0"],
        "variable": "item",
        "commands": [
            {
                "command_name": "MoveByTimeRange",
                "parameters": {
                    "source_key": "pred_${item}",
                    "destination_key": "gt_${unknown_var}",
                    "start_frame": 0,
                    "end_frame": 100
                }
            }
        ]
    })";

    CommandDescriptor cmd;
    cmd.command_name = "ForEachKey";
    cmd.parameters = rfl::json::read<rfl::Generic>(params_json).value();

    CommandSequenceDescriptor seq;
    seq.commands = {cmd};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    CHECK_FALSE(result.errors.empty());
    bool found = false;
    for (auto const & err: result.errors) {
        if (err.find("${unknown_var}") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

// =============================================================================
// Multi-command sequence validation
// =============================================================================

TEST_CASE("validateSequence validates all commands in a sequence",
          "[commands][validation]") {
    CommandSequenceDescriptor seq;

    CommandDescriptor good;
    good.command_name = "AddInterval";
    good.parameters = rfl::json::read<rfl::Generic>(
                              R"({"interval_key":"k","start_frame":0,"end_frame":10,"create_if_missing":true})")
                              .value();

    CommandDescriptor bad;
    bad.command_name = "NotACommand";

    seq.commands = {good, bad};

    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    REQUIRE_FALSE(result.valid);
    // Should report error only for the second command
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].find("NotACommand") != std::string::npos);
}

TEST_CASE("validateSequence returns valid for empty sequence",
          "[commands][validation]") {
    CommandSequenceDescriptor seq;
    CommandContext ctx;
    auto result = validateSequence(seq, ctx);

    CHECK(result.valid);
    CHECK(result.errors.empty());
    CHECK(result.warnings.empty());
}
