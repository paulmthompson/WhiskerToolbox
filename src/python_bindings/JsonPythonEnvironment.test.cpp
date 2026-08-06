/**
 * @file JsonPythonEnvironment.test.cpp
 * @brief Tests for JSON-driven Python environment configuration.
 */

#include "JsonPythonEnvironment.hpp"
#include "PythonEngine.hpp"

#include "nlohmann/json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

PythonEngine & engine() {
    static PythonEngine eng;
    return eng;
}

std::filesystem::path makeModuleDir(std::string const & module_name) {
    auto const dir = std::filesystem::temp_directory_path() / "neuralyzer_json_python_environment_test";
    std::filesystem::create_directories(dir);

    std::ofstream module_file(dir / (module_name + ".py"));
    module_file << "VALUE = 123\n";
    return dir;
}

}// namespace

TEST_CASE("configurePythonEnvironmentFromJson prepends sys.path and verifies import", "[python][json-environment]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto const module_dir = makeModuleDir("neuralyzer_probe_module");
    std::string error;
    auto const config = nlohmann::json{
            {"prepend_sys_paths", {module_dir.string()}},
            {"required_imports", {"neuralyzer_probe_module"}},
            {"fail_on_missing_imports", true}};

    REQUIRE(configurePythonEnvironmentFromJson(eng, config, error));
    REQUIRE(error.empty());

    auto result = eng.execute("import neuralyzer_probe_module\nprint(neuralyzer_probe_module.VALUE)");
    REQUIRE(result.success);
    REQUIRE(result.stdout_text == "123\n");
}

TEST_CASE("configurePythonEnvironmentFromJson reports missing required imports", "[python][json-environment]") {
    auto & eng = engine();
    eng.resetNamespace();

    std::string error;
    auto const config = nlohmann::json{
            {"required_imports", {"module_that_should_not_exist_neuralyzer_test"}},
            {"fail_on_missing_imports", true}};

    REQUIRE_FALSE(configurePythonEnvironmentFromJson(eng, config, error));
    REQUIRE_THAT(error, Catch::Matchers::ContainsSubstring("module_that_should_not_exist_neuralyzer_test"));
}

TEST_CASE("configurePythonEnvironmentFromJson allows missing optional imports", "[python][json-environment]") {
    auto & eng = engine();
    eng.resetNamespace();

    std::string error;
    auto const config = nlohmann::json{
            {"required_imports", {"module_that_should_not_exist_neuralyzer_optional_test"}},
            {"fail_on_missing_imports", false}};

    REQUIRE(configurePythonEnvironmentFromJson(eng, config, error));
    REQUIRE_THAT(error, Catch::Matchers::ContainsSubstring("module_that_should_not_exist_neuralyzer_optional_test"));
}
