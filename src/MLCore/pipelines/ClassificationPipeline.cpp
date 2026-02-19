/**
 * @file ClassificationPipeline.cpp
 * @brief Implementation of the end-to-end supervised classification pipeline
 */

#include "ClassificationPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "features/FeatureConverter.hpp"
#include "features/FeatureValidator.hpp"
#include "features/LabelAssembler.hpp"
#include "metrics/ClassificationMetrics.hpp"
#include "models/MLModelOperation.hpp"
#include "models/MLModelRegistry.hpp"
#include "output/PredictionWriter.hpp"
#include "preprocessing/ClassBalancing.hpp"

#include <cstddef>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace MLCore {

// ============================================================================
// Stage names
// ============================================================================

std::string toString(ClassificationStage stage) {
    switch (stage) {
        case ClassificationStage::ValidatingFeatures:
            return "Validating features";
        case ClassificationStage::ConvertingFeatures:
            return "Converting features";
        case ClassificationStage::AssemblingLabels:
            return "Assembling labels";
        case ClassificationStage::BalancingClasses:
            return "Balancing classes";
        case ClassificationStage::Training:
            return "Training model";
        case ClassificationStage::Predicting:
            return "Predicting";
        case ClassificationStage::ComputingMetrics:
            return "Computing metrics";
        case ClassificationStage::WritingOutput:
            return "Writing output";
        case ClassificationStage::Complete:
            return "Complete";
        case ClassificationStage::Failed:
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
        PipelineProgressCallback const & cb,
        ClassificationStage stage,
        std::string const & msg) {
    if (cb) {
        cb(stage, msg);
    }
}

/**
 * @brief Build a failure result at a specific stage
 */
ClassificationPipelineResult makeFailure(
        ClassificationStage stage,
        std::string const & msg) {
    ClassificationPipelineResult result;
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
 * @brief Filter an arma::Row<size_t> by valid row indices
 *
 * Returns a new row containing only elements at the specified indices.
 */
arma::Row<std::size_t> filterLabels(
        arma::Row<std::size_t> const & labels,
        std::vector<std::size_t> const & valid_indices) {
    arma::Row<std::size_t> filtered(valid_indices.size());
    for (std::size_t i = 0; i < valid_indices.size(); ++i) {
        filtered[i] = labels[valid_indices[i]];
    }
    return filtered;
}

}// anonymous namespace

// ============================================================================
// Pipeline implementation
// ============================================================================

ClassificationPipelineResult runClassificationPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        ClassificationPipelineConfig const & config,
        PipelineProgressCallback progress) {
    // ========================================================================
    // Stage 1: Validate features
    // ========================================================================
    reportProgress(progress, ClassificationStage::ValidatingFeatures,
                   "Validating feature tensor '" + config.feature_tensor_key + "'");

    // Look up the feature tensor in DataManager
    auto feature_tensor = dm.getData<TensorData>(config.feature_tensor_key);
    if (!feature_tensor) {
        return makeFailure(ClassificationStage::ValidatingFeatures,
                           "Feature tensor '" + config.feature_tensor_key +
                                   "' not found in DataManager");
    }

    // Validate tensor structure
    auto tensor_validation = validateFeatureTensor(*feature_tensor);
    if (!tensor_validation.valid) {
        return makeFailure(ClassificationStage::ValidatingFeatures,
                           "Feature tensor validation failed: " + tensor_validation.message);
    }

    // Verify the tensor has time-indexed rows (required for label alignment)
    if (feature_tensor->rows().type() != RowType::TimeFrameIndex) {
        return makeFailure(ClassificationStage::ValidatingFeatures,
                           "Feature tensor must have RowType::TimeFrameIndex rows "
                           "for supervised classification");
    }

    // Extract row times from the feature tensor
    auto all_row_times = extractRowTimes(*feature_tensor);

    // Verify the model exists in the registry
    if (!registry.hasModel(config.model_name)) {
        return makeFailure(ClassificationStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' not found in registry");
    }

    // ========================================================================
    // Stage 2: Convert features (TensorData → arma::mat)
    // ========================================================================
    reportProgress(progress, ClassificationStage::ConvertingFeatures,
                   "Converting " + std::to_string(feature_tensor->numRows()) + "×" +
                           std::to_string(feature_tensor->numColumns()) + " tensor to arma::mat");

    ConvertedFeatures converted;
    try {
        converted = convertTensorToArma(*feature_tensor, config.conversion_config);
    } catch (std::exception const & e) {
        return makeFailure(ClassificationStage::ConvertingFeatures,
                           std::string("Feature conversion failed: ") + e.what());
    }

    // Track valid row times after NaN dropping
    auto valid_row_times = filterRowTimes(all_row_times, converted.valid_row_indices);

    if (converted.matrix.n_cols == 0) {
        return makeFailure(ClassificationStage::ConvertingFeatures,
                           "All rows were dropped during feature conversion "
                           "(all contain NaN/Inf values)");
    }

    // ========================================================================
    // Stage 3: Assemble labels
    // ========================================================================
    reportProgress(progress, ClassificationStage::AssemblingLabels,
                   "Assembling labels for " + std::to_string(valid_row_times.size()) +
                           " observations");

    AssembledLabels labels;
    try {
        labels = std::visit([&](auto const & label_cfg) -> AssembledLabels {
            using T = std::decay_t<decltype(label_cfg)>;

            if constexpr (std::is_same_v<T, LabelFromIntervals>) {
                // Need the interval series and time frame
                auto intervals = dm.getData<DigitalIntervalSeries>(config.label_interval_key);
                if (!intervals) {
                    throw std::runtime_error(
                            "Label interval series '" + config.label_interval_key +
                            "' not found in DataManager");
                }
                auto time_frame = dm.getTime(TimeKey(config.time_key_str));
                if (!time_frame) {
                    throw std::runtime_error(
                            "TimeFrame '" + config.time_key_str + "' not found");
                }
                return assembleLabelsFromIntervals(
                        *intervals, *time_frame, valid_row_times, label_cfg);

            } else if constexpr (std::is_same_v<T, LabelFromTimeEntityGroups>) {
                auto * groups = dm.getEntityGroupManager();
                auto * reg = dm.getEntityRegistry();
                if (!groups || !reg) {
                    throw std::runtime_error(
                            "EntityGroupManager or EntityRegistry not available");
                }
                return assembleLabelsFromTimeEntityGroups(
                        *groups, *reg, valid_row_times, label_cfg);

            } else if constexpr (std::is_same_v<T, LabelFromDataEntityGroups>) {
                auto * groups = dm.getEntityGroupManager();
                auto * reg = dm.getEntityRegistry();
                if (!groups || !reg) {
                    throw std::runtime_error(
                            "EntityGroupManager or EntityRegistry not available");
                }
                return assembleLabelsFromDataEntityGroups(
                        *groups, *reg, valid_row_times, label_cfg);

            } else {
                static_assert(sizeof(T) == 0, "Unhandled label config variant");
            }
        },
                            config.label_config);
    } catch (std::exception const & e) {
        return makeFailure(ClassificationStage::AssemblingLabels,
                           std::string("Label assembly failed: ") + e.what());
    }

    if (labels.num_classes < 2) {
        return makeFailure(ClassificationStage::AssemblingLabels,
                           "Need at least 2 classes for classification, got " +
                                   std::to_string(labels.num_classes));
    }

    // For group-based labeling, filter out unlabeled observations
    // (they have sentinel label = num_classes)
    arma::mat train_features = converted.matrix;
    arma::Row<std::size_t> train_labels = labels.labels;
    std::vector<TimeFrameIndex> train_row_times = valid_row_times;

    if (labels.unlabeled_count > 0) {
        // Build indices of labeled observations only
        std::vector<arma::uword> labeled_cols;
        labeled_cols.reserve(train_labels.n_elem - labels.unlabeled_count);
        for (arma::uword i = 0; i < train_labels.n_elem; ++i) {
            if (train_labels[i] < labels.num_classes) {
                labeled_cols.push_back(i);
            }
        }

        if (labeled_cols.empty()) {
            return makeFailure(ClassificationStage::AssemblingLabels,
                               "No labeled observations found — all rows are unlabeled");
        }

        // Subset the matrices
        arma::uvec col_indices(labeled_cols.size());
        for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
            col_indices[i] = labeled_cols[i];
        }
        train_features = train_features.cols(col_indices);

        arma::Row<std::size_t> filtered_labels(labeled_cols.size());
        std::vector<TimeFrameIndex> filtered_times;
        filtered_times.reserve(labeled_cols.size());
        for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
            filtered_labels[i] = train_labels[labeled_cols[i]];
            filtered_times.push_back(train_row_times[labeled_cols[i]]);
        }
        train_labels = filtered_labels;
        train_row_times = filtered_times;
    }

    // ========================================================================
    // Stage 4: Class balancing (optional)
    // ========================================================================
    bool was_balanced = false;
    if (config.balance_classes) {
        reportProgress(progress, ClassificationStage::BalancingClasses,
                       "Balancing " + std::to_string(train_features.n_cols) +
                               " training observations");

        try {
            auto balanced = balanceClasses(train_features, train_labels,
                                           config.balancing_config);
            if (balanced.was_balanced) {
                train_features = std::move(balanced.features);
                train_labels = std::move(balanced.labels);
                was_balanced = true;

                // Note: after balancing, row times are no longer 1:1 with
                // train_row_times (some rows may be duplicated or removed).
                // We don't update train_row_times because training doesn't
                // need time alignment — time is only needed for prediction.
            }
        } catch (std::exception const & e) {
            return makeFailure(ClassificationStage::BalancingClasses,
                               std::string("Class balancing failed: ") + e.what());
        }
    }

    // ========================================================================
    // Stage 5: Train model
    // ========================================================================
    reportProgress(progress, ClassificationStage::Training,
                   "Training '" + config.model_name + "' on " +
                           std::to_string(train_features.n_cols) + " observations × " +
                           std::to_string(train_features.n_rows) + " features");

    auto model = registry.create(config.model_name);
    if (!model) {
        return makeFailure(ClassificationStage::Training,
                           "Failed to create model '" + config.model_name + "'");
    }

    bool train_ok = model->train(train_features, train_labels, config.model_params);
    if (!train_ok || !model->isTrained()) {
        return makeFailure(ClassificationStage::Training,
                           "Model training failed for '" + config.model_name + "'");
    }

    // ========================================================================
    // Stage 6: Predict
    // ========================================================================
    // Determine prediction features and times
    arma::mat predict_features;
    std::vector<TimeFrameIndex> predict_row_times;
    bool do_prediction = false;

    if (config.prediction_region.predict_all_rows) {
        // Predict on all rows from the (original, pre-balance) converted tensor
        predict_features = converted.matrix;
        predict_row_times = valid_row_times;
        do_prediction = true;
    } else if (!config.prediction_region.prediction_tensor_key.empty()) {
        // Predict on a separate tensor
        auto pred_tensor = dm.getData<TensorData>(
                config.prediction_region.prediction_tensor_key);
        if (!pred_tensor) {
            return makeFailure(ClassificationStage::Predicting,
                               "Prediction tensor '" +
                                       config.prediction_region.prediction_tensor_key +
                                       "' not found in DataManager");
        }

        try {
            auto pred_converted = convertTensorToArma(*pred_tensor,
                                                      config.conversion_config);
            predict_features = std::move(pred_converted.matrix);

            if (pred_tensor->rows().type() == RowType::TimeFrameIndex) {
                auto pred_all_times = extractRowTimes(*pred_tensor);
                predict_row_times = filterRowTimes(pred_all_times,
                                                   pred_converted.valid_row_indices);
            } else {
                // Ordinal rows — generate synthetic sequential times
                predict_row_times.reserve(pred_converted.valid_row_indices.size());
                for (std::size_t i = 0; i < pred_converted.valid_row_indices.size(); ++i) {
                    predict_row_times.emplace_back(
                            static_cast<std::int64_t>(pred_converted.valid_row_indices[i]));
                }
            }
        } catch (std::exception const & e) {
            return makeFailure(ClassificationStage::Predicting,
                               std::string("Prediction feature conversion failed: ") +
                                       e.what());
        }
        do_prediction = true;
    }

    arma::Row<std::size_t> predictions;
    std::optional<arma::mat> probabilities;
    std::size_t prediction_count = 0;

    if (do_prediction) {
        reportProgress(progress, ClassificationStage::Predicting,
                       "Predicting on " + std::to_string(predict_features.n_cols) +
                               " observations");

        // Validate feature dimensionality matches training
        if (predict_features.n_rows != model->numFeatures()) {
            return makeFailure(ClassificationStage::Predicting,
                               "Prediction features have " +
                                       std::to_string(predict_features.n_rows) +
                                       " features but model expects " +
                                       std::to_string(model->numFeatures()));
        }

        // Apply z-score normalization with training parameters if they were computed
        if (config.conversion_config.zscore_normalize &&
            !converted.zscore_means.empty() &&
            !config.prediction_region.predict_all_rows) {
            try {
                applyZscoreNormalization(predict_features,
                                         converted.zscore_means,
                                         converted.zscore_stds,
                                         config.conversion_config.zscore_epsilon);
            } catch (std::exception const & e) {
                return makeFailure(ClassificationStage::Predicting,
                                   std::string("Z-score normalization of prediction "
                                               "features failed: ") +
                                           e.what());
            }
        }

        bool pred_ok = model->predict(predict_features, predictions);
        if (!pred_ok) {
            return makeFailure(ClassificationStage::Predicting,
                               "Model prediction failed");
        }
        prediction_count = predictions.n_elem;

        // Try to get probability estimates
        arma::mat prob_mat;
        if (model->predictProbabilities(predict_features, prob_mat)) {
            probabilities = std::move(prob_mat);
        }
    }

    // ========================================================================
    // Stage 7: Compute metrics
    // ========================================================================
    ClassificationPipelineResult result;
    result.success = true;
    result.training_observations = train_features.n_cols;
    result.num_features = train_features.n_rows;
    result.num_classes = labels.num_classes;
    result.class_names = labels.class_names;
    result.rows_dropped_nan = converted.rows_dropped;
    result.was_balanced = was_balanced;
    result.prediction_observations = prediction_count;
    result.trained_model = std::move(model);

    if (do_prediction && prediction_count > 0) {
        reportProgress(progress, ClassificationStage::ComputingMetrics,
                       "Computing metrics on " + std::to_string(prediction_count) +
                               " predictions");

        // Compute metrics when predicting on training data (for training-set score)
        if (config.prediction_region.predict_all_rows) {
            // Assemble ground-truth labels on the valid rows for comparison
            try {
                AssembledLabels pred_labels = std::visit(
                        [&](auto const & label_cfg) -> AssembledLabels {
                            using T = std::decay_t<decltype(label_cfg)>;

                            if constexpr (std::is_same_v<T, LabelFromIntervals>) {
                                auto intervals = dm.getData<DigitalIntervalSeries>(
                                        config.label_interval_key);
                                auto time_frame = dm.getTime(TimeKey(config.time_key_str));
                                return assembleLabelsFromIntervals(
                                        *intervals, *time_frame, predict_row_times, label_cfg);

                            } else if constexpr (std::is_same_v<T, LabelFromTimeEntityGroups>) {
                                return assembleLabelsFromTimeEntityGroups(
                                        *dm.getEntityGroupManager(),
                                        *dm.getEntityRegistry(),
                                        predict_row_times, label_cfg);

                            } else if constexpr (std::is_same_v<T, LabelFromDataEntityGroups>) {
                                return assembleLabelsFromDataEntityGroups(
                                        *dm.getEntityGroupManager(),
                                        *dm.getEntityRegistry(),
                                        predict_row_times, label_cfg);

                            } else {
                                static_assert(sizeof(T) == 0, "Unhandled label config variant");
                            }
                        },
                        config.label_config);

                if (labels.num_classes == 2) {
                    result.binary_train_metrics = computeBinaryMetrics(
                            predictions, pred_labels.labels);
                }
                result.multi_class_train_metrics = computeMultiClassMetrics(
                        predictions, pred_labels.labels, labels.num_classes);

            } catch (...) {
                // Metrics are best-effort — don't fail the pipeline
            }
        }

        // ====================================================================
        // Stage 8: Write output
        // ====================================================================
        reportProgress(progress, ClassificationStage::WritingOutput,
                       "Writing predictions to DataManager");

        try {
            PredictionOutput pred_output;
            pred_output.class_predictions = predictions;
            pred_output.class_probabilities = probabilities;
            pred_output.prediction_times = predict_row_times;

            auto writer_result = writePredictions(
                    dm, pred_output, labels.class_names, config.output_config);
            result.writer_result = std::move(writer_result);
        } catch (std::exception const & e) {
            return makeFailure(ClassificationStage::WritingOutput,
                               std::string("Output writing failed: ") + e.what());
        }
    }

    reportProgress(progress, ClassificationStage::Complete,
                   "Pipeline completed successfully");

    return result;
}

}// namespace MLCore
