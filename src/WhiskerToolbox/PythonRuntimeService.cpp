/**
 * @file PythonRuntimeService.cpp
 * @brief Application-wide embedded Python runtime service implementation.
 */

#include "PythonRuntimeService.hpp"

PythonRuntimeService & PythonRuntimeService::instance() {
    static PythonRuntimeService service;
    return service;
}

PythonEngine & PythonRuntimeService::engine() {
    return _engine;
}

PythonEngine const & PythonRuntimeService::engine() const {
    return _engine;
}
