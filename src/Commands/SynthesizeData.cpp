/**
 * @file SynthesizeData.cpp
 * @brief Implementation of the SynthesizeData command
 */

#include "SynthesizeData.hpp"

#include "Core/CommandContext.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <rfl/json.hpp>

#include <numeric>
#include <vector>

namespace {

/// @brief Extract the sample count from a generated DataTypeVariant
/// @return The number of samples, or 0 if the type is not recognized
std::size_t getSampleCount(DataTypeVariant const & data) {
    return std::visit([](auto const & ptr) -> std::size_t {
        using T = std::decay_t<decltype(*ptr)>;
        if constexpr (std::is_same_v<T, AnalogTimeSeries>) {
            return ptr->getNumSamples();
        } else if constexpr (std::is_same_v<T, DigitalEventSeries>) {
            return ptr->size();
        } else if constexpr (std::is_same_v<T, DigitalIntervalSeries>) {
            return ptr->size();
        } else if constexpr (std::is_same_v<T, MaskData>) {
            return ptr->getTimeCount();
        } else if constexpr (std::is_same_v<T, PointData>) {
            return ptr->getTimeCount();
        } else if constexpr (std::is_same_v<T, LineData>) {
            return ptr->getTimeCount();
        }
        return 0;
    },
                      data);
}

/// @brief Create a TimeFrame with identity mapping [0, 1, 2, ..., n-1]
std::shared_ptr<TimeFrame> createIdentityTimeFrame(std::size_t num_samples) {
    std::vector<int> times(num_samples);
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

}// namespace

namespace commands {

SynthesizeData::SynthesizeData(SynthesizeDataParams p)
    : _params(std::move(p)) {}

std::string SynthesizeData::commandName() const { return "SynthesizeData"; }

std::string SynthesizeData::toJson() const { return rfl::json::write(_params); }

CommandResult SynthesizeData::execute(CommandContext const & ctx) {
    auto & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();

    if (!registry.hasGenerator(_params.generator_name)) {
        return CommandResult::error(
                "Unknown generator: '" + _params.generator_name + "'");
    }

    auto const params_json = rfl::json::write(_params.parameters);

    auto const gen_ctx = WhiskerToolbox::DataSynthesizer::GeneratorContext{
            .data_manager = ctx.data_manager.get()};
    auto result = registry.generate(_params.generator_name, params_json, gen_ctx);
    if (!result.has_value()) {
        return CommandResult::error(
                "Generator '" + _params.generator_name + "' failed to produce data");
    }

    auto const time_key_str = _params.time_key.value_or(_params.output_key + "_time");
    auto const time_key = TimeKey(time_key_str);
    auto const num_samples = getSampleCount(*result);
    auto const mode = _params.time_frame_mode.value_or("create_new");

    if (mode == "use_existing") {
        auto existing_tf = ctx.data_manager->getTime(time_key);
        if (!existing_tf) {
            return CommandResult::error(
                    "TimeFrame '" + time_key_str + "' does not exist (mode=use_existing)");
        }
        if (num_samples > 0 && static_cast<std::size_t>(existing_tf->getTotalFrameCount()) < num_samples) {
            return CommandResult::error(
                    "TimeFrame '" + time_key_str + "' has " + std::to_string(existing_tf->getTotalFrameCount()) + " frames but data requires " + std::to_string(num_samples) + " (mode=use_existing)");
        }
    } else if (mode == "overwrite") {
        if (num_samples > 0) {
            ctx.data_manager->setTime(
                    time_key, createIdentityTimeFrame(num_samples), /*overwrite=*/true);
        }
    } else if (mode == "create_new") {
        if (num_samples > 0) {
            auto existing_tf = ctx.data_manager->getTime(time_key);
            if (existing_tf && existing_tf->getTotalFrameCount() > 0) {
                return CommandResult::error(
                        "TimeFrame '" + time_key_str + "' already exists (mode=create_new). "
                                                       "Use 'overwrite' or 'use_existing' instead.");
            }
            ctx.data_manager->setTime(
                    time_key, createIdentityTimeFrame(num_samples), /*overwrite=*/true);
        }
    } else {
        return CommandResult::error(
                "Unknown time_frame_mode: '" + mode + "'. Valid modes: create_new, use_existing, overwrite");
    }

    ctx.data_manager->setData(
            _params.output_key,
            std::move(*result),
            time_key);

    return CommandResult::ok({_params.output_key});
}

}// namespace commands
