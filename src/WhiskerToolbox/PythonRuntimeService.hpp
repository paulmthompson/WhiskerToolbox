/**
 * @file PythonRuntimeService.hpp
 * @brief Application-wide access to the embedded Python runtime.
 */

#ifndef WHISKERTOOLBOX_PYTHON_RUNTIME_SERVICE_HPP
#define WHISKERTOOLBOX_PYTHON_RUNTIME_SERVICE_HPP

#include "PythonEngine.hpp"

/**
 * @brief Owns the shared embedded Python engine used by app-level services.
 *
 * The service lets Python-backed loaders and the Python widget use the same
 * interpreter without making lower-level libraries depend on GUI objects.
 */
class PythonRuntimeService {
public:
    static PythonRuntimeService & instance();

    [[nodiscard]] PythonEngine & engine();
    [[nodiscard]] PythonEngine const & engine() const;

private:
    PythonRuntimeService() = default;

    PythonEngine _engine;
};

#endif// WHISKERTOOLBOX_PYTHON_RUNTIME_SERVICE_HPP
