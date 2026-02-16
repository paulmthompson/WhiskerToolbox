/**
 * @file PathResolver.test.cpp
 * @brief Unit tests for PathResolver utility
 *
 * Tests relative/absolute path conversion for workspace portability.
 */

#include <catch2/catch_test_macros.hpp>

#include "StateManagement/PathResolver.hpp"

#include <filesystem>

using namespace StateManagement;

TEST_CASE("PathResolver - toRelative basic conversion", "[StateManagement][PathResolver]") {
    auto const result = PathResolver::toRelative("/home/user/project/data/config.json",
                                                  "/home/user/project");
    CHECK(result == "data/config.json");
}

TEST_CASE("PathResolver - toRelative sibling directory", "[StateManagement][PathResolver]") {
    auto const result = PathResolver::toRelative("/home/user/other/data.csv",
                                                  "/home/user/project");
    CHECK(result == "../other/data.csv");
}

TEST_CASE("PathResolver - toRelative same directory", "[StateManagement][PathResolver]") {
    auto const result = PathResolver::toRelative("/home/user/project/file.json",
                                                  "/home/user/project");
    CHECK(result == "file.json");
}

TEST_CASE("PathResolver - toRelative empty inputs", "[StateManagement][PathResolver]") {
    CHECK(PathResolver::toRelative("", "/home/user") == "");
    CHECK(PathResolver::toRelative("/home/user/file.json", "") == "/home/user/file.json");
}

TEST_CASE("PathResolver - toRelative non-absolute path returns as-is", "[StateManagement][PathResolver]") {
    CHECK(PathResolver::toRelative("relative/path.json", "/home/user") == "relative/path.json");
}

TEST_CASE("PathResolver - toAbsolute basic resolution", "[StateManagement][PathResolver]") {
    auto const result = PathResolver::toAbsolute("data/config.json",
                                                  "/home/user/project");
    CHECK(result == "/home/user/project/data/config.json");
}

TEST_CASE("PathResolver - toAbsolute with parent traversal", "[StateManagement][PathResolver]") {
    auto const result = PathResolver::toAbsolute("../other/data.csv",
                                                  "/home/user/project");
    CHECK(result == "/home/user/other/data.csv");
}

TEST_CASE("PathResolver - toAbsolute already absolute returns as-is", "[StateManagement][PathResolver]") {
    CHECK(PathResolver::toAbsolute("/absolute/path.json", "/home/user") == "/absolute/path.json");
}

TEST_CASE("PathResolver - toAbsolute empty inputs", "[StateManagement][PathResolver]") {
    CHECK(PathResolver::toAbsolute("", "/home/user") == "");
    CHECK(PathResolver::toAbsolute("rel/path", "") == "rel/path");
}

TEST_CASE("PathResolver - round-trip relative then absolute", "[StateManagement][PathResolver]") {
    std::string const original = "/home/user/project/data/experiment.json";
    std::string const base = "/home/user/project";

    auto const relative = PathResolver::toRelative(original, base);
    auto const restored = PathResolver::toAbsolute(relative, base);

    CHECK(restored == original);
}

TEST_CASE("PathResolver - fileExists with temporary file", "[StateManagement][PathResolver]") {
    // A path that almost certainly doesn't exist
    CHECK_FALSE(PathResolver::fileExists("/nonexistent_path_abc123/file.txt"));

    // Current directory should exist
    CHECK(PathResolver::fileExists("."));
}
