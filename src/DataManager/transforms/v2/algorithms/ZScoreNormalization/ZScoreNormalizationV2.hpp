#ifndef WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_V2_HPP
#define WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_V2_HPP

/**
 * @file ZScoreNormalizationV2.hpp
 * @brief Z-Score normalization using value store bindings (no preprocessing)
 *
 * This is the V2 implementation of Z-Score normalization that uses the
 * Pipeline Value Store pattern instead of the preprocessing registry.
 *
 * ## V2 Pattern
 *
 * Instead of using a preprocess() method to compute statistics in a first pass,
 * V2 transforms use:
 * 1. Pre-reductions to compute statistics (MeanValue, StdValue)
 * 2. Parameter bindings to inject computed values into transform params
 *
 * ## Example Pipeline JSON
 *
 * ```json
 * {
 *   "name": "ZScoreNormalizationPipeline",
 *   "pre_reductions": [
 *     {"reduction": "MeanValue", "output_key": "computed_mean"},
 *     {"reduction": "StdValue", "output_key": "computed_std"}
 *   ],
 *   "steps": [
 *     {
 *       "transform": "ZScoreNormalizeV2",
 *       "params": {
 *         "clamp_outliers": true,
 *         "outlier_threshold": 3.0,
 *         "epsilon": 1e-8
 *       },
 *       "param_bindings": {
 *         "mean": "computed_mean",
 *         "std_dev": "computed_std"
 *       }
 *     }
 *   ]
 * }
 * ```
 *
 * ## Comparison with V1
 *
 * | Aspect | V1 (Preprocessing) | V2 (Value Store) |
 * |--------|-------------------|------------------|
 * | Statistics | Computed in preprocess() | Computed via pre_reductions |
 * | Storage | rfl::Skip fields | PipelineValueStore keys |
 * | Configuration | Code changes | JSON bindings |
 * | Modularity | Requires PreprocessingRegistry | Fully decoupled |
 *
 * @see ZScoreNormalization.hpp for V1 implementation
 * @see PipelineValueStore.hpp for the value store
 * @see RangeReductionRegistry.hpp for available reductions
 */

#include "transforms/v2/extension/ParameterBinding.hpp"

#include <rfl.hpp>

#include <algorithm>
#include <cmath>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief V2 parameters for Z-Score normalization
 *
 * Unlike V1 ZScoreNormalizationParams, this struct has no rfl::Skip fields
 * or preprocess() method. Instead, mean and std_dev are regular fields
 * that get populated via parameter bindings from the PipelineValueStore.
 *
 * This eliminates the need for PreprocessingRegistry and enables
 * configuration purely through JSON pipeline definitions.
 */
struct ZScoreNormalizationParamsV2 {
    // ========== Bound from Value Store ==========

    /**
     * @brief Mean value for normalization
     *
     * This field is populated via param_bindings from a pre-reduction.
     * In the pipeline JSON, use: "param_bindings": {"mean": "computed_mean"}
     */
    float mean = 0.0f;

    /**
     * @brief Standard deviation for normalization
     *
     * This field is populated via param_bindings from a pre-reduction.
     * In the pipeline JSON, use: "param_bindings": {"std_dev": "computed_std"}
     */
    float std_dev = 1.0f;

    // ========== User-Specified Configuration ==========

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
};

/**
 * @brief Apply Z-Score normalization to a single value (V2)
 *
 * Transforms value to z-score: z = (x - mean) / std
 *
 * Unlike V1, this function uses params.mean and params.std_dev directly
 * since they are populated via bindings before transform execution.
 *
 * @param value Input value to normalize
 * @param params Parameters containing mean and std_dev
 * @return Normalized z-score value
 */
[[nodiscard]] inline float zScoreNormalizationV2(
        float value,
        ZScoreNormalizationParamsV2 const & params) {

    // Compute z-score
    float z_score = (value - params.mean) / (params.std_dev + params.epsilon);

    // Optionally clamp outliers
    if (params.clamp_outliers) {
        z_score = std::clamp(z_score, -params.outlier_threshold, params.outlier_threshold);
    }

    return z_score;
}

// ============================================================================
// Binding Applicator Registration
// ============================================================================

namespace {

/**
 * @brief Register binding applicator for ZScoreNormalizationParamsV2
 *
 * This enables the pipeline to inject values from PipelineValueStore
 * into the params struct at runtime.
 */
inline auto const register_zscore_v2_binding_applicator =
        RegisterBindingApplicator<ZScoreNormalizationParamsV2>();

}// namespace

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_ZSCORE_NORMALIZATION_V2_HPP
