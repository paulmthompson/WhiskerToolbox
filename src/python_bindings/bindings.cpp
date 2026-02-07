#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

namespace py = pybind11;

PYBIND11_MODULE(whiskertoolbox_python, m) {
    m.doc() = "WhiskerToolbox Python Bindings";

    // Bind TimeFrameIndex
    py::class_<TimeFrameIndex>(m, "TimeFrameIndex")
        .def(py::init<int64_t>())
        .def("getValue", &TimeFrameIndex::getValue)
        .def("__repr__", [](const TimeFrameIndex& t) {
            return "TimeFrameIndex(" + std::to_string(t.getValue()) + ")";
        })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self > py::self)
        .def(py::self <= py::self)
        .def(py::self >= py::self);

    py::implicitly_convertible<int64_t, TimeFrameIndex>();

    // Bind DigitalEventSeries
    py::class_<DigitalEventSeries, std::shared_ptr<DigitalEventSeries>>(m, "DigitalEventSeries")
        .def(py::init<>())
        .def(py::init<std::vector<TimeFrameIndex>>())
        .def("addEvent", [](DigitalEventSeries& self, int64_t t) {
            self.addEvent(TimeFrameIndex(t));
        }, "Add an event at the specified time index (as integer)")
        .def("addEvent", &DigitalEventSeries::addEvent, "Add an event at the specified time index")
        .def("removeEvent", [](DigitalEventSeries& self, int64_t t) {
            return self.removeEvent(TimeFrameIndex(t));
        }, "Remove an event at the specified time index (as integer)")
        .def("removeEvent", &DigitalEventSeries::removeEvent, "Remove an event at the specified time index")
        .def("size", &DigitalEventSeries::size, "Get the number of events")
        .def("clear", &DigitalEventSeries::clear, "Clear all events")
        .def("isView", &DigitalEventSeries::isView, "Check if the series is a view")
        .def("isLazy", &DigitalEventSeries::isLazy, "Check if the series is lazy");
}
