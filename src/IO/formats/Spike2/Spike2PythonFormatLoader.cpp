/**
 * @file Spike2PythonFormatLoader.cpp
 * @brief Optional Spike2/SonPy loader skeleton implementation.
 */

#include "Spike2PythonFormatLoader.hpp"

#include "PythonEngine.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/interval_data.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

constexpr char const * kSpike2Format = "spike2";
constexpr char const * kSpike2HelperModule = "whiskertoolbox_io.spike2_sonpy";

bool isSupportedSpike2DataType(DM_DataType dataType) {
    return dataType == DM_DataType::Analog ||
           dataType == DM_DataType::DigitalEvent ||
           dataType == DM_DataType::DigitalInterval;
}

std::string missingSonPyMessage(std::string const & filepath) {
    return "Spike2 import requires the optional Python package 'sonpy'. "
           "Install it into the active Neuralyzer Python environment with "
           "'python -m pip install sonpy numpy', then retry. File: " +
           filepath;
}

std::filesystem::path defaultPythonHelperPath() {
    auto const isHelperRoot = [](std::filesystem::path const & candidate) {
        return std::filesystem::is_directory(candidate / "whiskertoolbox_io");
    };

    auto const tryCandidate = [&](std::filesystem::path const & candidate) -> std::optional<std::filesystem::path> {
        if (isHelperRoot(candidate)) {
            return candidate;
        }
        return std::nullopt;
    };

    // 1. Next to the executable (build-tree POST_BUILD and macOS bundle layout).
    auto const exe_dir = PythonEngine::executableDirectory();
    if (auto const found = tryCandidate(exe_dir / "resources" / "python")) {
        return *found;
    }

    // 2. Installed layout: prefix/bin/exe with prefix/resources/python.
    if (auto const found = tryCandidate((exe_dir / ".." / "resources" / "python").lexically_normal())) {
        return *found;
    }

    // 3. Walk up from the executable directory (monorepo / build-tree fallbacks).
    for (auto current = exe_dir; current.has_parent_path() && current != current.parent_path();
         current = current.parent_path()) {
        if (auto const found = tryCandidate(current / "resources" / "python")) {
            return *found;
        }
    }

    // 4. Walk up from the current working directory (running from source tree).
    for (auto current = std::filesystem::current_path(); current.has_parent_path() && current != current.parent_path();
         current = current.parent_path()) {
        if (auto const found = tryCandidate(current / "resources" / "python")) {
            return *found;
        }
    }

    return exe_dir / "resources" / "python";
}

void prependSysPath(std::filesystem::path const & path) {
    py::module_ const sys = py::module_::import("sys");
    py::list const sys_path = sys.attr("path");
    auto const path_string = path.string();

    for (py::handle const item: sys_path) {
        if (py::str(item).cast<std::string>() == path_string) {
            return;
        }
    }

    sys_path.attr("insert")(0, path_string);
}

py::dict jsonToPyDict(nlohmann::json const & config) {
    py::object const json_module = py::module_::import("json");
    return json_module.attr("loads")(config.dump()).cast<py::dict>();
}

std::string pyStringValue(py::handle item, char const * key, std::string const & fallback) {
    auto const dict = py::reinterpret_borrow<py::dict>(item);
    py::object const value = dict.attr("get")(key, fallback);
    return value.cast<std::string>();
}

std::vector<TimeFrameIndex> toTimeFrameIndices(py::handle array_like) {
    auto sequence = py::reinterpret_borrow<py::sequence>(array_like);
    std::vector<TimeFrameIndex> out;
    out.reserve(static_cast<std::size_t>(py::len(sequence)));
    for (py::handle const item: sequence) {
        out.emplace_back(item.cast<int64_t>());
    }
    return out;
}

std::string dataTypeToConfigString(DM_DataType dataType) {
    switch (dataType) {
        case DM_DataType::Analog:
            return "analog";
        case DM_DataType::DigitalEvent:
            return "digital_event";
        case DM_DataType::DigitalInterval:
            return "digital_interval";
        default:
            return "analog";
    }
}

std::vector<float> toFloatVector(py::handle array_like) {
    auto sequence = py::reinterpret_borrow<py::sequence>(array_like);
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(py::len(sequence)));
    for (py::handle const item: sequence) {
        out.push_back(item.cast<float>());
    }
    return out;
}

void appendAnalogResults(py::handle analog_items, std::vector<LoadResult> & results) {
    for (py::handle const item: py::reinterpret_borrow<py::iterable>(analog_items)) {
        auto const dict = py::reinterpret_borrow<py::dict>(item);
        auto times = toTimeFrameIndices(dict["times"]);
        auto values = toFloatVector(dict["values"]);
        if (times.size() != values.size()) {
            throw std::runtime_error("Spike2 analog payload has mismatched times and values lengths");
        }

        auto series = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(times));
        results.emplace_back(LoadedDataVariant{std::move(series)}, pyStringValue(item, "name", "spike2_analog"));
    }
}

void appendEventResults(py::handle event_items, std::vector<LoadResult> & results) {
    for (py::handle const item: py::reinterpret_borrow<py::iterable>(event_items)) {
        auto const dict = py::reinterpret_borrow<py::dict>(item);
        auto times = toTimeFrameIndices(dict["times"]);

        auto series = std::make_shared<DigitalEventSeries>(std::move(times));
        results.emplace_back(LoadedDataVariant{std::move(series)}, pyStringValue(item, "name", "spike2_events"));
    }
}

