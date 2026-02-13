#include "bind_module.hpp"
#include "numpy_utils.hpp"

#include <pybind11/stl.h>

#include "DataManager/Tensors/TensorData.hpp"

#include <string>

void init_tensor(py::module_ & m) {

    py::class_<TensorData, std::shared_ptr<TensorData>>(m, "TensorData",
        "Multi-dimensional tensor data with optional named columns")
        .def(py::init<>())

        // --- Dimension queries ---
        .def("ndim", &TensorData::ndim, "Number of dimensions")
        .def("shape", &TensorData::shape, "Shape as a list of sizes")
        .def("numRows", &TensorData::numRows)
        .def("numColumns", &TensorData::numColumns)
        .def("isEmpty", &TensorData::isEmpty)
        .def("isContiguous", &TensorData::isContiguous)

        // --- Column access ---
        .def("hasNamedColumns", &TensorData::hasNamedColumns)
        .def("columnNames", &TensorData::columnNames,
             py::return_value_policy::reference_internal,
             "Column names (empty if no named columns)")
        .def("setColumnNames", &TensorData::setColumnNames, py::arg("names"))

        .def("getColumn",
            static_cast<std::vector<float> (TensorData::*)(size_t) const>(
                &TensorData::getColumn),
            py::arg("index"),
            "Get a column by index (copy)")
        .def("getColumn",
            static_cast<std::vector<float> (TensorData::*)(std::string_view) const>(
                &TensorData::getColumn),
            py::arg("name"),
            "Get a column by name (copy)")

        .def("row", &TensorData::row, py::arg("index"),
             "Get a row by index (copy)")

        // --- Zero-copy read (requires NumPy at runtime) ---
        .def_property_readonly("values",
            [](std::shared_ptr<TensorData> const & self) {
                auto span = self->flatData();
                auto arr = wt::python::span_to_numpy_readonly(span, py::cast(self));
                // Reshape to the tensor's shape
                auto shape_vec = self->shape();
                std::vector<py::ssize_t> shape(shape_vec.begin(), shape_vec.end());
                return arr.reshape(shape);
            },
            "Read-only NumPy view of the tensor data, reshaped to shape() (zero-copy)")

        .def("flatValues",
            [](std::shared_ptr<TensorData> const & self) {
                auto span = self->flatData();
                return wt::python::span_to_numpy_readonly(span, py::cast(self));
            },
            "Read-only 1-D NumPy view of the flat data (zero-copy)")

        // --- Copy-based access (always works, no NumPy) ---
        .def("toList", &TensorData::materializeFlat,
             "Copy flat data to a Python list (row-major order)")

        // --- Factory methods ---
        .def_static("createOrdinal2D",
            &TensorData::createOrdinal2D,
            py::arg("data"), py::arg("rows"), py::arg("cols"),
            py::arg("column_names") = std::vector<std::string>{},
            "Create a 2-D tensor from flat float data with ordinal row indices")

        // --- TimeFrame ---
        .def("setTimeFrame", &TensorData::setTimeFrame, py::arg("time_frame"))
        .def("getTimeFrame", &TensorData::getTimeFrame)

        .def("__repr__", [](TensorData const & self) {
            auto s = self.shape();
            std::string shape_str = "(";
            for (size_t i = 0; i < s.size(); ++i) {
                if (i > 0) shape_str += ", ";
                shape_str += std::to_string(s[i]);
            }
            shape_str += ")";
            return "TensorData(shape=" + shape_str + ")";
        });
}
