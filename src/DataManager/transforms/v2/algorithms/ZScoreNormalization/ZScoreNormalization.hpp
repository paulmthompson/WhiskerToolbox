#ifndef WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP
#define WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP

/**
 * @file ZScoreNormalization.hpp
 * @brief Z-Score normalization transform parameters and function
 *
 * @note For the recommended V2 pattern using parameter bindings instead of
 * cached statistics, see ZScoreNormalizationV2.hpp. The V2 pattern uses
 * pre-reductions and parameter bindings instead of a preprocess() method.
 *
 * @see ZScoreNormalizationV2.hpp for the V2 implementation
 */

#include <rfl.hpp>

#include <cmath>
#include <optional>
#include <stdexcept>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for Z-Score normalization transform
 *
 * Contains user-configurable options and cached statistics.
 * The statistics can be manually set via setStatistics() or the
 * V2 pattern using parameter bindings can be used instead.
 *
 * @note Consider using ZScoreNormalizationParamsV2 with parameter bindings
 * for a more composable approach where statistics come from pre-reductions.
 */
struct ZScoreNormalizationParams {
    // ========== User-Specified Configuration (Serialized) ==========

    /**
     * @brief Whether to clamp outliers beyond threshold
     */
    bool clamp_outliers = false;

    /**
     * @brief Number of standard deviations for outlier threshold
     *
     * Only used if clamp_outliers is true.
     * Values beyond mean ± (threshold * std) are clamped.
     */
    float outlier_threshold = 3.0f;

    /**
     * @brief Epsilon to avoid division by zero
     */
    float epsilon = 1e-8f;

    // ========== Computed State (NOT Serialized) ==========

    /**
     * @brief Cached mean value
     *
     * Set via setStatistics(). rfl::Skip prevents serialization.
     */
    rfl::Skip<std::optional<float>> cached_mean;

    /**
     * @brief Cached standard deviation
     *
     * Set via setStatistics(). rfl::Skip prevents serialization.
     */
    rfl::Skip<std::optional<float>> cached_std;

    // ========== Statistics Interface ==========

    /**
     * @brief Set the cached statistics for normalization
     *
     * @param mean The mean value to use
     * @param std_dev The standard deviation to use
     */
    void setStatistics(float mean, float std_dev) {
        cached_mean.value() = mean;
        cached_std.value() = std_dev;
    }

    /**
     * @brief Check if statistics have been set
     */
    [[nodiscard]] bool hasStatistics() const noexcept {
        return cached_mean.value().has_value() && cached_std.value().has_value();
    }

    /**
     * @brief Get cached mean (throws if not set)
     */
    [[nodiscard]] float getMean() const {
        if (!cached_mean.value().has_value()) {
            throw std::runtime_error("ZScoreNormalization: mean not set. Call setStatistics() first.");
        }
        return cached_mean.value().value();
    }

    /**
     * @brief Get cached standard deviation (throws if not set)
     */
    [[nodiscard]] float getStd() const {
        if (!cached_std.value().has_value()) {
            throw std::runtime_error("ZScoreNormalization: std not set. Call setStatistics() first.");
        }
        return cached_std.value().value();
    }
};

/**
 * @brief Apply Z-Score normalization to a single value
 *
 * Transforms value to z-score: z = (x - mean) / std
 *
 * @param value Input value to normalize
 * @param params Parameters containing cached statistics
 * @return Normalized z-score value
 * @throws std::runtime_error if statistics have not been set
 */
[[nodiscard]] inline float zScoreNormalization(float value, ZScoreNormalizationParams const & params) {
    float mean = params.getMean();
    float std = params.getStd();

    // Compute z-score
    float z_score = (value - mean) / (std + params.epsilon);

    // Optionally clamp outliers
    if (params.clamp_outliers) {
        float threshold = params.outlier_threshold;
        if (z_score > threshold) {
            z_score = threshold;
        } else if (z_score < -threshold) {
            z_score = -threshold;
        }
    }

    return z_score;
}

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP
