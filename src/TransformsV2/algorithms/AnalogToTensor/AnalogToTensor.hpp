/**
 * @file AnalogToTensor.hpp
 * @brief N-ary container transform that packs N AnalogTimeSeries into one TensorData
 *
 * Phase 1 of the multi-channel storage roadmap.
 * Takes a vector of AnalogTimeSeries channels (sharing the same TimeFrame)
 * and produces a 2D TensorData with shape (num_samples, num_channels).
 */

#ifndef WHISKERTOOLBOX_V2_ANALOG_TO_TENSOR_HPP
#define WHISKERTOOLBOX_V2_ANALOG_TO_TENSOR_HPP

#include <memory>
#include <string>
#include <vector>

class AnalogTimeSeries;
class TensorData;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for AnalogToTensor transform (reflect-cpp compatible)
 */
struct AnalogToTensorParams {
    /// Prefix to match AnalogTimeSeries keys in DataManager
    std::string channel_prefix;

    /// Selected DataManager keys (populated from prefix in UI)
    std::vector<std::string> channel_keys;
};

/**
 * @brief Pack multiple AnalogTimeSeries channels into a single 2D TensorData
 *
 * All channels must share the same TimeFrame (by pointer identity) and have
 * the same sample count. Uses viewValues() for mmap-safe iteration.
 *
 * @param channels  Non-empty vector of channels to pack
 * @param params    Parameters (channel_keys used as column names if sizes match)
 * @param ctx       Compute context for progress/cancellation/logging
 * @return 2D TensorData (time × channels), or nullptr on validation failure
 *
 * @pre channels must not be empty
 * @pre All channels must have identical sample counts
 * @pre All channels must share the same TimeFrame (pointer identity)
 */
[[nodiscard]] auto analogToTensor(
        std::vector<std::shared_ptr<AnalogTimeSeries const>> const & channels,
        AnalogToTensorParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_ANALOG_TO_TENSOR_HPP
