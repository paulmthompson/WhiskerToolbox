#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "CoreGeometry/lines.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "Observer/Observer_Data.hpp"

#include <string>
#include <vector>

void init_line(py::module_ & m) {

    // --- Line2D (value type — list of Point2D<float>) ---
    py::class_<Line2D>(m, "Line2D",
        "An ordered sequence of 2-D float points forming a polyline")
        .def(py::init<>())
        .def(py::init<std::vector<Point2D<float>>>(), py::arg("points"))
        .def(py::init<std::vector<float> const &, std::vector<float> const &>(),
             py::arg("x"), py::arg("y"),
             "Construct from separate x and y coordinate vectors")
        .def("size", &Line2D::size)
        .def("empty", &Line2D::empty)
        .def("__len__", &Line2D::size)
        .def("__getitem__", [](Line2D const & self, size_t i) {
            if (i >= self.size()) throw py::index_error();
            return self[i];
        })
        .def("__iter__", [](Line2D const & self) {
            return py::make_iterator(self.begin(), self.end());
        }, py::keep_alive<0, 1>())
        .def("push_back", &Line2D::push_back, py::arg("point"))
        .def("toList", [](Line2D const & self) {
            std::vector<Point2D<float>> pts;
            pts.reserve(self.size());
            for (auto const & p : self) pts.push_back(p);
            return pts;
        }, "Copy all points to a Python list")
        .def("__repr__", [](Line2D const & self) {
            return "Line2D(points=" + std::to_string(self.size()) + ")";
        });

    // --- LineData (ragged time series of Line2D) ---
    py::class_<LineData, std::shared_ptr<LineData>>(m, "LineData",
        "Time series of polylines (ragged — multiple lines per time step)")
        .def(py::init<>())

        // --- Add data ---
        .def("addAtTime",
            [](LineData & self, TimeFrameIndex t, Line2D const & line) {
                self.addAtTime(t, line, NotifyObservers::Yes);
            },
            py::arg("time"), py::arg("line"),
            "Add a line at the given time index")

        // --- Query ---
        .def("getTimeCount", &LineData::getTimeCount,
             "Number of distinct time steps with data")
        .def("getTotalEntryCount", &LineData::getTotalEntryCount,
             "Total number of line entries across all times")

        .def("getTimesWithData",
            [](LineData const & self) {
                std::vector<TimeFrameIndex> result;
                for (auto const & t : self.getTimesWithData()) {
                    result.push_back(t);
                }
                return result;
            },
            "Get all time indices that have line data")

        .def("getAtTime",
            [](LineData const & self, TimeFrameIndex t) {
                std::vector<Line2D> result;
                for (auto const & line : self.getAtTime(t)) {
                    result.push_back(line);
                }
                return result;
            },
            py::arg("time"),
            "Get all lines at the given time index")

        .def("getDataByEntityId",
            [](LineData const & self, EntityId id) -> py::object {
                auto opt = self.getDataByEntityId(id);
                if (opt) return py::cast(opt->get());
                return py::none();
            },
            py::arg("entity_id"),
            "Get the line associated with an entity ID (None if not found)")

        // --- Image size ---
        .def("getImageSize", &LineData::getImageSize)
        .def("setImageSize",
            static_cast<void (LineData::*)(ImageSize const &)>(&LineData::setImageSize),
            py::arg("size"))

        // --- TimeFrame ---
        .def("setTimeFrame", &LineData::setTimeFrame, py::arg("time_frame"))
        .def("getTimeFrame", &LineData::getTimeFrame)

        .def("__repr__", [](LineData const & self) {
            return "LineData(times=" + std::to_string(self.getTimeCount())
                   + ", entries=" + std::to_string(self.getTotalEntryCount()) + ")";
        });
}
