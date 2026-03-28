#include "bind_module.hpp"
#include "numpy_utils.hpp"

#include <pybind11/stl.h>

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <optional>
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
            .def_property_readonly("values", [](std::shared_ptr<TensorData> const & self) {
                auto span = self->flatData();
                auto arr = wt::python::span_to_numpy_readonly(span, py::cast(self));
                // Reshape to the tensor's shape
                auto shape_vec = self->shape();
                std::vector<py::ssize_t> const shape(shape_vec.begin(), shape_vec.end());
                return arr.reshape(shape); }, "Read-only NumPy view of the tensor data, reshaped to shape() (zero-copy)")

            .def("flatValues", [](std::shared_ptr<TensorData> const & self) {
                auto span = self->flatData();
                return wt::python::span_to_numpy_readonly(span, py::cast(self)); }, "Read-only 1-D NumPy view of the flat data (zero-copy)")

            // --- Copy-based access (always works, no NumPy) ---
            .def("toList", &TensorData::materializeFlat, "Copy flat data to a Python list (row-major order)")

            // --- Factory methods ---
            .def_static("createOrdinal2D", &TensorData::createOrdinal2D, py::arg("data"), py::arg("rows"), py::arg("cols"), py::arg("column_names") = std::vector<std::string>{}, "Create a 2-D tensor from flat float data with ordinal row indices")

            .def_static("fromNumpy2D", [](py::array_t<float, py::array::c_style | py::array::forcecast> const & arr, std::vector<std::string> column_names) -> std::shared_ptr<TensorData> {
                auto buf = arr.request();
                if (buf.ndim != 2) {
                    throw std::invalid_argument(
                        "fromNumpy2D expects a 2-D array, got " + std::to_string(buf.ndim) + "-D");
                }
                auto const rows = static_cast<std::size_t>(buf.shape[0]);
                auto const cols = static_cast<std::size_t>(buf.shape[1]);
                auto data = wt::python::numpy_to_vector<float>(arr);
                return std::make_shared<TensorData>(
                    TensorData::createOrdinal2D(data, rows, cols, std::move(column_names))); }, py::arg("array"), py::arg("column_names") = std::vector<std::string>{}, "Create a 2-D TensorData from a NumPy array (copies data). "
                                                                                                                                                                                                                                                                        "Optionally provide column names matching the number of columns.")

            // --- TimeFrame ---
            .def("setTimeFrame", &TensorData::setTimeFrame, py::arg("time_frame"))
            .def("getTimeFrame", &TensorData::getTimeFrame)

            // --- Row-to-TimeFrameIndex mapping ---
            .def("rowType", [](TensorData const & self) -> std::string {
                switch (self.rows().type()) {
                    case RowType::TimeFrameIndex: return "TimeFrameIndex";
                    case RowType::Interval: return "Interval";
                    case RowType::Ordinal: return "Ordinal";
                }
                return "Unknown"; }, "Row type: 'TimeFrameIndex', 'Interval', or 'Ordinal'")

            .def("rowTimeIndices", [](TensorData const & self) -> std::vector<TimeFrameIndex> {
                auto const & rd = self.rows();
                if (rd.type() != RowType::TimeFrameIndex) {
                    throw std::logic_error(
                        "rowTimeIndices() requires RowType::TimeFrameIndex rows");
                }
                auto const & ts = rd.timeStorage();
                return ts.getAllTimeIndices(); }, "Get TimeFrameIndex for every row (only for TimeFrameIndex-typed rows)")

            .def("findRowForTime", [](TensorData const & self, TimeFrameIndex time_index) -> std::optional<std::size_t> {
                auto const & rd = self.rows();
                if (rd.type() != RowType::TimeFrameIndex) {
                    throw std::logic_error(
                        "findRowForTime() requires RowType::TimeFrameIndex rows");
                }
                return rd.timeStorage().findArrayPositionForTimeIndex(time_index); }, py::arg("time_index"), "Find the row index for a given TimeFrameIndex, or None if not found")

            .def("findRowGreaterOrEqual", [](TensorData const & self, TimeFrameIndex time_index) -> std::optional<std::size_t> {
                auto const & rd = self.rows();
                if (rd.type() != RowType::TimeFrameIndex) {
                    throw std::logic_error(
                        "findRowGreaterOrEqual() requires RowType::TimeFrameIndex rows");
                }
                return rd.timeStorage().findArrayPositionGreaterOrEqual(time_index); }, py::arg("time_index"), "Find the row for the smallest TimeFrameIndex >= the given value, or None")

            .def("findRowLessOrEqual", [](TensorData const & self, TimeFrameIndex time_index) -> std::optional<std::size_t> {
                auto const & rd = self.rows();
                if (rd.type() != RowType::TimeFrameIndex) {
                    throw std::logic_error(
                        "findRowLessOrEqual() requires RowType::TimeFrameIndex rows");
                }
                return rd.timeStorage().findArrayPositionLessOrEqual(time_index); }, py::arg("time_index"), "Find the row for the largest TimeFrameIndex <= the given value, or None")

            .def("__repr__", [](TensorData const & self) {
            auto s = self.shape();
            std::string shape_str = "(";
            for (size_t i = 0; i < s.size(); ++i) {
                if (i > 0) shape_str += ", ";
                shape_str += std::to_string(s[i]);
            }
            shape_str += ")";
            return "TensorData(shape=" + shape_str + ")"; });
}
