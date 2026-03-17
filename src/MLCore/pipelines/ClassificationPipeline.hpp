#ifndef MLCORE_CLASSIFICATIONPIPELINE_HPP
#define MLCORE_CLASSIFICATIONPIPELINE_HPP

/**
 * @file ClassificationPipeline.hpp
 * @brief End-to-end supervised classification pipeline
 *
 * Orchestrates the full ML classification workflow in a single call:
 *   validate tensor → convert to arma → assemble labels → balance →
 *   train → predict on region → compute metrics → write putative group output
 *
 * This pipeline composes the lower-level MLCore components (FeatureValidator,
 * FeatureConverter, LabelAssembler, ClassBalancing, MLModelOperation,
 * ClassificationMetrics, PredictionWriter) into a cohesive workflow.
 *
 * ## Usage
 * ```cpp
 * ClassificationPipelineConfig config;
 * config.model_name = "Random Forest";
 * config.feature_tensor_key = "ml_features";
 * config.label_config = LabelFromIntervals{"Inside", "Outside"};
 * config.label_interval_key = "contact_labels";
 * config.time_key_str = "camera";
 * config.output_config.output_prefix = "Predicted:";
 *
 * auto result = runClassificationPipeline(dm, config);
 * if (result.success) {
 *     // result.train_metrics, result.prediction_result, etc.
 * }
 * ```
 */

#include "features/FeatureConverter.hpp"
#include "features/LabelAssembler.hpp"
#include "metrics/ClassificationMetrics.hpp"
#include "models/MLModelOperation.hpp"
#include "models/MLModelRegistry.hpp"
#include "output/PredictionWriter.hpp"
#include "preprocessing/ClassBalancing.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

class DataManager;

namespace MLCore {

// ============================================================================
// Pipeline configuration
// ============================================================================

/**
 * @brief Specifies where prediction should be performed
 *
 * If predict_all_rows is true, predictions are made on all feature rows
 * (the same tensor used for training — useful for full-dataset scoring).
 *
 * Otherwise, a separate prediction tensor can be specified via
 * prediction_tensor_key. If neither is set, no prediction is performed.
 */
struct PredictionRegionConfig {
    /**
     * @brief If true, predict on all rows of the training feature tensor
     */
    bool predict_all_rows = true;

    /**
     * @brief DataManager key for a separate prediction feature tensor
     *
     * Used when predict_all_rows is false. Must have the same column
     * structure as the training tensor. Rows can be different.
     * If empty and predict_all_rows is false, no prediction is performed.
     */
    std::string prediction_tensor_key;
};

/**
 * @brief Full configuration for the classification pipeline
 */
struct ClassificationPipelineConfig {
    // -- Model --

    /**
     * @brief Name of the model to use (must be registered in the registry)
     *
     * E.g., "Random Forest", "Naive Bayes", "Logistic Regression"
     */
    std::string model_name;

    /**
     * @brief Model-specific parameters (optional — nullptr uses defaults)
     *
     * Must match the concrete parameter type expected by the chosen model.
     */
    MLModelParametersBase const * model_params = nullptr;

    // -- Features --

    /**
     * @brief DataManager key of the TensorData to use as the feature matrix
     *
     * The tensor must be 2D with time-indexed rows (RowType::TimeFrameIndex).
     */
    std::string feature_tensor_key;

    // -- Labels --

    /**
     * @brief Label assembly configuration (which labeling mode to use)
     */
    LabelAssemblyConfig label_config;

    /**
     * @brief DataManager key of the DigitalIntervalSeries for interval-based labels
     *
     * Required when label_config holds LabelFromIntervals.
     * Ignored for other labeling modes.
     */
    std::string label_interval_key;

    /**
     * @brief DataManager key of the DigitalEventSeries for event-based labels
     *
     * Required when label_config holds LabelFromEvents.
     * Ignored for other labeling modes.
     */
    std::string label_event_key;

    // -- Feature conversion --

    /**
     * @brief Feature conversion configuration
     *
     * Controls NaN dropping, z-score normalization, etc.
     */
    ConversionConfig conversion_config;

    // -- Class balancing --

    /**
     * @brief Whether to apply class balancing before training
     */
    bool balance_classes = false;

    /**
     * @brief Class balancing configuration (used if balance_classes is true)
     */
    BalancingConfig balancing_config;

    // -- Prediction --

    /**
     * @brief Where to make predictions after training
     */
    PredictionRegionConfig prediction_region;

