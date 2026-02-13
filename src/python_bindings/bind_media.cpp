#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DataManager/Media/Media_Data.hpp"

#include <string>

void init_media(py::module_ & m) {

    // MediaType enum
    py::enum_<MediaData::MediaType>(m, "MediaType")
        .value("Video", MediaData::MediaType::Video)
        .value("Images", MediaData::MediaType::Images)
        .value("HDF5", MediaData::MediaType::HDF5);

    // --- MediaData (abstract — read-only access from Python) ---
    py::class_<MediaData, std::shared_ptr<MediaData>>(m, "MediaData",
        "Media data container (video / images). Read-only from Python.")
        // No constructors — MediaData is abstract and obtained from DataManager

        // --- Properties ---
        .def("getHeight", &MediaData::getHeight)
        .def("getWidth", &MediaData::getWidth)
        .def("getImageSize", &MediaData::getImageSize)
        .def("getTotalFrameCount", &MediaData::getTotalFrameCount)
        .def("getFilename", &MediaData::getFilename)
        .def("getMediaType", &MediaData::getMediaType)

        // --- Bit depth ---
        .def("is8Bit", &MediaData::is8Bit)
        .def("is32Bit", &MediaData::is32Bit)

        // --- Frame data access (copy — per-frame) ---
        .def("getRawData8",
            [](MediaData & self, int frame) {
                return self.getRawData8(frame);
            },
            py::arg("frame"),
            "Get raw 8-bit data for a frame as a list of uint8")

        .def("getRawData32",
            [](MediaData & self, int frame) {
                return self.getRawData32(frame);
            },
            py::arg("frame"),
            "Get raw 32-bit data for a frame as a list of float")

        // --- TimeFrame ---
        .def("setTimeFrame", &MediaData::setTimeFrame, py::arg("time_frame"))
        .def("getTimeFrame", &MediaData::getTimeFrame)

        .def("__repr__", [](MediaData & self) {
            return "MediaData(" + std::to_string(self.getWidth()) + "x"
                   + std::to_string(self.getHeight()) + ", frames="
                   + std::to_string(self.getTotalFrameCount()) + ")";
        });
}
