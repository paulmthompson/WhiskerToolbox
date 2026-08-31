#ifndef COREMATH_SINC_INTERPOLATION_HPP
#define COREMATH_SINC_INTERPOLATION_HPP

/**
 * @file sinc_interpolation.hpp
 * @brief High-performance band-limited sinc interpolation, upsampling, and sub-sample alignment utilities.
 */

#include <span>
#include <utility>
#include <vector>

namespace CoreMath {

/**
 * @brief Window function type for the sinc kernel
 */
enum class SincWindowType {
    Lanczos,
    Hann,
    Blackman,
    Hamming
};

/**
 * @brief Boundary handling mode for edge samples
 */
enum class BoundaryMode {
    SymmetricExtension,
    ZeroPad,
    Replication
};

/**
 * @brief Reusable numerical class for band-limited Whittaker-Shannon sinc interpolation.
 */
class SincInterpolator {
public:
    /**
     * @brief Normalized sinc function: sinc(t) = sin(pi*t) / (pi*t), sinc(0) = 1.
     */
    [[nodiscard]] static double sinc(double t) noexcept;

    /**
     * @brief Evaluate window function at offset t for kernel half-width a.
     */
    [[nodiscard]] static double window(double t, int a, SincWindowType window_type) noexcept;

    /**
     * @brief Fetch sample with boundary handling.
     */
    [[nodiscard]] static float fetchSample(
            std::span<float const> data,
            int64_t index,
            BoundaryMode mode) noexcept;

    /**
     * @brief Interpolate discrete signal at an arbitrary continuous fractional index.
     *
     * @pre !data.empty()
     * @pre kernel_half_width >= 1
     * @param data Discrete input sample buffer
     * @param fractional_index Real-valued continuous sample position x
     * @param kernel_half_width Half-width of sinc kernel (default: 8)
     * @param window_type Window function (default: Lanczos)
     * @param mode Boundary handling mode (default: SymmetricExtension)
     * @return Interpolated continuous voltage value
     */
    [[nodiscard]] static float interpolateAt(
            std::span<float const> data,
            double fractional_index,
            int kernel_half_width = 8,
            SincWindowType window_type = SincWindowType::Lanczos,
            BoundaryMode mode = BoundaryMode::SymmetricExtension);

    /**
     * @brief Upsample discrete signal by an integer factor U.
     *
     * Produces an output buffer of (N - 1) * U + 1 samples.
     *
     * @pre !data.empty()
     * @pre upsampling_factor >= 1
     * @pre kernel_half_width >= 1
     * @param data Discrete input buffer
     * @param upsampling_factor Integer upsampling factor
     * @param kernel_half_width Sinc kernel half-width (default: 8)
     * @param window_type Window function (default: Lanczos)
     * @param mode Boundary handling mode (default: SymmetricExtension)
     * @return Vector of upsampled samples
     */
    [[nodiscard]] static std::vector<float> upsample(
            std::span<float const> data,
            int upsampling_factor,
            int kernel_half_width = 8,
            SincWindowType window_type = SincWindowType::Lanczos,
            BoundaryMode mode = BoundaryMode::SymmetricExtension);

    /**
     * @brief Shift discrete signal by a continuous fractional sample offset.
     *
     * For each index i, evaluates signal at continuous position (i - fractional_shift).
     *
     * @pre !data.empty()
     * @pre kernel_half_width >= 1
     * @param data Input sample buffer
     * @param fractional_shift Continuous shift offset in samples
     * @param kernel_half_width Sinc kernel half-width (default: 8)
     * @param window_type Window function (default: Hann)
     * @param mode Boundary handling mode (default: SymmetricExtension)
     * @return Vector of shifted samples (same size as input)
     */
    [[nodiscard]] static std::vector<float> shift(
            std::span<float const> data,
            double fractional_shift,
            int kernel_half_width = 8,
            SincWindowType window_type = SincWindowType::Hann,
            BoundaryMode mode = BoundaryMode::SymmetricExtension);

    /**
     * @brief Find sub-sample continuous extremum (peak or trough) with sub-sample precision.
     *
     * Performs fine sub-sample search using sinc evaluation around discrete extremum.
     *
     * @pre !data.empty()
     * @pre center_index >= 0 && center_index < data.size()
     * @param data Input sample buffer
     * @param center_index Discrete candidate index
     * @param find_minimum True to find trough, false to find peak
     * @param search_radius Discrete search radius around center_index (default: 2)
     * @param sub_divisions Sub-sample evaluation steps per sample (default: 16)
     * @param kernel_half_width Sinc kernel half-width (default: 8)
     * @param window_type Window function (default: Hann)
     * @param mode Boundary handling mode (default: SymmetricExtension)
     * @return Pair of {continuous_sample_position, extremum_value}
     */
    [[nodiscard]] static std::pair<double, float> findSubSampleExtremum(
            std::span<float const> data,
            int center_index,
            bool find_minimum = true,
            int search_radius = 2,
            int sub_divisions = 16,
            int kernel_half_width = 8,
            SincWindowType window_type = SincWindowType::Hann,
            BoundaryMode mode = BoundaryMode::SymmetricExtension);

    /**
     * @brief Compute continuous sub-sample V^2 energy centroid.
     *
     * Evaluates continuous V^2 distribution over window to compute sub-sample centroid.
     *
     * @pre !data.empty()
     * @pre center_index >= 0 && center_index < data.size()
     * @param data Input sample buffer
     * @param center_index Discrete candidate index
     * @param window_half_width Window half-width in samples (default: 6)
     * @param sub_divisions Sub-sample evaluation steps per sample (default: 16)
     * @param kernel_half_width Sinc kernel half-width (default: 8)
     * @param window_type Window function (default: Hann)
     * @param mode Boundary handling mode (default: SymmetricExtension)
     * @return Continuous sample position of energy centroid
     */
    [[nodiscard]] static double findSubSampleEnergyCentroid(
            std::span<float const> data,
            int center_index,
            int window_half_width = 6,
            int sub_divisions = 16,
            int kernel_half_width = 8,
            SincWindowType window_type = SincWindowType::Hann,
            BoundaryMode mode = BoundaryMode::SymmetricExtension);
};

}// namespace CoreMath

#endif// COREMATH_SINC_INTERPOLATION_HPP
