/**
 * @file SincInterpolation.hpp
 * @brief Band-limited sinc interpolation (Nyquist upsampling) for AnalogTimeSeries.
 *
 * Pure data container transform: takes N samples and produces (N-1)*factor+1
 * output samples using windowed-sinc interpolation. No TimeFrame awareness.
 */

#ifndef WHISKERTOOLBOX_V2_SINC_INTERPOLATION_HPP
#define WHISKERTOOLBOX_V2_SINC_INTERPOLATION_HPP

#include <memory>

class AnalogTimeSeries;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Window function type for the sinc kernel
 */
enum class SincWindowType {
    Lanczos,
    Hann,
    Blackman
};

/**
 * @brief Boundary handling mode for edge samples
 */
enum class BoundaryMode {
    SymmetricExtension,
    ZeroPad
};

/**
 * @brief Parameters for sinc interpolation upsampling
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Enum fields are automatically detected by AutoParamWidget and shown as combo boxes.
 *
 * Example JSON:
 * ```json
 * {
 *   "upsampling_factor": 4,
 *   "kernel_half_width": 8,
 *   "window_type": "Lanczos",
 *   "boundary_mode": "SymmetricExtension"
 * }
 * ```
 */
struct SincInterpolationParams {
    /// Integer upsampling factor (must be >= 1)
    int upsampling_factor = 1;

    /// Half-width of the sinc kernel in input samples
    int kernel_half_width = 8;

    /// Window function applied to the sinc kernel 
    SincWindowType window_type = SincWindowType::Lanczos;

    /// Boundary handling mode
    BoundaryMode boundary_mode = BoundaryMode::SymmetricExtension;

};

/**
 * @brief Apply band-limited sinc interpolation to upsample an AnalogTimeSeries
 *
 * For each output sample position m in [0, M-1] where M = (N-1)*factor + 1:
 *   1. Compute fractional input position: x = m / factor
 *   2. For each input sample n in [floor(x) - K, floor(x) + K]:
 *      accumulate sinc(x - n) * window(x - n) * input[n]
 *   3. Output sample = weighted sum
 *
 * Where K = kernel_half_width and sinc(t) = sin(pi*t) / (pi*t), sinc(0) = 1.
 *
 * @pre params.upsampling_factor >= 1
 *
 * @param input Input analog time series with N samples
 * @param params Interpolation parameters
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to a new AnalogTimeSeries with (N-1)*factor+1 samples,
 *         or nullptr if input is empty or params are invalid
 */
std::shared_ptr<AnalogTimeSeries> sincInterpolation(
        AnalogTimeSeries const & input,
        SincInterpolationParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_SINC_INTERPOLATION_HPP
