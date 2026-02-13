#include "PythonBridge.hpp"

#include "PythonEngine.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <pybind11/embed.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_set>
#include <vector>

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PythonBridge::PythonBridge(std::shared_ptr<DataManager> dm, PythonEngine & engine)
    : _dm(std::move(dm)),
      _engine(engine) {
    assert(_dm != nullptr);
}

// ---------------------------------------------------------------------------
// Exposure
// ---------------------------------------------------------------------------

void PythonBridge::exposeDataManager() {
    if (!_engine.isInitialized()) {
        return;
    }

    // Import the embedded module FIRST — this registers all pybind11 types
    // (DataManager, AnalogTimeSeries, etc.) so that py::cast() works.
    auto result = _engine.execute("import whiskertoolbox_python as wt");
    (void)result; // If import fails the user will see the error on next interaction

    // Now inject the live DataManager shared_ptr into the Python namespace as `dm`.
    // Because bind_datamanager.cpp registers DataManager with shared_ptr holder,
    // py::cast wraps the same shared_ptr — no copy, same pointer.
    _engine.inject("dm", py::cast(_dm));

    _dm_exposed = true;
}

bool PythonBridge::exposeData(std::string const & key, std::string const & python_name) {
    if (!_engine.isInitialized()) {
        return false;
    }

    // Ensure dm is available in the namespace
    exposeDataManager();

    // Use the already-bound DataManager.getData() Python method, which
    // handles variant dispatch and type-casting internally.  This avoids
    // needing complete type definitions for all variant alternatives in
    // this TU.
    auto r = _engine.execute(
        python_name + " = dm.getData('" + key + "')");
    if (!r.success) {
        return false;
    }

    // Verify it's not None (key missing or unbound type)
    auto check = _engine.execute(
        "_wt_expose_check = (" + python_name + " is not None)");
    if (!check.success) {
        return false;
    }

    try {
        auto val = _engine.globals()["_wt_expose_check"];
        bool ok = val.cast<bool>();
        // Clean up temp variable
        _engine.globals().attr("pop")("_wt_expose_check", py::none());
        return ok;
    } catch (...) {
        return false;
    }
}

bool PythonBridge::exposeTimeFrame(std::string const & time_key, std::string const & python_name) {
    if (!_engine.isInitialized()) {
        return false;
    }

    auto tf = _dm->getTime(TimeKey(time_key));
    if (!tf) {
        return false;
    }

    _engine.inject(python_name, py::cast(tf));
    return true;
}

// ---------------------------------------------------------------------------
// Data Import
// ---------------------------------------------------------------------------

namespace {

/// Try to cast a py::object to a shared_ptr<T> and register it in the DataManager.
/// Returns true if the object was of the requested type and was registered.
template<typename T>
bool tryImportAs(
    py::object const & obj,
    DataManager & dm,
    std::string const & key,
    std::string const & time_key) {
    try {
        auto ptr = obj.cast<std::shared_ptr<T>>();
        if (ptr) {
            dm.setData(key, ptr, TimeKey(time_key));
            return true;
        }
    } catch (py::cast_error &) {
        // Not this type — try next
    }
    return false;
}

} // anonymous namespace

std::vector<std::string> PythonBridge::importNewData(std::string const & default_time_key) {
    std::vector<std::string> imported;
    if (!_engine.isInitialized()) {
        return imported;
    }

    // Build a set of keys already in the DataManager so we can skip them
    auto existing_keys = _dm->getAllKeys();
    std::unordered_set<std::string> existing(existing_keys.begin(), existing_keys.end());

    // Also skip well-known namespace entries
    static std::unordered_set<std::string> const skip_names = {
        "dm", "wt", "__builtins__", "__name__", "__doc__",
        "_wt_stdout", "_wt_stderr", "__file__"
    };

    auto const & globals = _engine.globals();
    for (auto const & item : globals) {
        auto name = item.first.cast<std::string>();

        // Skip dunder, internal, and already-registered names
        if (name.size() >= 2 && name[0] == '_' && name[1] == '_') continue;
        if (skip_names.count(name)) continue;
        if (existing.count(name)) continue;

        py::object obj = py::reinterpret_borrow<py::object>(item.second);

        // Skip module objects
        try {
            if (py::isinstance(obj, py::module_::import("types").attr("ModuleType"))) {
                continue;
            }
        } catch (...) {}

        // Try to cast to each supported data type (order: most common first)
        bool registered =
            tryImportAs<AnalogTimeSeries>(obj, *_dm, name, default_time_key) ||
            tryImportAs<DigitalEventSeries>(obj, *_dm, name, default_time_key) ||
            tryImportAs<DigitalIntervalSeries>(obj, *_dm, name, default_time_key) ||
            tryImportAs<LineData>(obj, *_dm, name, default_time_key) ||
            tryImportAs<MaskData>(obj, *_dm, name, default_time_key) ||
            tryImportAs<PointData>(obj, *_dm, name, default_time_key) ||
            tryImportAs<TensorData>(obj, *_dm, name, default_time_key);

        if (registered) {
            imported.push_back(name);
        }
    }

    std::sort(imported.begin(), imported.end());
    return imported;
}

// ---------------------------------------------------------------------------
// Convenience Execution
// ---------------------------------------------------------------------------

PythonResult PythonBridge::execute(std::string const & code) {
    exposeDataManager();
    return _engine.execute(code);
}

PythonResult PythonBridge::executeFile(std::filesystem::path const & path) {
    exposeDataManager();
    return _engine.executeFile(path);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool PythonBridge::isDataManagerExposed() const {
    return _dm_exposed;
}

std::shared_ptr<DataManager> const & PythonBridge::dataManager() const {
    return _dm;
}

PythonEngine & PythonBridge::engine() {
    return _engine;
}

PythonEngine const & PythonBridge::engine() const {
    return _engine;
}
