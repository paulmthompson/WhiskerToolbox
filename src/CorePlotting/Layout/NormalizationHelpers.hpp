#ifndef COREPLOTTING_LAYOUT_NORMALIZATIONHELPERS_HPP
#define COREPLOTTING_LAYOUT_NORMALIZATIONHELPERS_HPP

#include "LayoutTransform.hpp"
#include <cmath>

namespace CorePlotting {
namespace NormalizationHelpers {

/**
 * @brief Z-score normalization: output = (value - mean) / std_dev
 * 
 * Centers data at 0, scales so ±1 = ±1 standard deviation.
 * Useful for comparing signals with different baselines and amplitudes.
 * 
 * @param mean Data mean value
 * @param std_dev Data standard deviation
 * @return Transform that applies z-score normalization
 */
[[nodiscard]] inline LayoutTransform forZScore(float mean, float std_dev) {
    float const safe_std = (std::abs(std_dev) > 1e-10f) ? std_dev : 1.0f;
    float const gain = 1.0f / safe_std;
    float const offset = -mean * gain;
    return { offset, gain };
}

/**
 * @brief Map [data_min, data_max] → [target_min, target_max]
 * 
 * Peak-to-peak normalization for fitting data into a known range.
 * Default maps to [-1, 1].
 * 
 * @param data_min Minimum value in data
 * @param data_max Maximum value in data
 * @param target_min Desired minimum output (default: -1)
 * @param target_max Desired maximum output (default: 1)
 * @return Transform that maps data range to target range
 */
[[nodiscard]] inline LayoutTransform forPeakToPeak(
        float data_min, float data_max,
        float target_min = -1.0f, float target_max = 1.0f) {
    float const data_range = data_max - data_min;
    float const target_range = target_max - target_min;
    float const safe_range = (std::abs(data_range) > 1e-10f) ? data_range : 1.0f;
    
    float const gain = target_range / safe_range;
    float const offset = target_min - data_min * gain;
    return { offset, gain };
}

/**
 * @brief ±N standard deviations from mean map to ±1
 * 
 * Common for neural data visualization where 3 std_devs → full display range.
 * Data at mean maps to 0, data at mean ± N*std_dev maps to ±1.
 * 
 * @param mean Data mean value
 * @param std_dev Data standard deviation
 * @param num_std_devs Number of standard deviations that map to ±1 (default: 3)
 * @return Transform that maps ±N std_devs to ±1
 */
[[nodiscard]] inline LayoutTransform forStdDevRange(
        float mean, float std_dev, float num_std_devs = 3.0f) {
    float const safe_std = (std::abs(std_dev) > 1e-10f) ? std_dev : 1.0f;
    float const gain = 1.0f / (num_std_devs * safe_std);
    float const offset = -mean * gain;
    return { offset, gain };
}

/**
 * @brief Map [0, 1] input range to arbitrary output range
 * 
 * For data that's already normalized to [0, 1].
 * 
 * @param target_min Desired minimum output (default: -1)
 * @param target_max Desired maximum output (default: 1)
 * @return Transform that maps [0,1] to [target_min, target_max]
 */
[[nodiscard]] inline LayoutTransform forUnitRange(
        float target_min = -1.0f, float target_max = 1.0f) {
    return forPeakToPeak(0.0f, 1.0f, target_min, target_max);
}

/**
 * @brief Percentile-based normalization
 * 
 * Maps [low_percentile_value, high_percentile_value] to [-1, 1].
 * Useful for robust normalization that ignores outliers.
 * 
 * @param low_value Value at low percentile (e.g., 5th percentile)
 * @param high_value Value at high percentile (e.g., 95th percentile)
 * @param target_min Desired minimum output (default: -1)
 * @param target_max Desired maximum output (default: 1)
 * @return Transform that maps percentile range to target range
 */
[[nodiscard]] inline LayoutTransform forPercentileRange(
        float low_value, float high_value,
        float target_min = -1.0f, float target_max = 1.0f) {
    return forPeakToPeak(low_value, high_value, target_min, target_max);
}

/**
 * @brief Center data at specified value, with optional gain
 * 
 * Output = (value - center) * gain
 * 
 * @param center Value that maps to 0
 * @param gain Scale factor (default: 1)
 * @return Transform that centers at specified value
 */
[[nodiscard]] inline LayoutTransform forCentered(float center, float gain = 1.0f) {
    return { -center * gain, gain };
}

/**
 * @brief Manual gain and offset specification
 * 
 * For full user control: output = value * gain + offset
 * 
 * @param gain Scale factor
 * @param offset Translation
 * @return Transform with specified gain and offset
 */
[[nodiscard]] inline LayoutTransform manual(float gain, float offset) {
    return { offset, gain };
}

} // namespace NormalizationHelpers
} // namespace CorePlotting

#endif // COREPLOTTING_LAYOUT_NORMALIZATIONHELPERS_HPP
