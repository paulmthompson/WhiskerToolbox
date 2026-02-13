#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <string>
#include <vector>

void init_digital_interval(py::module_ & m) {

    py::class_<DigitalIntervalSeries, std::shared_ptr<DigitalIntervalSeries>>(
        m, "DigitalIntervalSeries",
        "Series of time intervals (start, end pairs)")
        // --- Constructors ---
        .def(py::init<>(), "Construct an empty interval series")
        .def(py::init<std::vector<Interval>>(), py::arg("intervals"),
             "Construct from a vector of Interval objects")

        // --- Mutation ---
        .def("addEvent",
            static_cast<void (DigitalIntervalSeries::*)(Interval)>(
                &DigitalIntervalSeries::addEvent),
            py::arg("interval"),
            "Add an interval")
        .def("addEvent",
            static_cast<void (DigitalIntervalSeries::*)(TimeFrameIndex, TimeFrameIndex)>(
                &DigitalIntervalSeries::addEvent),
            py::arg("start"), py::arg("end"),
            "Add an interval from start and end indices")
        .def("addInterval",
            [](DigitalIntervalSeries & self, int64_t start, int64_t end) {
                self.addEvent(TimeFrameIndex(start), TimeFrameIndex(end));
            },
            py::arg("start"), py::arg("end"),
            "Add an interval from integer start and end")
        .def("removeInterval", &DigitalIntervalSeries::removeInterval,
             py::arg("interval"),
             "Remove an interval (returns true if found)")
        .def("clear",
            [](DigitalIntervalSeries & self) {
                // Clear by reconstructing
                self = DigitalIntervalSeries();
            },
            "Remove all intervals")

        // --- Queries ---
        .def("size", &DigitalIntervalSeries::size,
             "Number of intervals")
        .def("__len__", &DigitalIntervalSeries::size)

        // --- Iteration / bulk access ---
        .def("toList",
            [](DigitalIntervalSeries const & self) {
                std::vector<Interval> result;
                for (auto const & iv : self.view()) {
                    result.push_back(iv.value());
                }
                return result;
            },
            "Get all intervals as a list of Interval objects")

        .def("toListWithIds",
            [](DigitalIntervalSeries const & self) {
                py::list result;
                for (auto const & iv : self.view()) {
                    result.append(py::make_tuple(iv.value(), iv.id()));
                }
                return result;
            },
            "Get all intervals as (Interval, EntityId) tuples")

        // --- TimeFrame ---
        .def("setTimeFrame", &DigitalIntervalSeries::setTimeFrame,
             py::arg("time_frame"))
        .def("getTimeFrame", &DigitalIntervalSeries::getTimeFrame)

        // --- Storage info ---
        .def("isView", &DigitalIntervalSeries::isView)
        .def("isLazy", &DigitalIntervalSeries::isLazy)

        .def("__repr__", [](DigitalIntervalSeries const & self) {
            return "DigitalIntervalSeries(intervals="
                   + std::to_string(self.size()) + ")";
        });
}
