/**
 * @file AnalogToTensor.cpp
 * @brief Implementation of AnalogToTensor N-ary container transform
 */

#include "AnalogToTensor.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <armadillo>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto analogToTensor(
        std::vector<std::shared_ptr<AnalogTimeSeries const>> const & channels,
        AnalogToTensorParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    if (channels.empty()) {
        ctx.logMessage("AnalogToTensor: channels vector is empty");
        return nullptr;
    }

    // All channels must be non-null
    for (std::size_t i = 0; i < channels.size(); ++i) {
        if (!channels[i]) {
            ctx.logMessage("AnalogToTensor: channel " + std::to_string(i) + " is null");
            return nullptr;
        }
    }

    auto const num_samples = channels[0]->getNumSamples();
    auto const num_channels = channels.size();
    auto const time_frame = channels[0]->getTimeFrame();
    auto const time_storage = channels[0]->getTimeStorage();

    if (num_samples == 0) {
        ctx.logMessage("AnalogToTensor: channels have zero samples");
        return nullptr;
    }

    // Validate all channels share the same TimeFrame and sample count
    for (std::size_t i = 1; i < num_channels; ++i) {
        if (channels[i]->getNumSamples() != num_samples) {
            ctx.logMessage("AnalogToTensor: channel " + std::to_string(i) +
                           " has " + std::to_string(channels[i]->getNumSamples()) +
                           " samples, expected " + std::to_string(num_samples));
            return nullptr;
        }
        if (channels[i]->getTimeFrame().get() != time_frame.get()) {
            ctx.logMessage("AnalogToTensor: channel " + std::to_string(i) +
                           " has a different TimeFrame");
            return nullptr;
        }
    }

    ctx.reportProgress(0);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // Build arma::fmat directly — each channel maps to one column.
    // Armadillo is column-major, so writing column-by-column is contiguous and cache-friendly.
    arma::fmat mat(static_cast<arma::uword>(num_samples),
                   static_cast<arma::uword>(num_channels));

    for (std::size_t c = 0; c < num_channels; ++c) {
        auto const & values = channels[c]->viewValues();
        assert(values.size() == num_samples);
        auto col = mat.col(static_cast<arma::uword>(c));
        std::copy(values.begin(), values.end(), col.begin());
    }

    ctx.reportProgress(70);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // Column names: use channel_keys if sizes match, otherwise generate
    std::vector<std::string> col_names;
    if (params.channel_keys.size() == num_channels) {
        col_names = params.channel_keys;
    } else {
        col_names.reserve(num_channels);
        for (std::size_t i = 0; i < num_channels; ++i) {
            col_names.push_back("ch" + std::to_string(i));
        }
    }

    auto tensor = TensorData::createTimeSeries2D(
            std::move(mat), time_storage, time_frame, col_names);

    ctx.reportProgress(100);

    return std::make_shared<TensorData>(std::move(tensor));
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
