/**
 * @file ClusteringPipeline.cpp
 * @brief Implementation of the end-to-end unsupervised clustering pipeline
 */

#include "ClusteringPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "features/FeatureConverter.hpp"
#include "features/FeatureValidator.hpp"
#include "models/MLModelOperation.hpp"
#include "models/MLModelRegistry.hpp"
#include "output/PredictionWriter.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

// ============================================================================
// Stage names
// ============================================================================

std::string toString(ClusteringStage stage) {
    switch (stage) {
        case ClusteringStage::ValidatingFeatures:
            return "Validating features";
        case ClusteringStage::ConvertingFeatures:
            return "Converting features";
        case ClusteringStage::Fitting:
            return "Fitting model";
        case ClusteringStage::AssigningClusters:
            return "Assigning clusters";
        case ClusteringStage::WritingOutput:
            return "Writing output";
        case ClusteringStage::Complete:
            return "Complete";
        case ClusteringStage::Failed:
            return "Failed";
    }
    return "Unknown";
}

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/**
 * @brief Report progress if a callback is registered
 */
void reportProgress(
        ClusteringProgressCallback const & cb,
        ClusteringStage stage,
        std::string const & msg) {
    if (cb) {
        cb(stage, msg);
    }
}

/**
 * @brief Build a failure result at a specific stage
 */
ClusteringPipelineResult makeFailure(
        ClusteringStage stage,
        std::string const & msg) {
    ClusteringPipelineResult result;
    result.success = false;
    result.error_message = msg;
    result.failed_stage = stage;
    return result;
}

/**
 * @brief Extract TimeFrameIndex values from a TensorData's RowDescriptor
 *
 * The tensor must have RowType::TimeFrameIndex rows.
 */
std::vector<TimeFrameIndex> extractRowTimes(TensorData const & tensor) {
    auto const & rd = tensor.rows();
    auto const & ts = rd.timeStorage();
    auto const count = ts.size();

    std::vector<TimeFrameIndex> row_times;
    row_times.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        row_times.push_back(ts.getTimeFrameIndexAt(i));
    }
    return row_times;
}

/**
 * @brief Filter a row_times vector to keep only indices that survived NaN dropping
 */
std::vector<TimeFrameIndex> filterRowTimes(
        std::vector<TimeFrameIndex> const & all_times,
        std::vector<std::size_t> const & valid_indices) {
    std::vector<TimeFrameIndex> filtered;
    filtered.reserve(valid_indices.size());
    for (auto idx: valid_indices) {
        filtered.push_back(all_times[idx]);
    }
    return filtered;
}

/**
 * @brief Compute per-cluster sizes from cluster assignments
 *
 * @param assignments Cluster labels (may include SIZE_MAX for noise)
 * @param num_clusters Number of actual clusters (excluding noise)
 * @return Vector of sizes indexed by cluster id
 */
std::vector<std::size_t> computeClusterSizes(
        arma::Row<std::size_t> const & assignments,
        std::size_t num_clusters) {
    std::vector<std::size_t> sizes(num_clusters, 0);
    for (std::size_t j = 0; j < assignments.n_elem; ++j) {
        if (assignments[j] < num_clusters) {
            sizes[assignments[j]]++;
        }
    }
    return sizes;
}

/**
 * @brief Count noise points (assigned SIZE_MAX) in cluster assignments
 */
