/**
 * @file JsonPythonEnvironment.cpp
 * @brief JSON-driven Python environment configuration helpers.
 */

#include "JsonPythonEnvironment.hpp"

#include "PythonEngine.hpp"

#include "nlohmann/json.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

std::vector<std::string> readStringArray(nlohmann::json const & config, char const * key) {
    std::vector<std::string> values;
    if (!config.contains(key) || !config[key].is_array()) {
        return values;
    }

    for (auto const & item: config[key]) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

bool prependSysPaths(std::vector<std::string> const & paths, std::string & error_message) {
    if (paths.empty()) {
        return true;
    }

    try {
        py::gil_scoped_acquire const gil;
        py::module_ const sys = py::module_::import("sys");
        py::list const sys_path = sys.attr("path");
        for (auto it = paths.rbegin(); it != paths.rend(); ++it) {
            sys_path.attr("insert")(0, *it);
        }
        return true;
    } catch (py::error_already_set const & e) {
        error_message = std::string("Failed to update Python sys.path: ") + e.what();
        return false;
    }
}

bool verifyRequiredImports(nlohmann::json const & config, std::string & error_message) {
    auto const required_imports = readStringArray(config, "required_imports");
    bool const fail_on_missing = config.value("fail_on_missing_imports", true);
    std::vector<std::string> missing;

    for (auto const & import_name: required_imports) {
        try {
            py::gil_scoped_acquire const gil;
            py::module_::import(import_name.c_str());
        } catch (py::error_already_set const & e) {
            if (fail_on_missing) {
                error_message = "Required Python import failed for '" + import_name + "': " + e.what();
                return false;
            }
            missing.push_back(import_name);
        }
    }

    if (!missing.empty()) {
        std::ostringstream stream;
        stream << "Missing optional Python imports:";
        for (auto const & name: missing) {
            stream << ' ' << name;
        }
        error_message = stream.str();
    }
    return true;
}

}// namespace

bool configurePythonEnvironmentFromJson(PythonEngine & engine,
                                        nlohmann::json const & config,
                                        std::string & error_message) {
    if (!engine.isInitialized()) {
        error_message = "Embedded Python interpreter is not initialized.";
        return false;
    }

    if (config.contains("venv_path")) {
        if (!config["venv_path"].is_string()) {
            error_message = "python.venv_path must be a string.";
            return false;
        }

        auto const venv_path = std::filesystem::path(config["venv_path"].get<std::string>());
        auto const activation_error = engine.activateVenv(venv_path);
        if (!activation_error.empty()) {
            error_message = activation_error;
            return false;
        }
    }

    if (!prependSysPaths(readStringArray(config, "prepend_sys_paths"), error_message)) {
        return false;
    }

    return verifyRequiredImports(config, error_message);
}
