#include "bind_module.hpp"
#include "numpy_utils.hpp"

#include <pybind11/stl.h>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <string>

void init_analog(py::module_ & m) {

    py::class_<AnalogTimeSeries, std::shared_ptr<AnalogTimeSeries>>(m, "AnalogTimeSeries",
                                                                    "Continuous analog signal sampled over time")
            // --- Constructors ---
            .def(py::init<>(), "Construct an empty AnalogTimeSeries")
            .def(py::init<std::vector<float>, std::vector<TimeFrameIndex>>(),
                 py::arg("values"), py::arg("times"),
                 "Construct from vectors of values and time indices")
            .def(py::init<std::vector<float>, size_t>(),
                 py::arg("values"), py::arg("num_samples"),
                 "Construct from values vector with sample count")

            // --- Size ---
            .def("getNumSamples", &AnalogTimeSeries::getNumSamples,
                 "Number of samples in the series")
            .def("__len__", &AnalogTimeSeries::getNumSamples)

            // --- Zero-copy read (requires NumPy at runtime) ---
            .def_property_readonly("values", [](std::shared_ptr<AnalogTimeSeries> const & self) {
                auto span = self->getAnalogTimeSeries();
                return wt::python::span_to_numpy_readonly(span, py::cast(self)); }, "Read-only NumPy view of the analog data (zero-copy)")

            .def("getDataInRange", [](std::shared_ptr<AnalogTimeSeries> const & self, TimeFrameIndex start, TimeFrameIndex end) {
                auto span = self->getDataInTimeFrameIndexRange(start, end);
                return wt::python::span_to_numpy_readonly(span, py::cast(self)); }, py::arg("start"), py::arg("end"), "Read-only NumPy view of data in [start, end) (zero-copy)")

            // --- Copy-based accessors (always work, no NumPy) ---
            .def("toList", [](AnalogTimeSeries const & self) {
                auto span = self.getAnalogTimeSeries();
                return std::vector<float>(span.begin(), span.end()); }, "Copy all values to a Python list")

            .def("getTimeSeries", &AnalogTimeSeries::getTimeSeries, "Get all time indices as a list of TimeFrameIndex")

            // --- NumPy time indices ---
            .def_property_readonly("times", [](std::shared_ptr<AnalogTimeSeries> const & self) {
                auto const n = static_cast<py::ssize_t>(self->getNumSamples());
                auto arr = py::array_t<int64_t>(n);
                auto ptr = arr.mutable_data();
                auto time_vec = self->getTimeSeries();
                for (py::ssize_t i = 0; i < n; ++i) {
                    ptr[i] = time_vec[static_cast<size_t>(i)].getValue();
                }
                return arr; }, "NumPy array of time indices (int64)")

            // --- Paired time + value arrays ---
            .def("toTimeValueArrays", [](std::shared_ptr<AnalogTimeSeries> const & self) {
                auto const n = static_cast<py::ssize_t>(self->getNumSamples());

                auto times_arr = py::array_t<int64_t>(n);
                auto * t_ptr = times_arr.mutable_data();
                auto time_vec = self->getTimeSeries();
                for (py::ssize_t i = 0; i < n; ++i) {
                    t_ptr[i] = time_vec[static_cast<size_t>(i)].getValue();
                }

                auto values_span = self->getAnalogTimeSeries();
                auto values_arr = wt::python::span_to_numpy_readonly(values_span, py::cast(self));

                return py::make_tuple(times_arr, values_arr); }, "Return (times, values) as a tuple of NumPy arrays")

            .def("getAtTime", &AnalogTimeSeries::getAtTime, py::arg("time"), "Value at a specific time (None if not found)")

            // --- TimeFrame ---
            .def("setTimeFrame", &AnalogTimeSeries::setTimeFrame, py::arg("time_frame"))
            .def("getTimeFrame", &AnalogTimeSeries::getTimeFrame)

            // --- Storage info ---
            .def("__repr__", [](AnalogTimeSeries const & self) { return "AnalogTimeSeries(samples=" + std::to_string(self.getNumSamples()) + ")"; });
}
