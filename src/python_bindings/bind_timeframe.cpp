#include "bind_module.hpp"

#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <functional>
#include <string>

void init_timeframe(py::module_ & m) {

    // --- TimeFrameIndex (extended from Phase 1 with arithmetic / hash) ---
    py::class_<TimeFrameIndex>(m, "TimeFrameIndex",
        "Strongly-typed index into a TimeFrame")
        .def(py::init<int64_t>(), py::arg("value"))
        .def("getValue", &TimeFrameIndex::getValue,
             "Get the underlying int64 value")
        // Comparison
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self > py::self)
        .def(py::self <= py::self)
        .def(py::self >= py::self)
        // Arithmetic
        .def(py::self + py::self)
        .def(py::self - py::self)
        // Python special methods
        .def("__hash__", [](TimeFrameIndex const & t) {
            return std::hash<int64_t>{}(t.getValue());
        })
        .def("__int__", [](TimeFrameIndex const & t) {
            return t.getValue();
        })
        .def("__index__", [](TimeFrameIndex const & t) {
            return t.getValue();
        })
        .def("__repr__", [](TimeFrameIndex const & t) {
            return "TimeFrameIndex(" + std::to_string(t.getValue()) + ")";
        });

    py::implicitly_convertible<int64_t, TimeFrameIndex>();

    // --- TimeFrame ---
    py::class_<TimeFrame, std::shared_ptr<TimeFrame>>(m, "TimeFrame",
        "Temporal coordinate system mapping indices to absolute times")
        .def(py::init<>())
        .def(py::init<std::vector<int> const &>(), py::arg("times"),
             "Construct from a vector of absolute time values")
        .def("getTotalFrameCount", &TimeFrame::getTotalFrameCount,
             "Total number of frames in this time base")
        .def("getTimeAtIndex", &TimeFrame::getTimeAtIndex, py::arg("index"),
             "Get the absolute time value at a given index")
        .def("getIndexAtTime", &TimeFrame::getIndexAtTime,
             py::arg("time"), py::arg("preceding") = true,
             "Get the index closest to the given time")
        .def("__len__", &TimeFrame::getTotalFrameCount)
        .def("__repr__", [](TimeFrame const & tf) {
            return "TimeFrame(frames=" + std::to_string(tf.getTotalFrameCount()) + ")";
        });

    // --- Interval (int64_t start/end pair) ---
    py::class_<Interval>(m, "Interval",
        "Time interval with int64 start and end")
        .def(py::init([](int64_t start, int64_t end) {
            return Interval{start, end};
        }), py::arg("start"), py::arg("end"))
        .def_readwrite("start", &Interval::start)
        .def_readwrite("end", &Interval::end)
        .def("__repr__", [](Interval const & iv) {
            return "Interval(" + std::to_string(iv.start) + ", "
                               + std::to_string(iv.end) + ")";
        })
        .def(py::self == py::self);
}