std::size_t countNoisePoints(arma::Row<std::size_t> const & assignments,
                             std::size_t num_clusters) {
    std::size_t count = 0;
    for (std::size_t j = 0; j < assignments.n_elem; ++j) {
        if (assignments[j] >= num_clusters) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Generate cluster names from config prefix
 */
std::vector<std::string> generateClusterNames(
        std::string const & prefix,
        std::size_t num_clusters) {
    std::vector<std::string> names;
    names.reserve(num_clusters);
    for (std::size_t k = 0; k < num_clusters; ++k) {
        names.push_back(prefix + std::to_string(k));
    }
    return names;
}

}// anonymous namespace

// ============================================================================
// Pipeline implementation
// ============================================================================

ClusteringPipelineResult runClusteringPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        ClusteringPipelineConfig const & config,
        const ClusteringProgressCallback& progress) {
    // ========================================================================
    // Stage 1: Validate features
    // ========================================================================
    reportProgress(progress, ClusteringStage::ValidatingFeatures,
                   "Validating feature tensor '" + config.feature_tensor_key + "'");

    // Look up the feature tensor in DataManager
    auto feature_tensor = dm.getData<TensorData>(config.feature_tensor_key);
    if (!feature_tensor) {
        return makeFailure(ClusteringStage::ValidatingFeatures,
                           "Feature tensor '" + config.feature_tensor_key +
                                   "' not found in DataManager");
    }

    // Validate tensor structure
    auto tensor_validation = validateFeatureTensor(*feature_tensor);
    if (!tensor_validation.valid) {
        return makeFailure(ClusteringStage::ValidatingFeatures,
                           "Feature tensor validation failed: " + tensor_validation.message);
    }

    // Check that the tensor has time-indexed rows (needed for time-based output)
    bool const has_time_rows =
            (feature_tensor->rows().type() == RowType::TimeFrameIndex);

    // Extract row times if available (used for output writing)
    std::vector<TimeFrameIndex> all_row_times;
    if (has_time_rows) {
        all_row_times = extractRowTimes(*feature_tensor);
    }

    // Verify the model exists in the registry
    if (!registry.hasModel(config.model_name)) {
        return makeFailure(ClusteringStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' not found in registry");
    }

    // Verify the model is a clustering model
    auto task_type = registry.getTaskType(config.model_name);
    if (task_type && *task_type != MLTaskType::Clustering) {
        return makeFailure(ClusteringStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' is not a clustering model (task type: " +
                                   toString(*task_type) + ")");
    }

    // ========================================================================
    // Stage 2: Convert features (TensorData → arma::mat)
    // ========================================================================
    reportProgress(progress, ClusteringStage::ConvertingFeatures,
                   "Converting " + std::to_string(feature_tensor->numRows()) + "×" +
                           std::to_string(feature_tensor->numColumns()) + " tensor to arma::mat");

    ConvertedFeatures converted;
    try {
        converted = convertTensorToArma(*feature_tensor, config.conversion_config);
    } catch (std::exception const & e) {
        return makeFailure(ClusteringStage::ConvertingFeatures,
                           std::string("Feature conversion failed: ") + e.what());
    }

    // Track valid row times after NaN dropping
    std::vector<TimeFrameIndex> valid_row_times;
    if (has_time_rows) {
        valid_row_times = filterRowTimes(all_row_times, converted.valid_row_indices);
    }

    if (converted.matrix.n_cols == 0) {
        return makeFailure(ClusteringStage::ConvertingFeatures,
                           "All rows were dropped during feature conversion "
                           "(all contain NaN/Inf values)");
    }

    // ========================================================================
    // Stage 3: Fit model
    // ========================================================================
    reportProgress(progress, ClusteringStage::Fitting,
                   "Fitting '" + config.model_name + "' on " +
                           std::to_string(converted.matrix.n_cols) + " observations × " +
                           std::to_string(converted.matrix.n_rows) + " features");

    auto model = registry.create(config.model_name);
    if (!model) {
        return makeFailure(ClusteringStage::Fitting,
                           "Failed to create model '" + config.model_name + "'");
    }

    bool const fit_ok = model->fit(converted.matrix, config.model_params);
    if (!fit_ok || !model->isTrained()) {
        return makeFailure(ClusteringStage::Fitting,
                           "Model fitting failed for '" + config.model_name + "'");
    }

    std::size_t const num_clusters = model->numClasses();

    // ========================================================================
    // Stage 4: Assign clusters
    // ========================================================================
    arma::mat assign_features;
    std::vector<TimeFrameIndex> assign_row_times;
    bool has_assign_time_rows = false;
    bool do_assignment = false;

    if (config.assignment_region.assign_all_rows) {
        // Assign on all rows from the converted tensor
        assign_features = converted.matrix;
        assign_row_times = valid_row_times;
        has_assign_time_rows = has_time_rows;
        do_assignment = true;
    } else if (!config.assignment_region.assignment_tensor_key.empty()) {
        // Assign on a separate tensor
        auto assign_tensor = dm.getData<TensorData>(
                config.assignment_region.assignment_tensor_key);
        if (!assign_tensor) {
            return makeFailure(ClusteringStage::AssigningClusters,
                               "Assignment tensor '" +
                                       config.assignment_region.assignment_tensor_key +
                                       "' not found in DataManager");
        }

        try {
            auto assign_converted = convertTensorToArma(*assign_tensor,
                                                        config.conversion_config);
            assign_features = std::move(assign_converted.matrix);

            if (assign_tensor->rows().type() == RowType::TimeFrameIndex) {
                auto assign_all_times = extractRowTimes(*assign_tensor);
                assign_row_times = filterRowTimes(assign_all_times,
                                                  assign_converted.valid_row_indices);
                has_assign_time_rows = true;
            } else {
                // Ordinal rows — generate synthetic sequential times
                assign_row_times.reserve(assign_converted.valid_row_indices.size());
                for (unsigned long valid_row_indice : assign_converted.valid_row_indices) {
                    assign_row_times.emplace_back(
                            static_cast<std::int64_t>(valid_row_indice));
                }
                has_assign_time_rows = false;
            }
        } catch (std::exception const & e) {
            return makeFailure(ClusteringStage::AssigningClusters,
                               std::string("Assignment feature conversion failed: ") +
                                       e.what());
        }
        do_assignment = true;
    }

    arma::Row<std::size_t> assignments;
    std::size_t assignment_count = 0;
    std::size_t noise_count = 0;
    std::vector<std::size_t> cluster_sizes;

    if (do_assignment) {
        reportProgress(progress, ClusteringStage::AssigningClusters,
                       "Assigning clusters to " +
                               std::to_string(assign_features.n_cols) + " observations");

        // Validate feature dimensionality matches fitting
        if (assign_features.n_rows != model->numFeatures()) {
            return makeFailure(ClusteringStage::AssigningClusters,
                               "Assignment features have " +
                                       std::to_string(assign_features.n_rows) +
                                       " features but model expects " +
                                       std::to_string(model->numFeatures()));
        }

        // Apply z-score normalization with fitting parameters if computed
        if (config.conversion_config.zscore_normalize &&
            !converted.zscore_means.empty() &&
            !config.assignment_region.assign_all_rows) {
            try {
                applyZscoreNormalization(assign_features,
                                         converted.zscore_means,
                                         converted.zscore_stds,
                                         config.conversion_config.zscore_epsilon);
            } catch (std::exception const & e) {
                return makeFailure(ClusteringStage::AssigningClusters,
                                   std::string("Z-score normalization of assignment "
                                               "features failed: ") +
                                           e.what());
            }
        }

        bool const assign_ok = model->assignClusters(assign_features, assignments);
        if (!assign_ok) {
            return makeFailure(ClusteringStage::AssigningClusters,
                               "Cluster assignment failed");
        }

        assignment_count = assignments.n_elem;
        cluster_sizes = computeClusterSizes(assignments, num_clusters);
        noise_count = countNoisePoints(assignments, num_clusters);
    }

    // ========================================================================
    // Stage 5: Write output
    // ========================================================================
    ClusteringPipelineResult result;
    result.success = true;
    result.fitting_observations = converted.matrix.n_cols;
    result.num_features = converted.matrix.n_rows;
    result.num_clusters = num_clusters;
    result.rows_dropped_nan = converted.rows_dropped;
    result.noise_points = noise_count;
    result.assignment_observations = assignment_count;
    result.cluster_sizes = cluster_sizes;
    result.fitted_model = std::move(model);

    auto cluster_names = generateClusterNames(
            config.output_config.output_prefix, num_clusters);
    result.cluster_names = cluster_names;

    if (do_assignment && assignment_count > 0) {
        reportProgress(progress, ClusteringStage::WritingOutput,
                       "Writing cluster assignments to DataManager");

        try {
            // Filter out noise points (SIZE_MAX assignments from DBSCAN)
            // PredictionWriter expects all labels in [0, num_classes)
            arma::Row<std::size_t> filtered_assignments;
            std::vector<TimeFrameIndex> filtered_times;
            arma::mat filtered_features;

            if (noise_count > 0) {
                // Build indices of non-noise points
                std::vector<arma::uword> non_noise_cols;
                non_noise_cols.reserve(assignment_count - noise_count);
                for (arma::uword j = 0; j < assignments.n_elem; ++j) {
                    if (assignments[j] < num_clusters) {
                        non_noise_cols.push_back(j);
                    }
                }

                if (non_noise_cols.empty()) {
                    // All noise — skip output writing
                    reportProgress(progress, ClusteringStage::Complete,
                                   "Pipeline completed (all points are noise — no output)");
                    return result;
                }

                filtered_assignments.set_size(non_noise_cols.size());
                filtered_times.reserve(non_noise_cols.size());

                arma::uvec col_indices(non_noise_cols.size());
                for (std::size_t i = 0; i < non_noise_cols.size(); ++i) {
                    filtered_assignments[i] = assignments[non_noise_cols[i]];
                    if (!assign_row_times.empty()) {
                        filtered_times.push_back(assign_row_times[non_noise_cols[i]]);
                    }
                    col_indices[i] = non_noise_cols[i];
                }
                filtered_features = assign_features.cols(col_indices);
            } else {
                filtered_assignments = assignments;
                filtered_times = assign_row_times;
                filtered_features = assign_features;
            }

            // Build a PredictionOutput for reuse of the PredictionWriter infrastructure
            PredictionOutput pred_output;
            pred_output.class_predictions = filtered_assignments;
            pred_output.prediction_times = filtered_times;

            // Try to get soft cluster probabilities from the model if supported
            // (e.g., GaussianMixtureOperation) via predictProbabilities
            // which is the generic interface for probability output
            arma::mat prob_mat;
            if (result.fitted_model && result.fitted_model->predictProbabilities(
                                               filtered_features, prob_mat)) {
                pred_output.class_probabilities = std::move(prob_mat);
            }

            // Translate ClusteringOutputConfig → PredictionWriterConfig
            PredictionWriterConfig writer_config;
            writer_config.output_prefix = config.output_config.output_prefix;
            writer_config.write_intervals = config.output_config.write_intervals && has_assign_time_rows;
            writer_config.write_probabilities = config.output_config.write_probabilities;
            writer_config.write_to_putative_groups = config.output_config.write_to_putative_groups && has_assign_time_rows;
            writer_config.time_key_str = config.output_config.time_key_str;

            if (config.defer_dm_writes) {
                // Store output for the caller to write on the main thread.
                result.deferred_output = std::move(pred_output);
                result.deferred_cluster_names = cluster_names;
                result.deferred_output_config = writer_config;
            } else {
                auto writer_result = writePredictions(
                        dm, pred_output, cluster_names, writer_config);

                result.interval_keys = std::move(writer_result.interval_keys);
                result.probability_keys = std::move(writer_result.probability_keys);
                result.putative_group_ids = std::move(writer_result.putative_group_ids);
            }

        } catch (std::exception const & e) {
            return makeFailure(ClusteringStage::WritingOutput,
                               std::string("Output writing failed: ") + e.what());
        }
    }

    reportProgress(progress, ClusteringStage::Complete,
                   "Pipeline completed successfully");

    return result;
}

}// namespace MLCore
