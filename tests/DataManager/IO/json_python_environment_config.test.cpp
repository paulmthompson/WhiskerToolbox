/**
 * @file json_python_environment_config.test.cpp
 * @brief Tests for DataManager JSON Python environment configurator hook.
 */

#include "DataManager.hpp"

#include "nlohmann/json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("load_data_from_json_config invokes python configurator", "[data-manager][json][python]") {
    DataManager dm;
    bool called = false;

    setJsonPythonEnvironmentConfigurator([&](nlohmann::json const & config, std::string & error_message) {
        called = true;
        REQUIRE(config.at("required_imports").at(0).get<std::string>() == "example_module");
        error_message.clear();
        return true;
    });

    auto const config = nlohmann::json{
            {"python", {{"required_imports", {"example_module"}}}},
            {"data", nlohmann::json::array()}};

    auto loaded = load_data_from_json_config(&dm, config, ".");

    REQUIRE(called);
    REQUIRE(loaded.empty());

    clearJsonPythonEnvironmentConfigurator();
}

TEST_CASE("load_data_from_json_config stops when python configurator fails", "[data-manager][json][python]") {
    DataManager dm;
    bool called = false;

    setJsonPythonEnvironmentConfigurator([&](nlohmann::json const &, std::string & error_message) {
        called = true;
        error_message = "intentional test failure";
        return false;
    });

    auto const config = nlohmann::json{
            {"python", nlohmann::json::object()},
            {"data", nlohmann::json::array({{{"name", "should_not_load"},
                                             {"format", "csv"},
                                             {"data_type", "analog"},
                                             {"filepath", "missing.csv"}}})}};

    auto loaded = load_data_from_json_config(&dm, config, ".");

    REQUIRE(called);
    REQUIRE(loaded.empty());
    REQUIRE(dm.getAllKeys().size() == 1);// default media only

    clearJsonPythonEnvironmentConfigurator();
}
