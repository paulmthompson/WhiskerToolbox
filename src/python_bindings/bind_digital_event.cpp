#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"

#include <string>
#include <vector>

void init_digital_event(py::module_ & m) {

    py::class_<DigitalEventSeries, std::shared_ptr<DigitalEventSeries>>(
        m, "DigitalEventSeries",
        "Series of discrete time events")
        // --- Constructors ---
        .def(py::init<>(), "Construct an empty event series")
        .def(py::init<std::vector<TimeFrameIndex>>(), py::arg("events"),
             "Construct from a vector of time indices")

        // --- Mutation ---
        .def("addEvent", &DigitalEventSeries::addEvent,
             py::arg("time"),
             "Add an event at the specified time index")
        .def("addEvent",
            [](DigitalEventSeries & self, int64_t t) {
                self.addEvent(TimeFrameIndex(t));
            },
            py::arg("time_int"),
            "Add an event at the specified time (as integer)")
        .def("removeEvent", &DigitalEventSeries::removeEvent,
             py::arg("time"),
             "Remove an event at the specified time index")
        .def("removeEvent",
            [](DigitalEventSeries & self, int64_t t) {
                return self.removeEvent(TimeFrameIndex(t));
            },
            py::arg("time_int"),
            "Remove an event at the specified time (as integer)")
        .def("clear", &DigitalEventSeries::clear,
             "Remove all events")

        // --- Queries ---
        .def("size", &DigitalEventSeries::size,
             "Number of events")
        .def("__len__", &DigitalEventSeries::size)

        // --- Iteration / bulk access ---
        .def("toList",
            [](DigitalEventSeries const & self) {
                std::vector<TimeFrameIndex> result;
                for (auto const & ev : self.view()) {
                    result.push_back(ev.time());
                }
                return result;
            },
            "Get all event times as a list of TimeFrameIndex")

        .def("toListWithIds",
            [](DigitalEventSeries const & self) {
                py::list result;
                for (auto const & ev : self.view()) {
                    result.append(py::make_tuple(ev.time(), ev.id()));
                }
                return result;
            },
            "Get all events as a list of (TimeFrameIndex, EntityId) tuples")

        // --- TimeFrame ---
        .def("setTimeFrame", &DigitalEventSeries::setTimeFrame,
             py::arg("time_frame"))
        .def("getTimeFrame", &DigitalEventSeries::getTimeFrame)

        // --- Storage info ---
        .def("isView", &DigitalEventSeries::isView)
        .def("isLazy", &DigitalEventSeries::isLazy)

        .def("__repr__", [](DigitalEventSeries const & self) {
            return "DigitalEventSeries(events=" + std::to_string(self.size()) + ")";
        });
}
