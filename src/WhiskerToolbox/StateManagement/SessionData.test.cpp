/**
 * @file SessionData.test.cpp
 * @brief Unit tests for SessionData (reflect-cpp serialization)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "StateManagement/SessionData.hpp"

#include <rfl/json.hpp>

using namespace StateManagement;

TEST_CASE("SessionData - default construction", "[StateManagement][Session]") {
    SessionData data;

    CHECK(data.version == "1.0");
    CHECK(data.last_used_paths.empty());
    CHECK(data.recent_workspaces.empty());
    CHECK(data.window_x == 100);
    CHECK(data.window_y == 100);
    CHECK(data.window_width == 1400);
    CHECK(data.window_height == 900);
    CHECK_FALSE(data.window_maximized);
}

TEST_CASE("SessionData - round trip serialization", "[StateManagement][Session]") {
    SessionData original;
    original.last_used_paths = {
        {"import_csv", "/data/csv"},
        {"load_video", "/data/videos"},
        {"export_table", "/results"}
    };
    original.recent_workspaces = {
        "/exp01/workspace.whisker",
        "/exp02/workspace.whisker"
    };
    original.window_x = 200;
    original.window_y = 150;
    original.window_width = 1920;
    original.window_height = 1080;
    original.window_maximized = true;

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<SessionData>(json);

    REQUIRE(result);
    auto const & restored = *result;

    CHECK(restored.version == original.version);
    CHECK(restored.last_used_paths == original.last_used_paths);
    CHECK(restored.recent_workspaces == original.recent_workspaces);
    CHECK(restored.window_x == original.window_x);
    CHECK(restored.window_y == original.window_y);
    CHECK(restored.window_width == original.window_width);
    CHECK(restored.window_height == original.window_height);
    CHECK(restored.window_maximized == original.window_maximized);
}

TEST_CASE("SessionData - default-constructed round trip", "[StateManagement][Session]") {
    // Default-constructed data should serialize and deserialize cleanly
    SessionData defaults;
    auto const json = rfl::json::write(defaults);
    auto const result = rfl::json::read<SessionData>(json);

    REQUIRE(result);
    auto const & data = *result;

    CHECK(data.version == "1.0");
    CHECK(data.last_used_paths.empty());
    CHECK(data.recent_workspaces.empty());
    CHECK(data.window_width == 1400);
    CHECK(data.window_height == 900);
    CHECK_FALSE(data.window_maximized);
}

TEST_CASE("SessionData - max_recent_workspaces constant", "[StateManagement][Session]") {
    CHECK(SessionData::max_recent_workspaces == 10);
}

TEST_CASE("SessionData - path memory with many dialogs", "[StateManagement][Session]") {
    SessionData data;
    // Simulate many different file dialogs remembering paths
    for (int i = 0; i < 50; ++i) {
        data.last_used_paths["dialog_" + std::to_string(i)] = "/path/" + std::to_string(i);
    }

    auto const json = rfl::json::write(data);
    auto const result = rfl::json::read<SessionData>(json);

    REQUIRE(result);
    auto const & restored = *result;
    CHECK(restored.last_used_paths.size() == 50);
    CHECK(restored.last_used_paths.at("dialog_42") == "/path/42");
}

TEST_CASE("SessionData - JSON contains expected fields", "[StateManagement][Session]") {
    SessionData data;
    data.window_maximized = true;
    data.last_used_paths["test"] = "/tmp";

    auto const json = rfl::json::write(data);

    using Catch::Matchers::ContainsSubstring;
    CHECK_THAT(json, ContainsSubstring("\"window_maximized\""));
    CHECK_THAT(json, ContainsSubstring("\"last_used_paths\""));
    CHECK_THAT(json, ContainsSubstring("\"test\""));
}

TEST_CASE("SessionData - empty recent workspaces round trip", "[StateManagement][Session]") {
    SessionData data;
    // Explicitly empty
    data.recent_workspaces = {};

    auto const json = rfl::json::write(data);
    auto const result = rfl::json::read<SessionData>(json);

    REQUIRE(result);
    CHECK((*result).recent_workspaces.empty());
}
