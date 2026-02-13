#include <pybind11/pybind11.h>
#include <pybind11/operators.h>

#include "TimeFrame/interval_data.hpp"

namespace py = pybind11;

void bind_interval(py::module_ &m) {
    py::class_<Interval>(m, "Interval", "A time interval with start and end points")
        .def(py::init<>(), "Default constructor")
        .def(py::init<int64_t, int64_t>(), 
             py::arg("start"), 
             py::arg("end"),
             "Construct interval with start and end times")
        .def_readwrite("start", &Interval::start, "Start time of the interval")
        .def_readwrite("end", &Interval::end, "End time of the interval")
        .def(py::self == py::self, "Check if two intervals are equal")
        .def(py::self < py::self, "Compare intervals by start time")
        .def("__repr__", [](const Interval &interval) {
            return "Interval(start=" + std::to_string(interval.start) + 
                   ", end=" + std::to_string(interval.end) + ")";
        });

    // Bind utility functions
    m.def("is_overlapping", 
          py::overload_cast<Interval const &, Interval const &>(&is_overlapping<int64_t>),
          py::arg("a"), py::arg("b"),
          "Check if two intervals overlap");
    
    m.def("is_contiguous", 
          py::overload_cast<Interval const &, Interval const &>(&is_contiguous<int64_t>),
          py::arg("a"), py::arg("b"),
          "Check if two intervals are contiguous (adjacent)");
    
    m.def("is_contained", 
          py::overload_cast<Interval const &, Interval const &>(&is_contained<int64_t>),
          py::arg("a"), py::arg("b"),
          "Check if interval b is contained within interval a");
    
    m.def("is_contained", 
          py::overload_cast<Interval const &, int64_t const>(&is_contained<int64_t>),
          py::arg("interval"), py::arg("time"),
          "Check if a time point is contained within an interval");
}
