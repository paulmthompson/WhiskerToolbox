/**
 * @file AppPreferencesData.test.cpp
 * @brief Unit tests for AppPreferencesData (reflect-cpp serialization)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "StateManagement/AppPreferencesData.hpp"

#include <rfl/json.hpp>

using namespace StateManagement;

TEST_CASE("AppPreferencesData - default construction", "[StateManagement][AppPreferences]") {
    AppPreferencesData data;

    CHECK(data.version == "1.0");
    CHECK(data.python_env_search_paths.empty());
    CHECK(data.preferred_python_env.empty());
    CHECK(data.default_import_directory.empty());
    CHECK(data.default_export_directory.empty());
    CHECK(data.default_time_frame_key.empty());
}

TEST_CASE("AppPreferencesData - round trip serialization", "[StateManagement][AppPreferences]") {
    AppPreferencesData original;
    original.python_env_search_paths = {"/usr/bin", "/home/user/.venvs/myenv"};
    original.preferred_python_env = "/home/user/.venvs/myenv/bin/python";
    original.default_import_directory = "/data/experiments";
    original.default_export_directory = "/data/results";
    original.default_time_frame_key = "time";

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<AppPreferencesData>(json);

    REQUIRE(result);
    auto const & restored = *result;

    CHECK(restored.version == original.version);
    CHECK(restored.python_env_search_paths == original.python_env_search_paths);
    CHECK(restored.preferred_python_env == original.preferred_python_env);
    CHECK(restored.default_import_directory == original.default_import_directory);
    CHECK(restored.default_export_directory == original.default_export_directory);
    CHECK(restored.default_time_frame_key == original.default_time_frame_key);
}

TEST_CASE("AppPreferencesData - default-constructed round trip", "[StateManagement][AppPreferences]") {
    // Default-constructed data should serialize and deserialize cleanly
    AppPreferencesData defaults;
    auto const json = rfl::json::write(defaults);
    auto const result = rfl::json::read<AppPreferencesData>(json);

    REQUIRE(result);
    auto const & data = *result;

    CHECK(data.version == "1.0");
    CHECK(data.python_env_search_paths.empty());
    CHECK(data.preferred_python_env.empty());
    CHECK(data.default_import_directory.empty());
    CHECK(data.default_export_directory.empty());
    CHECK(data.default_time_frame_key.empty());
}

TEST_CASE("AppPreferencesData - JSON contains expected fields", "[StateManagement][AppPreferences]") {
    AppPreferencesData data;
    data.default_import_directory = "/some/path";

    auto const json = rfl::json::write(data);

    using Catch::Matchers::ContainsSubstring;
    CHECK_THAT(json, ContainsSubstring("\"version\""));
    CHECK_THAT(json, ContainsSubstring("\"default_import_directory\""));
    CHECK_THAT(json, ContainsSubstring("/some/path"));
}

TEST_CASE("AppPreferencesData - multiple python env paths", "[StateManagement][AppPreferences]") {
    AppPreferencesData data;
    data.python_env_search_paths = {
        "/usr/bin",
        "/opt/conda/envs",
        "/home/user/.pyenv/versions",
        "/home/user/.venvs"
    };

    auto const json = rfl::json::write(data);
    auto const result = rfl::json::read<AppPreferencesData>(json);

    REQUIRE(result);
    auto const & restored = *result;
    REQUIRE(restored.python_env_search_paths.size() == 4);
    CHECK(restored.python_env_search_paths[2] == "/home/user/.pyenv/versions");
}
