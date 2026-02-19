#ifndef MLCORE_WIDGET_STATE_DATA_HPP
#define MLCORE_WIDGET_STATE_DATA_HPP

/**
 * @file MLCoreWidgetStateData.hpp
 * @brief Serializable data structure for MLCoreWidgetState
 *
 * This struct holds all persistent state for the MLCore_Widget.
 * It is designed for reflect-cpp JSON serialization and contains
 * no Qt types — only standard library types.
 *
 * ## Fields
 *
 * - **instance_id / display_name**: Standard EditorState identity fields.
 * - **feature_tensor_key**: DataManager key of the TensorData to use as features.
 * - **training_region_key / prediction_region_key**: DigitalIntervalSeries keys
 *   defining which frames to use for training and prediction.
 * - **label_source_type**: One of "intervals", "groups", or "entity_groups".
 * - **label_interval_key**: DigitalIntervalSeries key for binary interval labels.
 * - **label_group_ids**: EntityGroupManager group IDs for group-based labeling.
 * - **selected_model_name**: Registry name of the selected MLModelOperation.
 * - **model_parameters_json**: JSON string of model-specific parameters.
 * - **output_prefix**: Prefix for putative group and data output names.
 * - **probability_threshold**: Decision threshold for binary classification.
 * - **output_probabilities**: Whether to write probability AnalogTimeSeries.
 * - **active_tab**: Index of the currently active workflow tab (0=classification, 1=clustering).
 *
 * @see MLCoreWidgetState for the EditorState wrapper
 * @see ClassificationPipeline for the supervised pipeline this state drives
 * @see ClusteringPipeline for the unsupervised pipeline this state drives
 */

#include <cstdint>
#include <string>
#include <vector>

struct MLCoreWidgetStateData {
    /// Unique instance identifier (matches EditorState::getInstanceId)
    std::string instance_id;

    /// Display name shown in dock tab
    std::string display_name = "ML Workflow";

    // === Feature configuration ===

    /// DataManager key of the TensorData to use as the feature matrix
    std::string feature_tensor_key;

    // === Region configuration ===

    /// DigitalIntervalSeries key defining the training region
    std::string training_region_key;

    /// DigitalIntervalSeries key defining the prediction region (empty = all frames)
    std::string prediction_region_key;

    // === Label configuration ===

    /// Label source type: "intervals", "groups", or "entity_groups"
    std::string label_source_type = "groups";

    /// DigitalIntervalSeries key for binary interval-based labels
    std::string label_interval_key;

    /// Class name for the positive class (inside intervals) in interval mode
    std::string label_positive_class_name = "Inside";

    /// Class name for the negative class (outside intervals) in interval mode
    std::string label_negative_class_name = "Outside";

    /// EntityGroupManager group IDs for group-based labeling
    std::vector<uint64_t> label_group_ids;

    /// DataManager key for data-entity group labeling (entity_groups mode)
    std::string label_data_key;

    // === Model configuration ===

    /// Registry name of the selected MLModelOperation (e.g. "Random Forest")
    std::string selected_model_name = "Random Forest";

    /// JSON string of model-specific parameters
    std::string model_parameters_json;

    // === Balancing configuration ===

    /// Whether class balancing is enabled
    bool balancing_enabled = false;

    /// Balancing strategy: "subsample" or "oversample"
    std::string balancing_strategy = "subsample";

    /// Maximum ratio of majority to minority class samples
    double balancing_max_ratio = 1.0;

    // === Output configuration ===

    /// Prefix for output data keys (e.g. "ml_contact" → "Predicted:contact")
    std::string output_prefix = "ml";

    /// Decision threshold for binary classification
    double probability_threshold = 0.5;

    /// Whether to output probability values as AnalogTimeSeries
    bool output_probabilities = true;

    /// Whether to output predictions as DigitalIntervalSeries
    bool output_predictions = true;

    // === Clustering configuration ===

    /// DataManager key of the TensorData to use for clustering
    std::string clustering_tensor_key;

    /// Registry name of the selected clustering algorithm (e.g. "K-Means")
    std::string clustering_model_name = "K-Means";

    /// Output prefix for clustering results
    std::string clustering_output_prefix = "Cluster:";

    /// Whether to write cluster intervals to DataManager
    bool clustering_write_intervals = true;

    /// Whether to write cluster probabilities (if model supports them)
    bool clustering_write_probabilities = true;

    /// Whether to normalize features (z-score) before clustering
    bool clustering_zscore_normalize = false;

    // === UI state ===

    /// Active workflow tab index (0 = classification, 1 = clustering)
    int active_tab = 0;
};

#endif // MLCORE_WIDGET_STATE_DATA_HPP
