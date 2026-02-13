#include "bind_module.hpp"
#include "numpy_utils.hpp"

#include <pybind11/stl.h>

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

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
        .def_property_readonly("values",
            [](std::shared_ptr<AnalogTimeSeries> const & self) {
                auto span = self->getAnalogTimeSeries();
                return wt::python::span_to_numpy_readonly(span, py::cast(self));
            },
            "Read-only NumPy view of the analog data (zero-copy)")

        .def("getDataInRange",
            [](std::shared_ptr<AnalogTimeSeries> const & self,
               TimeFrameIndex start, TimeFrameIndex end) {
                auto span = self->getDataInTimeFrameIndexRange(start, end);
                return wt::python::span_to_numpy_readonly(span, py::cast(self));
            },
            py::arg("start"), py::arg("end"),
            "Read-only NumPy view of data in [start, end) (zero-copy)")

        // --- Copy-based accessors (always work, no NumPy) ---
        .def("toList",
            [](AnalogTimeSeries const & self) {
                auto span = self.getAnalogTimeSeries();
                return std::vector<float>(span.begin(), span.end());
            },
            "Copy all values to a Python list")

        .def("getTimeSeries", &AnalogTimeSeries::getTimeSeries,
             "Get all time indices as a list of TimeFrameIndex")

        .def("getAtTime", &AnalogTimeSeries::getAtTime, py::arg("time"),
             "Value at a specific time (None if not found)")

        // --- TimeFrame ---
        .def("setTimeFrame", &AnalogTimeSeries::setTimeFrame,
             py::arg("time_frame"))
        .def("getTimeFrame", &AnalogTimeSeries::getTimeFrame)

        // --- Storage info ---
        .def("__repr__", [](AnalogTimeSeries const & self) {
            return "AnalogTimeSeries(samples="
                   + std::to_string(self.getNumSamples()) + ")";
        });
}