void appendIntervalResults(py::handle interval_items, std::vector<LoadResult> & results) {
    for (py::handle const item: py::reinterpret_borrow<py::iterable>(interval_items)) {
        auto const dict = py::reinterpret_borrow<py::dict>(item);
        auto starts = toTimeFrameIndices(dict["starts"]);
        auto ends = toTimeFrameIndices(dict["ends"]);
        if (starts.size() != ends.size()) {
            throw std::runtime_error("Spike2 interval payload has mismatched starts and ends lengths");
        }

        std::vector<TimeFrameInterval> intervals;
        intervals.reserve(starts.size());
        for (std::size_t i = 0; i < starts.size(); ++i) {
            intervals.push_back(TimeFrameInterval{starts[i], ends[i]});
        }

        auto series = std::make_shared<DigitalIntervalSeries>(std::move(intervals));
        results.emplace_back(LoadedDataVariant{std::move(series)}, pyStringValue(item, "name", "spike2_intervals"));
    }
}

}// namespace

Spike2PythonFormatLoader::Spike2PythonFormatLoader(PythonEngine & engine)
    : _engine(engine) {}

LoadResult Spike2PythonFormatLoader::load(std::string const & filepath,
                                          DM_DataType dataType,
                                          nlohmann::json const & config) const {
    auto batch = loadBatch(filepath, dataType, config);
    if (batch.success && !batch.results.empty()) {
        return std::move(batch.results.front());
    }
    return {batch.error_message};
}

bool Spike2PythonFormatLoader::supportsBatchLoading(std::string const & format,
                                                    DM_DataType dataType) const {
    return supportsFormat(format, dataType);
}

BatchLoadResult Spike2PythonFormatLoader::loadBatch(std::string const & filepath,
                                                    DM_DataType dataType,
                                                    nlohmann::json const & config) const {
    if (!supportsFormat(kSpike2Format, dataType)) {
        return BatchLoadResult::error("Spike2 loader does not support requested data type: " +
                                      std::to_string(static_cast<int>(dataType)));
    }

    return loadFromPythonPayload(filepath, dataType, config);
}

bool Spike2PythonFormatLoader::supportsFormat(std::string const & format,
                                              DM_DataType dataType) const {
    return format == kSpike2Format && isSupportedSpike2DataType(dataType);
}

std::string Spike2PythonFormatLoader::getLoaderName() const {
    return "Spike2PythonFormatLoader (SonPy skeleton)";
}

std::vector<LoaderInfo> Spike2PythonFormatLoader::getLoaderInfo() const {
    auto schema = extractParameterSchema<Spike2SonPyLoaderOptions>();
    return std::vector<LoaderInfo>{
            {kSpike2Format, DM_DataType::Analog, "Spike2 waveform/realwave channels via SonPy", true, schema},
            {kSpike2Format, DM_DataType::DigitalEvent, "Spike2 event channels via SonPy", true, schema},
            {kSpike2Format, DM_DataType::DigitalInterval, "Spike2 level channels via SonPy", true, schema},
    };
}

BatchLoadResult Spike2PythonFormatLoader::loadFromPythonPayload(std::string const & filepath,
                                                                DM_DataType dataType,
                                                                nlohmann::json const & config) const {
    if (!_engine.isInitialized()) {
        return BatchLoadResult::error("Spike2 import requires the embedded Python interpreter, but Python is not initialized.");
    }

    try {
        py::gil_scoped_acquire const gil;

        auto const helper_path = config.contains("python_helper_path")
                                         ? std::filesystem::path(config["python_helper_path"].get<std::string>())
                                         : defaultPythonHelperPath();
        prependSysPath(helper_path);

        py::module_ const helper = py::module_::import(kSpike2HelperModule);
        nlohmann::json loader_config = config;
        if (!loader_config.contains("data_type")) {
            loader_config["data_type"] = dataTypeToConfigString(dataType);
        }
        py::dict const py_config = jsonToPyDict(loader_config);
        auto const payload = helper.attr("load_spike2")(filepath, py_config).cast<py::dict>();

        std::vector<LoadResult> results;
        if (dataType == DM_DataType::Analog) {
            appendAnalogResults(payload["analog"], results);
        } else if (dataType == DM_DataType::DigitalEvent) {
            appendEventResults(payload["events"], results);
        } else if (dataType == DM_DataType::DigitalInterval) {
            appendIntervalResults(payload["intervals"], results);
        }

        if (results.empty()) {
            return BatchLoadResult::error("Spike2 file contained no matching data objects for requested type. File: " + filepath);
        }

        return BatchLoadResult::fromVector(std::move(results));
    } catch (py::error_already_set const & e) {
        std::string const error_text = e.what();
        if (error_text.find("No module named 'sonpy'") != std::string::npos ||
            error_text.find("No module named \"sonpy\"") != std::string::npos) {
            return BatchLoadResult::error(missingSonPyMessage(filepath));
        }
        return BatchLoadResult::error("Spike2/SonPy Python error: " + error_text);
    } catch (std::exception const & e) {
        return BatchLoadResult::error("Spike2/SonPy payload conversion failed: " + std::string(e.what()));
    }
}
