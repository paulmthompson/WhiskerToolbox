#include "bind_module.hpp"

#include <pybind11/stl.h>

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <string>

void init_datamanager(py::module_ & m) {

    // --- DM_DataType enum ---
    py::enum_<DM_DataType>(m, "DataType",
        "Enumeration of data types stored in the DataManager")
        .value("Video", DM_DataType::Video)
        .value("Images", DM_DataType::Images)
        .value("Points", DM_DataType::Points)
        .value("Mask", DM_DataType::Mask)
        .value("Line", DM_DataType::Line)
        .value("Analog", DM_DataType::Analog)
        .value("DigitalEvent", DM_DataType::DigitalEvent)
        .value("DigitalInterval", DM_DataType::DigitalInterval)
        .value("Tensor", DM_DataType::Tensor)
        .value("Unknown", DM_DataType::Unknown);

    // --- DataManager ---
    py::class_<DataManager, std::shared_ptr<DataManager>>(m, "DataManager",
        "Central data registry managing all data objects and time frames")
        .def(py::init<>())

        // --- Type-agnostic data retrieval (auto-dispatched via variant) ---
        .def("getData",
            [](DataManager & dm, std::string const & key) -> py::object {
                auto var = dm.getDataVariant(key);
                if (!var) return py::none();
                return std::visit([](auto const & ptr) -> py::object {
                    if (!ptr) return py::none();
                    try {
                        return py::cast(ptr);
                    } catch (py::cast_error &) {
                        return py::none(); // Unbound type (e.g. RaggedAnalogTimeSeries)
                    }
                }, *var);
            },
            py::arg("key"),
            "Get data by key (returns the typed Python wrapper, or None)")

        // --- Type-specific setData overloads ---
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<AnalogTimeSeries> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<DigitalEventSeries> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<DigitalIntervalSeries> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<LineData> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<MaskData> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<PointData> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))
        .def("setData",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<TensorData> data, std::string const & time_key) {
                dm.setData(key, data, TimeKey(time_key));
            },
            py::arg("key"), py::arg("data"), py::arg("time_key"))

        // --- Delete ---
        .def("deleteData", &DataManager::deleteData, py::arg("key"),
             "Delete data by key (returns True if found)")

        // --- Key queries ---
        .def("getAllKeys", &DataManager::getAllKeys,
             "Get all registered data keys")
        .def("getType", &DataManager::getType, py::arg("key"),
             "Get the data type enum for a key")

        // --- Type-specific key lists ---
        .def("getAnalogKeys",
            &DataManager::getKeys<AnalogTimeSeries>,
            "Get all keys for AnalogTimeSeries data")
        .def("getDigitalEventKeys",
            &DataManager::getKeys<DigitalEventSeries>,
            "Get all keys for DigitalEventSeries data")
        .def("getDigitalIntervalKeys",
            &DataManager::getKeys<DigitalIntervalSeries>,
            "Get all keys for DigitalIntervalSeries data")
        .def("getLineKeys",
            &DataManager::getKeys<LineData>,
            "Get all keys for LineData")
        .def("getMaskKeys",
            &DataManager::getKeys<MaskData>,
            "Get all keys for MaskData")
        .def("getPointKeys",
            &DataManager::getKeys<PointData>,
            "Get all keys for PointData")
        .def("getTensorKeys",
            &DataManager::getKeys<TensorData>,
            "Get all keys for TensorData")
        .def("getMediaKeys",
            &DataManager::getKeys<MediaData>,
            "Get all keys for MediaData")

        // --- TimeFrame management ---
        .def("setTime",
            [](DataManager & dm, std::string const & key,
               std::shared_ptr<TimeFrame> tf, bool overwrite) {
                return dm.setTime(TimeKey(key), tf, overwrite);
            },
            py::arg("key"), py::arg("time_frame"),
            py::arg("overwrite") = false,
            "Register a TimeFrame under a key")
        .def("getTime",
            [](DataManager & dm, std::string const & key) {
                return dm.getTime(TimeKey(key));
            },
            py::arg("key"),
            "Get the TimeFrame for a key")
        .def("getTimeKey",
            [](DataManager & dm, std::string const & data_key) {
                return dm.getTimeKey(data_key).str();
            },
            py::arg("data_key"),
            "Get the time key associated with a data key")
        .def("getTimeFrameKeys",
            [](DataManager & dm) {
                auto keys = dm.getTimeFrameKeys();
                std::vector<std::string> result;
                result.reserve(keys.size());
                for (auto const & k : keys) result.push_back(k.str());
                return result;
            },
            "Get all registered TimeFrame keys")

        // --- Entity system ---
        .def("getEntityGroupManager", &DataManager::getEntityGroupManager,
             py::return_value_policy::reference_internal,
             "Access the EntityGroupManager (lifetime tied to DataManager)")

        // --- Observers ---
        .def("addObserver",
            [](DataManager & dm, py::function callback) {
                return dm.addObserver([callback]() { callback(); });
            },
            py::arg("callback"),
            "Add a Python callback for DataManager state changes")
        .def("removeObserver", &DataManager::removeObserver,
             py::arg("observer_id"))

        // --- Other ---
        .def("reset", &DataManager::reset,
             "Clear all data and reset to initial state")
        .def("setOutputPath", &DataManager::setOutputPath, py::arg("path"))
        .def("getOutputPath", &DataManager::getOutputPath)

        .def("__repr__", [](DataManager & dm) {
            return "DataManager(keys=" + std::to_string(dm.getAllKeys().size()) + ")";
        });
}