    // -- Output --

    /**
     * @brief Configuration for writing prediction outputs
     */
    PredictionWriterConfig output_config;

    /**
     * @brief Time key string for time frame lookups
     *
     * Identifies which TimeFrame the feature tensor's rows are indexed by.
     * Used for label assembly (interval lookups) and output writing.
     */
    std::string time_key_str = "time";
};

// ============================================================================
// Pipeline progress callback
// ============================================================================

/**
 * @brief Stages of the classification pipeline
 */
enum class ClassificationStage {
    ValidatingFeatures,
    ConvertingFeatures,
    AssemblingLabels,
    BalancingClasses,
    SegmentingSequences,
    Training,
    Predicting,
    ComputingMetrics,
    WritingOutput,
    Complete,
    Failed,
};

/**
 * @brief Human-readable description of a pipeline stage
 */
[[nodiscard]] std::string toString(ClassificationStage stage);

/**
 * @brief Optional callback for pipeline progress reporting
 *
 * Called at the start of each pipeline stage. The string parameter
 * provides a human-readable status message.
 */
using PipelineProgressCallback = std::function<void(ClassificationStage, std::string const &)>;

// ============================================================================
// Pipeline result
// ============================================================================

/**
 * @brief Complete result of a classification pipeline run
 */
struct ClassificationPipelineResult {
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
    ClassificationStage failed_stage = ClassificationStage::ValidatingFeatures;

    // -- Training info --

    /**
     * @brief Number of training observations (after NaN dropping and balancing)
     */
    std::size_t training_observations = 0;

    /**
     * @brief Number of features used
     */
    std::size_t num_features = 0;

    /**
     * @brief Number of classes
     */
    std::size_t num_classes = 0;

    /**
     * @brief Class names from label assembly
     */
    std::vector<std::string> class_names;

    /**
     * @brief Number of rows dropped due to NaN/Inf in feature conversion
     */
    std::size_t rows_dropped_nan = 0;

    /**
     * @brief Whether class balancing was applied
     */
    bool was_balanced = false;

    // -- Metrics (training set, or prediction set if different) --

    /**
     * @brief Training-set metrics (evaluated on training data, may be overfit)
     *
     * Present when the model was successfully trained and predict_all_rows
     * was used on the training tensor.
     */
    std::optional<BinaryClassificationMetrics> binary_train_metrics;

    /**
     * @brief Multi-class training metrics (if num_classes > 2)
     */
    std::optional<MultiClassMetrics> multi_class_train_metrics;

    // -- Prediction output --

    /**
     * @brief Number of observations predicted
     */
    std::size_t prediction_observations = 0;

    /**
     * @brief Summary of written outputs (DataManager keys, group IDs)
     *
     * Present when prediction and output writing succeeded.
     */
    std::optional<PredictionWriterResult> writer_result;

    // -- Model --

    /**
     * @brief The trained model (ownership transferred to caller)
     *
     * Allows the caller to reuse the model for further predictions or
     * save it to disk. nullptr if training failed.
     */
    std::unique_ptr<MLModelOperation> trained_model;
};

// ============================================================================
// Pipeline entry point
// ============================================================================

/**
 * @brief Run the full supervised classification pipeline
 *
 * Orchestrates:
 * 1. **Validate** — checks feature tensor exists and is valid
 * 2. **Convert** — TensorData → arma::mat (NaN dropping, optional z-score)
 * 3. **Assemble labels** — from intervals, time-entity groups, or data-entity groups
 * 4. **Balance** — optional class balancing (subsample/oversample)
 * 5. **Segment** — (sequence models only) split into contiguous temporal sequences
 * 6. **Train** — trains the selected model
 * 7. **Predict** — on training data or a separate prediction tensor
 * 8. **Metrics** — computes classification metrics on predictions
 * 8. **Write output** — writes predictions as intervals, probabilities, and/or groups
 *
 * @param dm DataManager containing the feature tensor, label sources, and output targets
 * @param registry Model registry containing the model factory
 * @param config Full pipeline configuration
 * @param progress Optional callback for progress reporting
 * @return Complete pipeline result with metrics, outputs, and trained model
 */
[[nodiscard]] ClassificationPipelineResult runClassificationPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        ClassificationPipelineConfig const & config,
        PipelineProgressCallback const & progress = nullptr);

}// namespace MLCore

#endif// MLCORE_CLASSIFICATIONPIPELINE_HPP
