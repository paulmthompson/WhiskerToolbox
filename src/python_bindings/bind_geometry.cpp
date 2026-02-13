#include "bind_module.hpp"

#include <pybind11/operators.h>

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <string>

void init_geometry(py::module_ & m) {

    // --- Point2D<float> (used by PointData, LineData) ---
    py::class_<Point2D<float>>(m, "Point2D",
        "2-D point with float coordinates")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &Point2D<float>::x)
        .def_readwrite("y", &Point2D<float>::y)
        .def("__repr__", [](Point2D<float> const & p) {
            return "Point2D(" + std::to_string(p.x) + ", "
                              + std::to_string(p.y) + ")";
        })
        .def(py::self == py::self);

    // --- Point2D<uint32_t> (used by MaskData) ---
    py::class_<Point2D<uint32_t>>(m, "Point2DU32",
        "2-D point with uint32 coordinates (masks)")
        .def(py::init<>())
        .def(py::init<uint32_t, uint32_t>(), py::arg("x"), py::arg("y"))
        .def_readwrite("x", &Point2D<uint32_t>::x)
        .def_readwrite("y", &Point2D<uint32_t>::y)
        .def("__repr__", [](Point2D<uint32_t> const & p) {
            return "Point2DU32(" + std::to_string(p.x) + ", "
                                 + std::to_string(p.y) + ")";
        })
        .def(py::self == py::self);

    // --- ImageSize ---
    py::class_<ImageSize>(m, "ImageSize",
        "Image dimensions (width, height)")
        .def(py::init<>())
        .def(py::init([](int w, int h) { return ImageSize{w, h}; }),
             py::arg("width"), py::arg("height"))
        .def_readwrite("width", &ImageSize::width)
        .def_readwrite("height", &ImageSize::height)
        .def("__repr__", [](ImageSize const & s) {
            return "ImageSize(" + std::to_string(s.width) + ", "
                                + std::to_string(s.height) + ")";
        })
        .def("__eq__", &ImageSize::operator==);
}
