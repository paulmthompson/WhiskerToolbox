#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/ClockTicks.hpp"

#include <cstddef>
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
            .def("addEvent", static_cast<void (DigitalEventSeries::*)(TimeFrameIndex)>(&DigitalEventSeries::addEvent),
                 py::arg("time"),
                 "Add an event at the specified time index")
            .def("addEvent", [](DigitalEventSeries & self, int64_t t) { self.addEvent(TimeFrameIndex(t)); }, py::arg("time_int"), "Add an event at the specified time (as integer)")
            .def("removeEvent", &DigitalEventSeries::removeEvent, py::arg("time"), "Remove an event at the specified time index")
            .def("removeEvent", [](DigitalEventSeries & self, int64_t t) { return self.removeEvent(TimeFrameIndex(t)); }, py::arg("time_int"), "Remove an event at the specified time (as integer)")
            .def("clear", &DigitalEventSeries::clear, "Remove all events")

            // --- Queries ---
            .def("size", &DigitalEventSeries::size, "Number of events")
            .def("__len__", &DigitalEventSeries::size)

            // --- Iteration / bulk access ---
            .def("toList", [](DigitalEventSeries const & self) {
                std::vector<ClockTicks> result;
                result.reserve(self.size());
                if (auto const time_frame = self.getTimeFrame()) {
                    for (std::size_t i = 0; i < self.size(); ++i) {
                        result.push_back(time_frame->getTimeAtIndex(self.getStoredEvent(i)));
                    }
                } else {
                    for (std::size_t i = 0; i < self.size(); ++i) {
                        result.push_back(ClockTicks(self.getStoredEvent(i).getValue()));
                    }
                }
                return result; }, "Get all event times as a list of ClockTicks")

            .def("toListWithIds", [](DigitalEventSeries const & self) {
                py::list result;
                if (auto const time_frame = self.getTimeFrame()) {
                    for (std::size_t i = 0; i < self.size(); ++i) {
                        result.append(py::make_tuple(
                                time_frame->getTimeAtIndex(self.getStoredEvent(i)),
                                self.getStoredEntityId(i)));
                    }
                } else {
                    for (std::size_t i = 0; i < self.size(); ++i) {
                        result.append(py::make_tuple(
                                ClockTicks(self.getStoredEvent(i).getValue()),
                                self.getStoredEntityId(i)));
                    }
                }
                return result; }, "Get all events as a list of (ClockTicks, EntityId) tuples")

            // --- TimeFrame ---
            .def("setTimeFrame", &DigitalEventSeries::setTimeFrame, py::arg("time_frame"))
            .def("getTimeFrame", &DigitalEventSeries::getTimeFrame)

            // --- Storage info ---
            .def("isView", &DigitalEventSeries::isView)
            .def("isLazy", &DigitalEventSeries::isLazy)

            .def("__repr__", [](DigitalEventSeries const & self) { return "DigitalEventSeries(events=" + std::to_string(self.size()) + ")"; });
}
