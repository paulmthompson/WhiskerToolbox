/**
 * @file CommandInfo.test.cpp
 * @brief Unit tests for CommandInfo and factory query functions
 */

#include "DataManager/Commands/CommandInfo.hpp"
#include "DataManager/Commands/CommandFactory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace commands;

// -- getAvailableCommands tests --

TEST_CASE("getAvailableCommands returns all known commands", "[commands][introspection]") {
    auto const all = getAvailableCommands();

    std::vector<std::string> const expected_names = {
            "MoveByTimeRange", "CopyByTimeRange", "AddInterval", "ForEachKey",
            "SaveData", "LoadData"};

    REQUIRE(all.size() == expected_names.size());

    for (auto const & expected: expected_names) {
        auto const it = std::find_if(
                all.begin(), all.end(),
                [&](CommandInfo const & info) { return info.name == expected; });
        REQUIRE(it != all.end());
    }
}

TEST_CASE("getAvailableCommands entries have non-empty descriptions", "[commands][introspection]") {
    auto const all = getAvailableCommands();
    for (auto const & info: all) {
        REQUIRE_FALSE(info.name.empty());
        REQUIRE_FALSE(info.description.empty());
        REQUIRE_FALSE(info.category.empty());
    }
}

// -- getCommandInfo tests --

TEST_CASE("getCommandInfo returns correct info for MoveByTimeRange", "[commands][introspection]") {
    auto const info = getCommandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    CHECK(info->name == "MoveByTimeRange");
    CHECK(info->category == "data_mutation");
    CHECK(info->supports_undo == true);
    CHECK_FALSE(info->supported_data_types.empty());
}

TEST_CASE("getCommandInfo returns correct info for CopyByTimeRange", "[commands][introspection]") {
    auto const info = getCommandInfo("CopyByTimeRange");
    REQUIRE(info.has_value());
    CHECK(info->name == "CopyByTimeRange");
    CHECK(info->category == "data_mutation");
    CHECK(info->supports_undo == false);
}

TEST_CASE("getCommandInfo returns correct info for AddInterval", "[commands][introspection]") {
    auto const info = getCommandInfo("AddInterval");
    REQUIRE(info.has_value());
    CHECK(info->name == "AddInterval");
    CHECK(info->category == "data_mutation");
    CHECK(info->supports_undo == false);
}

TEST_CASE("getCommandInfo returns correct info for ForEachKey", "[commands][introspection]") {
    auto const info = getCommandInfo("ForEachKey");
    REQUIRE(info.has_value());
    CHECK(info->name == "ForEachKey");
    CHECK(info->category == "meta");
}

TEST_CASE("getCommandInfo returns nullopt for unknown command", "[commands][introspection]") {
    auto const info = getCommandInfo("NonExistentCommand");
    REQUIRE_FALSE(info.has_value());
}

TEST_CASE("getCommandInfo returns nullopt for empty name", "[commands][introspection]") {
    auto const info = getCommandInfo("");
    REQUIRE_FALSE(info.has_value());
}

// -- ParameterSchema correctness tests --

TEST_CASE("MoveByTimeRange schema has correct field count", "[commands][introspection]") {
    auto const info = getCommandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    REQUIRE(schema.fields.size() == 4);
    CHECK(schema.field("source_key") != nullptr);
    CHECK(schema.field("destination_key") != nullptr);
    CHECK(schema.field("start_frame") != nullptr);
    CHECK(schema.field("end_frame") != nullptr);
}

TEST_CASE("CopyByTimeRange schema has correct field count", "[commands][introspection]") {
    auto const info = getCommandInfo("CopyByTimeRange");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    REQUIRE(schema.fields.size() == 4);
    CHECK(schema.field("source_key") != nullptr);
    CHECK(schema.field("destination_key") != nullptr);
    CHECK(schema.field("start_frame") != nullptr);
    CHECK(schema.field("end_frame") != nullptr);
}

TEST_CASE("AddInterval schema has correct field count", "[commands][introspection]") {
    auto const info = getCommandInfo("AddInterval");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    REQUIRE(schema.fields.size() == 4);
    CHECK(schema.field("interval_key") != nullptr);
    CHECK(schema.field("start_frame") != nullptr);
    CHECK(schema.field("end_frame") != nullptr);
    CHECK(schema.field("create_if_missing") != nullptr);
}

TEST_CASE("ForEachKey schema has correct field count", "[commands][introspection]") {
    auto const info = getCommandInfo("ForEachKey");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    REQUIRE(schema.fields.size() == 3);
    CHECK(schema.field("items") != nullptr);
    CHECK(schema.field("variable") != nullptr);
    CHECK(schema.field("commands") != nullptr);
}

// -- ParameterUIHints tests --

TEST_CASE("MoveByTimeRange schema fields have tooltips from UIHints", "[commands][introspection]") {
    auto const info = getCommandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    CHECK_FALSE(schema.field("source_key")->tooltip.empty());
    CHECK_FALSE(schema.field("destination_key")->tooltip.empty());
    CHECK_FALSE(schema.field("start_frame")->tooltip.empty());
    CHECK_FALSE(schema.field("end_frame")->tooltip.empty());
}

TEST_CASE("AddInterval schema fields have tooltips from UIHints", "[commands][introspection]") {
    auto const info = getCommandInfo("AddInterval");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    CHECK_FALSE(schema.field("interval_key")->tooltip.empty());
    CHECK_FALSE(schema.field("create_if_missing")->tooltip.empty());
}

TEST_CASE("ForEachKey commands field is marked advanced", "[commands][introspection]") {
    auto const info = getCommandInfo("ForEachKey");
    REQUIRE(info.has_value());
    auto const * commands_field = info->parameter_schema.field("commands");
    REQUIRE(commands_field != nullptr);
    CHECK(commands_field->is_advanced == true);
}

// -- Schema type correctness --

TEST_CASE("MoveByTimeRange schema reports correct field types", "[commands][introspection]") {
    auto const info = getCommandInfo("MoveByTimeRange");
    REQUIRE(info.has_value());
    auto const & schema = info->parameter_schema;
    CHECK(schema.field("source_key")->type_name == "std::string");
    // int64_t may be reported as "int" (via determineBaseType), "long", or "int64_t" depending on platform
    auto const & frame_type = schema.field("start_frame")->type_name;
    CHECK((frame_type == "int64_t" || frame_type == "long" || frame_type == "int"));
}

TEST_CASE("AddInterval schema reports bool type for create_if_missing", "[commands][introspection]") {
    auto const info = getCommandInfo("AddInterval");
    REQUIRE(info.has_value());
    CHECK(info->parameter_schema.field("create_if_missing")->type_name == "bool");
}
