#ifndef MLCORE_CLUSTERINGPIPELINE_HPP
#define MLCORE_CLUSTERINGPIPELINE_HPP

/**
 * @file ClusteringPipeline.hpp
 * @brief End-to-end unsupervised clustering pipeline
 *
 * Orchestrates the full ML clustering workflow in a single call:
 *   validate tensor → convert to arma → fit model → assign clusters →
 *   write output (putative groups, optional probability analog series)
 *
 * This pipeline composes the lower-level MLCore components (FeatureValidator,
 * FeatureConverter, MLModelOperation, PredictionWriter) into a cohesive
 * unsupervised workflow.
 *
 * ## Usage
 * ```cpp
 * ClusteringPipelineConfig config;
 * config.model_name = "K-Means";
 * config.feature_tensor_key = "pca_features";
 * config.time_key_str = "camera";
 * config.output_config.output_prefix = "Cluster:";
 *
 * auto result = runClusteringPipeline(dm, registry, config);
 * if (result.success) {
 *     // result.num_clusters, result.cluster_sizes, etc.
 * }
 * ```
 */

#include "features/FeatureConverter.hpp"
#include "models/MLModelOperation.hpp"
#include "models/MLModelRegistry.hpp"
#include "output/PredictionWriter.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class DataManager;

namespace MLCore {

// ============================================================================
// Pipeline configuration
// ============================================================================

/**
 * @brief Specifies where cluster assignment should be performed
 *
 * By default (assign_all_rows = true), cluster assignments are made on all
 * rows of the feature tensor used for fitting. Alternatively, a separate
 * tensor can be specified for assignment via assignment_tensor_key.
 */
struct ClusterAssignmentRegion {
    /**
     * @brief If true, assign clusters to all rows of the fitting tensor
     */
    bool assign_all_rows = true;

    /**
     * @brief DataManager key for a separate tensor to assign clusters to
     *
     * Used when assign_all_rows is false. Must have the same column
     * structure (features) as the fitting tensor. Rows can differ.
     * If empty and assign_all_rows is false, no cluster assignment output
     * is produced (fit-only mode).
     */
    std::string assignment_tensor_key;
};

/**
 * @brief Configuration for the clustering pipeline output
 */
struct ClusteringOutputConfig {
    /**
     * @brief Prefix for output group names and data keys
     *
     * Cluster groups are named "{output_prefix}{cluster_index}",
     * e.g., "Cluster:0", "Cluster:1".
     */
    std::string output_prefix = "Cluster:";

    /**
     * @brief Whether to write cluster assignments to putative entity groups
     *
     * Creates TimeEntity groups for each cluster, following the putative
     * group pattern.
     */
    bool write_to_putative_groups = true;

    /**
     * @brief Whether to write cluster assignments as DigitalIntervalSeries
     *
     * For each cluster, creates an interval series from contiguous runs
     * of that cluster assignment. Keys: "{output_prefix}{cluster_index}"
     */
    bool write_intervals = true;

    /**
     * @brief Whether to write soft cluster probabilities as AnalogTimeSeries
     *
     * Only effective if the model supports probability output (e.g., GMM).
     * Creates one AnalogTimeSeries per cluster.
     * Keys: "{output_prefix}prob_{cluster_index}"
     */
    bool write_probabilities = true;

    /**
     * @brief Time key string for the clock that feature rows are indexed by
     *
     * Used to register TimeEntities and set time keys on output data objects.
     */
    std::string time_key_str = "time";
};

/**
 * @brief Full configuration for the clustering pipeline
 */
struct ClusteringPipelineConfig {
    // -- Model --

    /**
     * @brief Name of the clustering model to use (must be registered in registry)
     *
     * E.g., "K-Means", "DBSCAN", "Gaussian Mixture Model"
     */
    std::string model_name;

    /**
     * @brief Model-specific parameters (optional — nullptr uses defaults)
     *
     * Must match the concrete parameter type expected by the chosen model.
     * E.g., KMeansParameters, DBSCANParameters, GMMParameters.
     */
    MLModelParametersBase const * model_params = nullptr;

    // -- Features --

    /**
     * @brief DataManager key of the TensorData to use as the feature matrix
     *
     * The tensor must be 2D. If rows are TimeFrameIndex-typed, time-based
     * output (intervals, groups) is supported. Ordinal rows are also accepted
     * but limit output options.
     */
    std::string feature_tensor_key;

    // -- Feature conversion --

    /**
     * @brief Feature conversion configuration
     *
     * Controls NaN dropping, z-score normalization, etc.
     */
    ConversionConfig conversion_config;

    // -- Assignment --

    /**
     * @brief Where to assign clusters after fitting
     */
    ClusterAssignmentRegion assignment_region;

