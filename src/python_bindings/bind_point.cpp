#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DataManager/Points/Point_Data.hpp"
#include "Observer/Observer_Data.hpp"

#include <string>
#include <vector>

void init_point(py::module_ & m) {

    // Point2D<float> is already registered in init_geometry

    // --- PointData (ragged time series of Point2D<float>) ---
    py::class_<PointData, std::shared_ptr<PointData>>(m, "PointData",
        "Time series of 2-D points (ragged — multiple points per time step)")
        .def(py::init<>())

        // --- Add data ---
        .def("addAtTime",
            [](PointData & self, TimeFrameIndex t, Point2D<float> const & pt) {
                self.addAtTime(t, pt, NotifyObservers::Yes);
            },
            py::arg("time"), py::arg("point"),
            "Add a point at the given time index")

        .def("addPointsAtTime",
            [](PointData & self, TimeFrameIndex t,
               std::vector<Point2D<float>> const & pts) {
                self.addAtTime(t, pts, NotifyObservers::Yes);
            },
            py::arg("time"), py::arg("points"),
            "Add multiple points at the given time index")

        // --- Query ---
        .def("getTimeCount", &PointData::getTimeCount)
        .def("getTotalEntryCount", &PointData::getTotalEntryCount)

        .def("getTimesWithData",
            [](PointData const & self) {
                std::vector<TimeFrameIndex> result;
                for (auto const & t : self.getTimesWithData()) {
                    result.push_back(t);
                }
                return result;
            },
            "Get all time indices that have point data")

        .def("getAtTime",
            [](PointData const & self, TimeFrameIndex t) {
                std::vector<Point2D<float>> result;
                for (auto const & pt : self.getAtTime(t)) {
                    result.push_back(pt);
                }
                return result;
            },
            py::arg("time"),
            "Get all points at the given time index")

        .def("getDataByEntityId",
            [](PointData const & self, EntityId id) -> py::object {
                auto opt = self.getDataByEntityId(id);
                if (opt) return py::cast(opt->get());
                return py::none();
            },
            py::arg("entity_id"))

        // --- Image size ---
        .def("getImageSize", &PointData::getImageSize)
        .def("setImageSize",
            static_cast<void (PointData::*)(ImageSize const &)>(&PointData::setImageSize),
            py::arg("size"))

        // --- TimeFrame ---
        .def("setTimeFrame", &PointData::setTimeFrame, py::arg("time_frame"))
        .def("getTimeFrame", &PointData::getTimeFrame)

        .def("__repr__", [](PointData const & self) {
            return "PointData(times=" + std::to_string(self.getTimeCount())
                   + ", entries=" + std::to_string(self.getTotalEntryCount()) + ")";
        });
}
