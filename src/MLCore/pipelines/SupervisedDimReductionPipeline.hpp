/**
 * @file SupervisedDimReductionPipeline.hpp
 * @brief End-to-end supervised dimensionality reduction pipeline
 *
 * Combines feature conversion (from DimReductionPipeline) with label assembly
 * (from ClassificationPipeline) to train a supervised projection and produce
 * a reduced TensorData output.
 *
 * The pipeline trains on labeled observations only, then transforms ALL valid
 * rows (labeled + unlabeled) so the output tensor covers the complete dataset.
 * This enables scatter visualization with labeled and unlabeled points side-by-side.
 *
 * ## Usage
 * ```cpp
 * SupervisedDimReductionPipelineConfig config;
 * config.model_name = "Logit Projection";
 * config.feature_tensor_key = "encoder_features";
 * config.label_config = LabelFromIntervals{"Contact", "NoContact"};
 * config.label_interval_key = "contact_labels";
 * config.output_key = "logit_projection";
 *
 * auto result = runSupervisedDimReductionPipeline(dm, registry, config);
 * if (result.success) {
 *     // result.output_key contains a TensorData with columns
 *     // "Logit:Contact", "Logit:NoContact"
 * }
 * ```
 *
 * @see ClassificationPipeline for the analogous supervised classification pipeline
 * @see DimReductionPipeline for the analogous unsupervised dim reduction pipeline
 * @see LogitProjectionOperation for the primary concrete algorithm
 */

#ifndef MLCORE_SUPERVISEDDIMREDUCTIONPIPELINE_HPP
#define MLCORE_SUPERVISEDDIMREDUCTIONPIPELINE_HPP

#include "features/FeatureConverter.hpp"
#include "features/LabelAssembler.hpp"
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
 * @brief Full configuration for the supervised dimensionality reduction pipeline
 */
struct SupervisedDimReductionPipelineConfig {
    // -- Model --

    /**
     * @brief Name of the supervised dim reduction model (must be in the registry)
     *
     * Defaults to "Logit Projection". The model must have task type
     * MLTaskType::SupervisedDimensionalityReduction.
     */
    std::string model_name = "Logit Projection";

    /**
     * @brief Model-specific parameters (optional — nullptr uses defaults)
     */
    MLModelParametersBase const * model_params = nullptr;

    // -- Features --

    /**
     * @brief DataManager key of the TensorData to reduce
     *
     * Must be a 2D tensor whose rows represent observations.
     */
    std::string feature_tensor_key;

    /**
     * @brief Feature conversion configuration (NaN dropping, z-score normalization)
     */
    ConversionConfig conversion_config;

    // -- Labels --

    /**
     * @brief Label assembly configuration (which labeling mode to use)
     *
     * Choose from: LabelFromIntervals, LabelFromTimeEntityGroups,
     * LabelFromDataEntityGroups, or LabelFromEvents.
     */
    LabelAssemblyConfig label_config;

    /**
     * @brief DataManager key of the DigitalIntervalSeries for interval-based labels
     *
     * Required when label_config holds LabelFromIntervals.
     */
    std::string label_interval_key;

    /**
     * @brief DataManager key of the DigitalEventSeries for event-based labels
     *
     * Required when label_config holds LabelFromEvents.
     */
    std::string label_event_key;

    // -- Output --

    /**
     * @brief DataManager key to store the output TensorData under
     */
    std::string output_key = "logit_projection";

    /**
     * @brief Time key string for TimeFrame lookups
     *
     * Identifies which TimeFrame the feature tensor's rows are indexed by.
     */
    std::string time_key_str = "time";

    /**
     * @brief If true, skip DataManager writes and store output in the result
     *
     * Set this to true when running from a background thread to avoid
     * DataManager observer callbacks modifying Qt widgets from the wrong thread.
     */
    bool defer_dm_writes = false;
};

// ============================================================================
// Pipeline stages
// ============================================================================

/**
 * @brief Stages of the supervised dimensionality reduction pipeline
 */
enum class SupervisedDimReductionStage {
    ValidatingFeatures,
    ConvertingFeatures,
    AssemblingLabels,
    FittingAndTransforming,
    WritingOutput,
    Complete,
    Failed,
};

/// Human-readable description of a pipeline stage
[[nodiscard]] std::string toString(SupervisedDimReductionStage stage);

/// Progress callback type
using SupervisedDimReductionProgressCallback =
        std::function<void(SupervisedDimReductionStage, std::string const &)>;

// ============================================================================
// Pipeline result
// ============================================================================

/**
 * @brief Complete result of a supervised dimensionality reduction pipeline run
 */
struct SupervisedDimReductionPipelineResult {
    /// Whether the pipeline completed successfully
    bool success = false;

    /// Error message (empty on success)
    std::string error_message;

    /// Stage at which the pipeline failed
    SupervisedDimReductionStage failed_stage =
            SupervisedDimReductionStage::ValidatingFeatures;

    // -- Training info --

    /// Total observations after NaN dropping (labeled + unlabeled in output)
    std::size_t num_observations = 0;

    /// Observations used for training (labeled rows only)
    std::size_t num_training_observations = 0;

    /// Number of input features (tensor columns)
    std::size_t num_input_features = 0;

    /// Number of output dimensions (number of classes)
    std::size_t num_output_dimensions = 0;

    /// Rows dropped due to NaN/Inf in feature conversion
    std::size_t rows_dropped_nan = 0;

    /// Rows excluded from training because they had no label
    std::size_t unlabeled_count = 0;

    /// Number of classes discovered by label assembly
    std::size_t num_classes = 0;

    /// Class names (one per output dimension)
    std::vector<std::string> class_names;

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
 * @brief Run the full supervised dimensionality reduction pipeline
 *
 * Orchestrates:
 * 1. **Validate** — checks feature tensor exists and is valid
 * 2. **Convert** — TensorData → arma::mat (NaN dropping, optional z-score)
 * 3. **Assemble labels** — from intervals, entity groups, or events
 * 4. **Fit+Transform** — fits on labeled rows, transforms ALL valid rows
 * 5. **Write output** — creates a TensorData and stores in DataManager
 *
 * The output TensorData contains ALL rows that survived NaN conversion,
 * including unlabeled rows (which are still projected by the fitted model).
 * This lets the scatter plot show the complete dataset.
 *
 * Column names in the output use actual class names: "Logit:<ClassName>".
 *
 * @param dm        DataManager containing the feature tensor and label sources
 * @param registry  Model registry containing the supervised dim reduction factory
 * @param config    Full pipeline configuration
 * @param progress  Optional callback for progress reporting
 * @return Complete pipeline result
 */
[[nodiscard]] SupervisedDimReductionPipelineResult runSupervisedDimReductionPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        SupervisedDimReductionPipelineConfig const & config,
        SupervisedDimReductionProgressCallback const & progress = nullptr);

}// namespace MLCore

#endif// MLCORE_SUPERVISEDDIMREDUCTIONPIPELINE_HPP
