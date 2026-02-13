#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "CoreGeometry/masks.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "Observer/Observer_Data.hpp"

#include <string>
#include <vector>

void init_mask(py::module_ & m) {

    // --- Mask2D (value type — list of Point2D<uint32_t>) ---
    py::class_<Mask2D>(m, "Mask2D",
        "A 2-D mask represented as a list of pixel coordinates")
        .def(py::init<>())
        .def(py::init<std::vector<Point2D<uint32_t>>>(), py::arg("points"))
        .def(py::init<std::vector<uint32_t> const &, std::vector<uint32_t> const &>(),
             py::arg("x"), py::arg("y"),
             "Construct from separate x and y uint32 coordinate vectors")
        .def("size", &Mask2D::size)
        .def("empty", &Mask2D::empty)
        .def("__len__", &Mask2D::size)
        .def("__getitem__", [](Mask2D const & self, size_t i) {
            if (i >= self.size()) throw py::index_error();
            return self[i];
        })
        .def("__iter__", [](Mask2D const & self) {
            return py::make_iterator(self.begin(), self.end());
        }, py::keep_alive<0, 1>())
        .def("push_back", &Mask2D::push_back, py::arg("point"))
        .def("points", &Mask2D::points,
             py::return_value_policy::reference_internal,
             "Const reference to the underlying point vector")
        .def("toList", [](Mask2D const & self) {
            return self.points();
        }, "Copy all points to a Python list")
        .def("__repr__", [](Mask2D const & self) {
            return "Mask2D(points=" + std::to_string(self.size()) + ")";
        });

    // --- MaskData (ragged time series of Mask2D) ---
    py::class_<MaskData, std::shared_ptr<MaskData>>(m, "MaskData",
        "Time series of masks (ragged — multiple masks per time step)")
        .def(py::init<>())

        // --- Add data ---
        .def("addAtTime",
            [](MaskData & self, TimeFrameIndex t, Mask2D const & mask) {
                self.addAtTime(t, mask, NotifyObservers::Yes);
            },
            py::arg("time"), py::arg("mask"),
            "Add a mask at the given time index")

        // --- Query ---
        .def("getTimeCount", &MaskData::getTimeCount)
        .def("getTotalEntryCount", &MaskData::getTotalEntryCount)

        .def("getTimesWithData",
            [](MaskData const & self) {
                std::vector<TimeFrameIndex> result;
                for (auto const & t : self.getTimesWithData()) {
                    result.push_back(t);
                }
                return result;
            },
            "Get all time indices that have mask data")

        .def("getAtTime",
            [](MaskData const & self, TimeFrameIndex t) {
                std::vector<Mask2D> result;
                for (auto const & mask : self.getAtTime(t)) {
                    result.push_back(mask);
                }
                return result;
            },
            py::arg("time"),
            "Get all masks at the given time index")

        .def("getDataByEntityId",
            [](MaskData const & self, EntityId id) -> py::object {
                auto opt = self.getDataByEntityId(id);
                if (opt) return py::cast(opt->get());
                return py::none();
            },
            py::arg("entity_id"))

        // --- Image size ---
        .def("getImageSize", &MaskData::getImageSize)
        .def("setImageSize",
            static_cast<void (MaskData::*)(ImageSize const &)>(&MaskData::setImageSize),
            py::arg("size"))

        // --- TimeFrame ---
        .def("setTimeFrame", &MaskData::setTimeFrame, py::arg("time_frame"))
        .def("getTimeFrame", &MaskData::getTimeFrame)

        .def("__repr__", [](MaskData const & self) {
            return "MaskData(times=" + std::to_string(self.getTimeCount())
                   + ", entries=" + std::to_string(self.getTotalEntryCount()) + ")";
        });
}
