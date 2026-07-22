/**
 * @file json_data_load_expansion.test.cpp
 * @brief Tests for JSON load-config loop expansion.
 */

#include "utils/JsonDataLoadExpansion.hpp"

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

TEST_CASE("parseLoopDefinition expands inclusive integer ranges",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const range = {{"from", 0}, {"to", 2}};
    auto const values = parseLoopDefinition(range);
    REQUIRE(values.has_value());
    REQUIRE(values->size() == 3);
    CHECK((*values)[0] == "0");// NOLINT(bugprone-unchecked-optional-access)
    CHECK((*values)[1] == "1");// NOLINT(bugprone-unchecked-optional-access)
    CHECK((*values)[2] == "2");// NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("parseLoopDefinition accepts explicit arrays",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const explicit_loop = nlohmann::json::array({0, "a", 3});
    auto const values = parseLoopDefinition(explicit_loop);
    REQUIRE(values.has_value());
    REQUIRE(values->size() == 3);
    CHECK((*values)[0] == "0");// NOLINT(bugprone-unchecked-optional-access)
    CHECK((*values)[1] == "a");// NOLINT(bugprone-unchecked-optional-access)
    CHECK((*values)[2] == "3");// NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("collectBracePlaceholders ignores dollar-brace variables",
          "[DataManager][JsonDataLoadExpansion]") {
    auto const placeholders =
            collectBracePlaceholders(std::string{"${root}/path/{whisker_id}.csv"});
    REQUIRE(placeholders.size() == 1);
    CHECK(*placeholders.begin() == "whisker_id");
}

TEST_CASE("expandDataArrayLoops expands templated entries",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const data = nlohmann::json::array({
            {{"filepath", "whisker_{whisker_id}.csv"},
             {"name", "whisker_{whisker_id}"},
             {"data_type", "line"}},
            {{"filepath", "static.csv"}, {"name", "static"}, {"data_type", "line"}},
    });
    nlohmann::json const loops = {{"whisker_id", {{"from", 0}, {"to", 1}}}};

    auto const expanded = expandDataArrayLoops(data, loops);
    REQUIRE(expanded.size() == 3);
    CHECK(expanded[0]["filepath"] == "whisker_0.csv");
    CHECK(expanded[0]["name"] == "whisker_0");
    CHECK(expanded[1]["filepath"] == "whisker_1.csv");
    CHECK(expanded[1]["name"] == "whisker_1");
    CHECK(expanded[2]["filepath"] == "static.csv");
}

TEST_CASE("expandDataArrayLoops skips entry with multiple loop placeholders",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const data = nlohmann::json::array({
            {{"filepath", "{a}_{b}.csv"}, {"name", "bad"}, {"data_type", "line"}},
    });
    nlohmann::json const loops = {
            {"a", nlohmann::json::array({0})},
            {"b", nlohmann::json::array({1})},
    };

    auto const expanded = expandDataArrayLoops(data, loops);
    CHECK(expanded.empty());
}

TEST_CASE("expandDataArrayLoops skips entry with unknown loop placeholder",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const data = nlohmann::json::array({
            {{"filepath", "whisker_{missing}.csv"}, {"name", "x"}, {"data_type", "line"}},
    });
    nlohmann::json const loops = {{"whisker_id", {{"from", 0}, {"to", 1}}}};

    auto const expanded = expandDataArrayLoops(data, loops);
    CHECK(expanded.empty());
}

TEST_CASE("expandDataArrayLoops passes through transformation blocks",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json const data = nlohmann::json::array({
            {{"transformations", {{"steps", nlohmann::json::array()}}}},
    });
    nlohmann::json const loops = {{"whisker_id", {{"from", 0}, {"to", 1}}}};

    auto const expanded = expandDataArrayLoops(data, loops);
    REQUIRE(expanded.size() == 1);
    CHECK(expanded[0].contains("transformations"));
}

TEST_CASE("substituteBracePlaceholders replaces values in nested fields",
          "[DataManager][JsonDataLoadExpansion]") {
    nlohmann::json value = {{"meta", {{"path", "dir/{id}/file.csv"}}}};
    auto const result = substituteBracePlaceholders(std::move(value), {{"id", "7"}});
    CHECK(result["meta"]["path"] == "dir/7/file.csv");
}
