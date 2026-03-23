/**
 * @file DimReductionPipeline.hpp
 * @brief End-to-end dimensionality reduction pipeline
 *
 * Orchestrates the full dimensionality reduction workflow:
 *   validate tensor → convert to arma → fit+transform → write output TensorData
 *
 * Similar in structure to ClusteringPipeline, but produces a reduced TensorData
 * instead of cluster assignments.
 *
 * @see ClusteringPipeline for the analogous clustering pipeline
 * @see MLDimReductionOperation for the algorithm interface
 * @see PCAOperation for the concrete PCA implementation
 */

#ifndef MLCORE_DIMREDUCTIONPIPELINE_HPP
#define MLCORE_DIMREDUCTIONPIPELINE_HPP

#include "features/FeatureConverter.hpp"
#include "models/MLModelOperation.hpp"
#include "models/MLModelRegistry.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class TensorData;

namespace MLCore {

// ============================================================================
// Pipeline configuration
// ============================================================================

/**
 * @brief Output configuration for the dimensionality reduction pipeline
 */
struct DimReductionOutputConfig {
    /// DataManager key to store the output TensorData under
    std::string output_key = "reduced";

    /// Time key string for the clock that feature rows are indexed by
    std::string time_key_str = "time";
};

/**
 * @brief Full configuration for the dimensionality reduction pipeline
 */
struct DimReductionPipelineConfig {
    // -- Model --

    /// Name of the dim reduction model (must be registered in registry)
    std::string model_name;

    /// Model-specific parameters (optional — nullptr uses defaults)
    MLModelParametersBase const * model_params = nullptr;

    // -- Features --

    /// DataManager key of the TensorData to reduce
    std::string feature_tensor_key;

    // -- Feature conversion --

    /// Feature conversion config (NaN dropping, z-score normalization)
    ConversionConfig conversion_config;

    // -- Output --

    /// Output configuration
    DimReductionOutputConfig output_config;

    /// Time key string for time frame lookups
    std::string time_key_str = "time";

    /// If true, skip DataManager writes and store output in the result
    bool defer_dm_writes = false;
};

// ============================================================================
// Pipeline stages
// ============================================================================

/**
 * @brief Stages of the dimensionality reduction pipeline
 */
enum class DimReductionStage {
    ValidatingFeatures,
    ConvertingFeatures,
    FittingAndTransforming,
    WritingOutput,
    Complete,
    Failed,
};

/// Human-readable description of a pipeline stage
[[nodiscard]] std::string toString(DimReductionStage stage);

/// Progress callback type
using DimReductionProgressCallback =
        std::function<void(DimReductionStage, std::string const &)>;

// ============================================================================
// Pipeline result
// ============================================================================

/**
 * @brief Complete result of a dimensionality reduction pipeline run
 */
struct DimReductionPipelineResult {
    /// Whether the pipeline completed successfully
    bool success = false;

    /// Error message (empty on success)
    std::string error_message;

    /// Stage at which the pipeline failed
    DimReductionStage failed_stage = DimReductionStage::ValidatingFeatures;

    // -- Fitting info --

    /// Number of observations used
    std::size_t num_observations = 0;

    /// Number of input features
    std::size_t num_input_features = 0;

    /// Number of output components
    std::size_t num_output_components = 0;

    /// Number of rows dropped due to NaN/Inf
    std::size_t rows_dropped_nan = 0;

    /// Explained variance ratio per component (empty if not applicable)
    std::vector<double> explained_variance_ratio;

    // -- Output --

    /// DataManager key of the output TensorData (empty if defer_dm_writes)
    std::string output_key;

    /// Deferred TensorData for main-thread writing (when defer_dm_writes is true)
    std::shared_ptr<TensorData> deferred_output;

    /// Deferred output key (paired with deferred_output)
    std::string deferred_output_key;

    /// Deferred time key (paired with deferred_output)
    std::string deferred_time_key;

    // -- Model --

    /// The fitted model (for re-projection of new data)
    std::unique_ptr<MLModelOperation> fitted_model;
};

// ============================================================================
// Pipeline entry point
// ============================================================================

/**
 * @brief Run the full dimensionality reduction pipeline
 *
 * Orchestrates:
 * 1. **Validate** — checks feature tensor exists, is 2D
 * 2. **Convert** — TensorData → arma::mat (NaN dropping, optional z-score)
 * 3. **Fit+Transform** — fits the model and produces reduced matrix
 * 4. **Write output** — creates a new TensorData and stores in DataManager
 *
 * @param dm DataManager containing the feature tensor
 * @param registry Model registry containing the dim reduction model factory
 * @param config Full pipeline configuration
 * @param progress Optional callback for progress reporting
 * @return Complete pipeline result
 */
[[nodiscard]] DimReductionPipelineResult runDimReductionPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        DimReductionPipelineConfig const & config,
        DimReductionProgressCallback const & progress = nullptr);

}// namespace MLCore

#endif// MLCORE_DIMREDUCTIONPIPELINE_HPP
