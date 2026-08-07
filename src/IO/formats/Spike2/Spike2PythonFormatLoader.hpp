/**
 * @file Spike2PythonFormatLoader.hpp
 * @brief Optional Spike2 loader skeleton backed by embedded Python and CED SonPy.
 */

#ifndef SPIKE2_PYTHON_FORMAT_LOADER_HPP
#define SPIKE2_PYTHON_FORMAT_LOADER_HPP

#include "IO/core/IFormatLoader.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <optional>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

class PythonEngine;

/**
 * @brief Analog preprocessing options for Spike2 ADC waveform channels.
 */
struct Spike2ProcessingOptions {
    bool invert{false};
    bool subtract_mean{false};
};

/**
 * @brief Loader options for Spike2/SonPy JSON entries.
 *
 * Each JSON ``data[]`` entry selects one channel and output type. Use ``channel``,
 * optional ``processing``, and optional ``threshold`` for ADC-derived outputs.
 */
struct Spike2SonPyLoaderOptions {
    std::string filepath;
    std::optional<int> channel;
    Spike2ProcessingOptions processing;
    std::optional<float> threshold;
};

template<>
struct ParameterUIHints<Spike2ProcessingOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("invert")) {
            f->tooltip = "Multiply waveform samples by -1 before output or thresholding";
        }
        if (auto * f = schema.field("subtract_mean")) {
            f->tooltip = "Subtract the channel mean from each waveform sample before output or thresholding";
        }
    }
};

template<>
struct ParameterUIHints<Spike2SonPyLoaderOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to a CED Spike2 .smr or .smrx file";
        }
        if (auto * f = schema.field("channel")) {
            f->tooltip = "SonPy channel index to read for this data entry";
        }
        if (auto * f = schema.field("processing")) {
            f->tooltip = "Optional invert and subtract-mean preprocessing applied to ADC waveforms";
        }
        if (auto * f = schema.field("threshold")) {
            f->tooltip = "Threshold for deriving digital events or intervals from an ADC waveform";
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