    // -- Output --

    /**
     * @brief Configuration for writing clustering outputs
     */
    ClusteringOutputConfig output_config;

    /**
     * @brief Time key string for time frame lookups
     *
     * Identifies which TimeFrame the feature tensor's rows are indexed by.
     * Used for output writing (intervals, groups).
     */
    std::string time_key_str = "time";
};

// ============================================================================
// Pipeline progress callback
// ============================================================================

/**
 * @brief Stages of the clustering pipeline
 */
enum class ClusteringStage {
    ValidatingFeatures,
    ConvertingFeatures,
    Fitting,
    AssigningClusters,
    WritingOutput,
    Complete,
    Failed,
};

/**
 * @brief Human-readable description of a pipeline stage
 */
[[nodiscard]] std::string toString(ClusteringStage stage);

/**
 * @brief Optional callback for pipeline progress reporting
 *
 * Called at the start of each pipeline stage. The string parameter
 * provides a human-readable status message.
 */
using ClusteringProgressCallback = std::function<void(ClusteringStage, std::string const &)>;

// ============================================================================
// Pipeline result
// ============================================================================

/**
 * @brief Complete result of a clustering pipeline run
 */
struct ClusteringPipelineResult {
    /**
     * @brief Whether the pipeline completed successfully
     */
    bool success = false;

    /**
     * @brief Human-readable error message (empty if success)
     */
    std::string error_message;

    /**
     * @brief Stage at which the pipeline failed (only meaningful if !success)
     */
    ClusteringStage failed_stage = ClusteringStage::ValidatingFeatures;

    // -- Fitting info --

    /**
     * @brief Number of observations used for fitting (after NaN dropping)
     */
    std::size_t fitting_observations = 0;

    /**
     * @brief Number of features used
     */
    std::size_t num_features = 0;

    /**
     * @brief Number of clusters discovered or requested
     */
    std::size_t num_clusters = 0;

    /**
     * @brief Number of rows dropped due to NaN/Inf in feature conversion
     */
    std::size_t rows_dropped_nan = 0;

    /**
     * @brief Number of noise points (only meaningful for DBSCAN)
     *
     * Points not assigned to any cluster. 0 for non-density-based methods.
     */
    std::size_t noise_points = 0;

    // -- Assignment info --

    /**
     * @brief Number of observations assigned to clusters
     */
    std::size_t assignment_observations = 0;

    /**
     * @brief Per-cluster observation counts (index = cluster id, value = count)
     *
     * Size = num_clusters. Does not include noise points.
     */
    std::vector<std::size_t> cluster_sizes;

    /**
     * @brief Cluster names corresponding to output groups
     *
     * Generated as "{output_prefix}{cluster_index}".
     */
    std::vector<std::string> cluster_names;

    // -- Output --

    /**
     * @brief DataManager keys of created DigitalIntervalSeries (one per cluster)
     *
     * Empty if write_intervals was false.
     */
    std::vector<std::string> interval_keys;

    /**
     * @brief DataManager keys of created AnalogTimeSeries (one per cluster)
     *
     * Empty if write_probabilities was false or model doesn't support them.
     */
    std::vector<std::string> probability_keys;

    /**
     * @brief GroupIds of created putative groups (one per cluster)
     *
     * Empty if write_to_putative_groups was false.
     */
    std::vector<GroupId> putative_group_ids;

    // -- Model --

    /**
     * @brief The fitted model (ownership transferred to caller)
     *
     * Allows the caller to reuse the model for further cluster assignments
     * or save it to disk. nullptr if fitting failed.
     */
    std::unique_ptr<MLModelOperation> fitted_model;
};

// ============================================================================
// Pipeline entry point
// ============================================================================

/**
 * @brief Run the full unsupervised clustering pipeline
 *
 * Orchestrates:
 * 1. **Validate** — checks feature tensor exists and is valid
 * 2. **Convert** — TensorData → arma::mat (NaN dropping, optional z-score)
 * 3. **Fit** — fits the selected clustering model
 * 4. **Assign** — assigns clusters to fitting data or separate tensor
 * 5. **Write output** — writes cluster assignments as interval series,
 *    putative entity groups, and optionally probability analog series
 *
 * @param dm DataManager containing the feature tensor and output targets
 * @param registry Model registry containing the clustering model factory
 * @param config Full pipeline configuration
 * @param progress Optional callback for progress reporting
 * @return Complete pipeline result with cluster info, outputs, and fitted model
 */
[[nodiscard]] ClusteringPipelineResult runClusteringPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        ClusteringPipelineConfig const & config,
        ClusteringProgressCallback progress = nullptr);

}// namespace MLCore

#endif// MLCORE_CLUSTERINGPIPELINE_HPP
