/**
 * @file JsonPythonEnvironment.hpp
 * @brief JSON-driven Python environment configuration helpers.
 */

#ifndef JSON_PYTHON_ENVIRONMENT_HPP
#define JSON_PYTHON_ENVIRONMENT_HPP

#include "nlohmann/json_fwd.hpp"

#include <string>

class PythonEngine;

/**
 * @brief Configure a PythonEngine from a top-level JSON `python` block.
 *
 * Supported fields:
 * - `venv_path`: venv/conda root to activate
 * - `prepend_sys_paths`: string array inserted at the front of `sys.path`
 * - `required_imports`: import names to verify
 * - `fail_on_missing_imports`: defaults to true
 */
bool configurePythonEnvironmentFromJson(PythonEngine & engine,
                                        nlohmann::json const & config,
                                        std::string & error_message);

#endif// JSON_PYTHON_ENVIRONMENT_HPP
