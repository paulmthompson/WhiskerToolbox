#pragma once

/**
 * @file bind_module.hpp
 * @brief Forward declarations for all pybind11 sub-module init functions.
 *
 * Each init_*() function is defined in its own translation unit (bind_*.cpp)
 * and called from the PYBIND11_EMBEDDED_MODULE in bindings.cpp.
 */

#include <pybind11/pybind11.h>

namespace py = pybind11;

// Foundation types (registered first)
void init_geometry(py::module_ & m);
void init_timeframe(py::module_ & m);
void init_entity(py::module_ & m);

// Data types
void init_analog(py::module_ & m);
void init_digital_event(py::module_ & m);
void init_digital_interval(py::module_ & m);
void init_line(py::module_ & m);
void init_mask(py::module_ & m);
void init_point(py::module_ & m);
void init_tensor(py::module_ & m);
void init_media(py::module_ & m);

// DataManager proxy (depends on all types above)
void init_datamanager(py::module_ & m);

/**
 * @brief Force-linkage function.
 *
 * Call this from any translation unit that must guarantee the embedded
 * module (whiskertoolbox_python) is linked in.  Without this, the linker
 * may strip bindings.o from a static archive if no other symbol in that
 * TU is referenced.
 */
void ensure_whiskertoolbox_bindings_linked();
