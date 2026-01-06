#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

namespace py = pybind11;

void bind_digital_interval_series(py::module_ &m) {
    py::class_<DigitalIntervalSeries, std::shared_ptr<DigitalIntervalSeries>>(
        m, "DigitalIntervalSeries", 
        "A series of digital intervals representing time periods when an event is active")
        
        // Constructors
        .def(py::init<>(), "Create an empty DigitalIntervalSeries")
        .def(py::init<std::vector<Interval>>(), 
             py::arg("intervals"),
             "Create DigitalIntervalSeries from a list of intervals")
        .def(py::init<std::vector<std::pair<float, float>> const &>(),
             py::arg("intervals"),
             "Create DigitalIntervalSeries from a list of (start, end) pairs")
        
        // Adding events
        .def("add_event", 
             py::overload_cast<Interval>(&DigitalIntervalSeries::addEvent),
             py::arg("interval"),
             "Add an interval to the series")
        
        // Getters
        .def("get_intervals", &DigitalIntervalSeries::getDigitalIntervalSeries,
             py::return_value_policy::reference_internal,
             "Get all intervals in the series")
        .def("is_event_at_time", &DigitalIntervalSeries::isEventAtTime,
             py::arg("time"),
             "Check if there is an event at the given time")
        .def("size", &DigitalIntervalSeries::size,
             "Get the number of intervals in the series")
        
        // Removing intervals
        .def("remove_interval", &DigitalIntervalSeries::removeInterval,
             py::arg("interval"),
             "Remove a specific interval from the series. Returns True if removed, False otherwise")
        .def("remove_intervals", &DigitalIntervalSeries::removeIntervals,
             py::arg("intervals"),
             "Remove multiple intervals from the series. Returns the number of intervals removed")
        
        // Create from boolean vector
        .def("create_intervals_from_bool", 
             &DigitalIntervalSeries::createIntervalsFromBool<bool>,
             py::arg("bool_vector"),
             "Create intervals from a boolean vector where True indicates the event is active")
        
        // String representation
        .def("__repr__", [](const DigitalIntervalSeries &series) {
            std::string repr = "DigitalIntervalSeries(size=" + std::to_string(series.size());
            if (series.size() > 0) {
                repr += ", intervals=[";
                const auto& intervals = series.getDigitalIntervalSeries();
                size_t show_count = std::min(size_t(3), intervals.size());
                for (size_t i = 0; i < show_count; ++i) {
                    if (i > 0) repr += ", ";
                    repr += "(" + std::to_string(intervals[i].start) + ", " + 
                           std::to_string(intervals[i].end) + ")";
                }
                if (intervals.size() > show_count) {
                    repr += ", ...";
                }
                repr += "]";
            }
            repr += ")";
            return repr;
        })
        
        // Length for Python len() function
        .def("__len__", &DigitalIntervalSeries::size);
}
