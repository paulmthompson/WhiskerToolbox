#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for binding functions
void bind_interval(py::module_ &m);
void bind_digital_interval_series(py::module_ &m);
void bind_transforms(py::module_ &m);

PYBIND11_MODULE(whiskertoolbox, m) {
    m.doc() = "WhiskerToolbox Python bindings for data types and transforms";

    // Bind the types
    bind_interval(m);
    bind_digital_interval_series(m);
    bind_transforms(m);
}
