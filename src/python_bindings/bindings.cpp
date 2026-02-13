/**
 * @file bindings.cpp
 * @brief PYBIND11_EMBEDDED_MODULE registration for whiskertoolbox_python.
 *
 * This module is compiled into the application (not a standalone .so).
 * It is available to the embedded Python interpreter via:
 *   import whiskertoolbox_python as wt
 */

#include "bind_module.hpp"

#include <pybind11/embed.h>

// Force-linkage: called by PythonEngine to guarantee this TU is included
// when linking from a static archive.
void ensure_whiskertoolbox_bindings_linked() {
    // intentionally empty
}

PYBIND11_EMBEDDED_MODULE(whiskertoolbox_python, m) {
    m.doc() = "WhiskerToolbox Python Bindings — data types for neural analysis";

    // Foundation types (must be registered first)
    init_geometry(m);
    init_timeframe(m);
    init_entity(m);

    // Data types
    init_analog(m);
    init_digital_event(m);
    init_digital_interval(m);
    init_line(m);
    init_mask(m);
    init_point(m);
    init_tensor(m);
    init_media(m);

    // DataManager proxy (depends on all types above)
    init_datamanager(m);
}
