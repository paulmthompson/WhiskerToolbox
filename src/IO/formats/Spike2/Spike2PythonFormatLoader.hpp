/**
 * @file Spike2PythonFormatLoader.hpp
 * @brief Optional Spike2 loader skeleton backed by embedded Python and CED SonPy.
 */

#ifndef SPIKE2_PYTHON_FORMAT_LOADER_HPP
#define SPIKE2_PYTHON_FORMAT_LOADER_HPP

#include "IO/core/IFormatLoader.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

class PythonEngine;

/**
 * @brief Loader options shared by the Spike2/SonPy loader skeleton.
 *
 * Phase 2 only validates SonPy availability and advertises these options. Later
 * phases will use them to drive the Python helper and C++ array conversion.
 */
struct Spike2SonPyLoaderOptions {
    std::string filepath;
    std::string preset{"native_channels"};
    bool include_raw_analog{true};
    bool include_event_channels{true};
    bool include_level_channels{true};
};

template<>
struct ParameterUIHints<Spike2SonPyLoaderOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to a CED Spike2 .smr or .smrx file";
        }
        if (auto * f = schema.field("preset")) {
            f->tooltip = "Import preset. Use native_channels for direct channel import or colleague_task_events for the threshold-derived workflow.";
            f->allowed_values = {"native_channels", "colleague_task_events"};
        }
        if (auto * f = schema.field("include_raw_analog")) {
            f->tooltip = "Import waveform and realwave channels as AnalogTimeSeries objects";
        }
        if (auto * f = schema.field("include_event_channels")) {
            f->tooltip = "Import event rise/fall channels as DigitalEventSeries objects";
        }
        if (auto * f = schema.field("include_level_channels")) {
            f->tooltip = "Import event-both/level channels as DigitalIntervalSeries objects";
        }
    }
};

/**
 * @brief Optional format loader for Spike2 files via Python SonPy.
 *
 * Phase 2 is a registration and dependency skeleton: it supports discovery,
 * batch loading metadata, and clear errors when SonPy or the extraction helper
 * is unavailable. Actual `.smr`/`.smrx` extraction is implemented in later
 * phases.
 */
class Spike2PythonFormatLoader final : public IFormatLoader {
public:
    explicit Spike2PythonFormatLoader(PythonEngine & engine);
    ~Spike2PythonFormatLoader() override = default;

    LoadResult load(std::string const & filepath,
                    DM_DataType dataType,
                    nlohmann::json const & config) const override;

    bool supportsBatchLoading(std::string const & format,
                              DM_DataType dataType) const override;

    BatchLoadResult loadBatch(std::string const & filepath,
                              DM_DataType dataType,
                              nlohmann::json const & config) const override;

    bool supportsFormat(std::string const & format, DM_DataType dataType) const override;

    std::string getLoaderName() const override;

    [[nodiscard]] std::vector<LoaderInfo> getLoaderInfo() const override;

private:
    [[nodiscard]] BatchLoadResult loadFromPythonPayload(std::string const & filepath,
                                                        DM_DataType dataType,
                                                        nlohmann::json const & config) const;

    PythonEngine & _engine;
};

#endif// SPIKE2_PYTHON_FORMAT_LOADER_HPP
