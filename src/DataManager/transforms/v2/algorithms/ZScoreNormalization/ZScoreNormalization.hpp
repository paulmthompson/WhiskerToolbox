#ifndef WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP
#define WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP

#include <rfl.hpp>

#include <cmath>
#include <optional>
#include <ranges>

#include "transforms/v2/core/TransformPipeline.hpp"

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for Z-Score normalization transform
 * 
 * This demonstrates the multi-pass preprocessing pattern:
 * - User-specified parameters are serialized to/from JSON
 * - Computed statistics (mean, std) are cached using rfl::Skip
 * - preprocess() method computes statistics in first pass over data
 * - Transform function uses cached statistics in second pass
 * 
 * This approach avoids intermediate container materialization while
 * enabling transforms that need global statistics.
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
     * @brief Cached mean value computed during preprocessing
     * 
     * rfl::Skip prevents this field from being serialized to/from JSON.
     */
    rfl::Skip<std::optional<float>> cached_mean;
    
    /**
     * @brief Cached standard deviation computed during preprocessing
     * 
     * rfl::Skip prevents this field from being serialized to/from JSON.
     */
    rfl::Skip<std::optional<float>> cached_std;
    
    /**
     * @brief Flag indicating whether preprocessing has been performed
     * 
     * Used to ensure idempotency - preprocessing only happens once.
     * rfl::Skip prevents this field from being serialized to/from JSON.
     */
    rfl::Skip<bool> preprocessed = false;
    
    // ========== Preprocessing Interface ==========
    
    /**
     * @brief Preprocess step: compute mean and standard deviation
     * 
     * This method is automatically detected and invoked by TransformPipeline
     * before the main execution loop. It performs a first pass over the data
     * to compute statistics, which are then used during transformation.
     * 
     * @tparam View Range type yielding (TimeFrameIndex, Value) pairs where Value is convertible to float
     * @param view_in Input data view
     */
    template<typename View>
        requires requires(View v) {
            std::ranges::begin(v);
            // Check that we can extract a float from pair.second in at least one way
            // AND that the extracted value is convertible to float
            requires (
                (requires { std::declval<decltype(*std::ranges::begin(v))>().second.data; } &&
                 std::convertible_to<decltype(std::declval<decltype(*std::ranges::begin(v))>().second.data), float>) ||
                (requires { std::declval<decltype(*std::ranges::begin(v))>().second.get().data; } &&
                 std::convertible_to<decltype(std::declval<decltype(*std::ranges::begin(v))>().second.get().data), float>) ||
                std::convertible_to<decltype(std::declval<decltype(*std::ranges::begin(v))>().second), float>
            );
        }
    void preprocess(View view_in) {
        if (preprocessed.value()) {
            return;  // Already preprocessed
        }
        
        // Online algorithm for mean and variance (Welford's method)
        // This is numerically stable and requires only one pass
        size_t count = 0;
        double mean = 0.0;
        double M2 = 0.0;  // Sum of squared differences from mean
        
        for (auto const& pair : view_in) {
            // Extract the float value from the pair
            // Different container types have different structures:
            // - AnalogTimeSeries: (TimeFrameIndex, float)
            // - RaggedTimeSeries: (TimeFrameIndex, reference_wrapper<DataEntry<float>>)
            float value;
            if constexpr (requires { pair.second.data; }) {
                // RaggedTimeSeries case: has .data member
                value = pair.second.data;
            } else if constexpr (requires { pair.second.get().data; }) {
                // RaggedTimeSeries with reference_wrapper case
                value = pair.second.get().data;
            } else {
                // AnalogTimeSeries case: second is directly the float
                value = static_cast<float>(pair.second);
            }
            
            count++;
            double delta = value - mean;
            mean += delta / count;
            double delta2 = value - mean;
            M2 += delta * delta2;
        }
        
        if (count == 0) {
            // No data - set defaults
            cached_mean = 0.0f;
            cached_std = 1.0f;
        } else if (count == 1) {
            // Single value - set std to 1 to avoid division by zero
            cached_mean = static_cast<float>(mean);
            cached_std = 1.0f;
        } else {
            // Normal case - compute variance and std
            double variance = M2 / (count - 1);  // Sample variance (Bessel's correction)
            cached_mean = static_cast<float>(mean);
            cached_std = static_cast<float>(std::sqrt(variance));
        }
        
        preprocessed = true;
    }
    
    /**
     * @brief Check if preprocessing has been performed
     * 
     * Used by TransformPipeline to ensure idempotency.
     */
    bool isPreprocessed() const {
        return preprocessed.value();
    }
    
    /**
     * @brief Get cached mean (throws if not preprocessed)
     */
    float getMean() const {
        if (!cached_mean.value().has_value()) {
            throw std::runtime_error("ZScoreNormalization: preprocess() must be called before getMean()");
        }
        return cached_mean.value().value();
    }
    
    /**
     * @brief Get cached standard deviation (throws if not preprocessed)
     */
    float getStd() const {
        if (!cached_std.value().has_value()) {
            throw std::runtime_error("ZScoreNormalization: preprocess() must be called before getStd()");
        }
        return cached_std.value().value();
    }
};

/**
 * @brief Apply Z-Score normalization to a single value
 * 
 * Transforms value to z-score: z = (x - mean) / std
 * 
 * This function assumes params.preprocess() has already been called
 * by the pipeline to populate cached_mean and cached_std.
 * 
 * @param value Input value to normalize
 * @param params Parameters containing cached statistics
 * @return Normalized z-score value
 */
inline float zScoreNormalization(float value, ZScoreNormalizationParams const& params) {
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

// ============================================================================
// Preprocessing Registration
// ============================================================================

namespace {
    // Register this parameter type for preprocessing
    // This tells the pipeline system that ZScoreNormalizationParams supports preprocess()
    inline auto const register_zscore_preprocessing = 
        RegisterPreprocessing<ZScoreNormalizationParams>();
}

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_HPP
