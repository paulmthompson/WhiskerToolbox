/**
 * @file ClassificationPipeline.cpp
 * @brief Implementation of the end-to-end supervised classification pipeline
 */

#include "ClassificationPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
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
#include "pipelines/BoundingSpan.hpp"
#include "pipelines/SequenceAssembler.hpp"
#include "preprocessing/ClassBalancing.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
        case ClassificationStage::SegmentingSequences:
            return "Segmenting sequences";
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

/**
 * @brief Split prediction segments at labeled-block boundaries
 *
 * When constrained Viterbi decoding is enabled, the initial state of each
 * segment is forced to a known label. With a single long segment this only
 * anchors the very first frame; accuracy degrades for frames far from that
 * anchor.
 *
 * This function splits at **both** label-boundary transitions:
 *   - unlabeled→labeled  (start of a training block)
 *   - labeled→unlabeled  (end of a training block)
 *
 * This ensures unlabeled gaps become their own segments, bookended by
 * labeled blocks. Each sub-segment's initial state is constrained either
 * directly (segment starts on a labeled frame) or via the first_time-1
 * fallback (segment starts right after a labeled block ends).
 *
 * @param segments Prediction segments from SequenceAssembler
 * @param label_map Map from TimeFrameIndex raw values to their known class labels
 * @return Vector of sub-segments, split at every label-boundary transition
 */
[[nodiscard]] std::vector<SequenceSegment> splitSegmentsAtLabelBoundaries(
        std::vector<SequenceSegment> const & segments,
        std::unordered_map<std::int64_t, std::size_t> const & label_map) {

    std::vector<SequenceSegment> result;
    result.reserve(segments.size() * 2);

    for (auto const & seg: segments) {
        if (seg.times.size() < 2) {
            result.push_back(seg);
            continue;
        }

        // Find indices where the labeled/unlabeled status changes
        std::vector<std::size_t> split_points;
        split_points.push_back(0);

        for (std::size_t i = 1; i < seg.times.size(); ++i) {
            bool const curr_labeled = label_map.contains(seg.times[i].getValue());
            bool const prev_labeled = label_map.contains(seg.times[i - 1].getValue());

            // Split at both transitions:
            //   unlabeled→labeled: start of a new training block
            //   labeled→unlabeled: end of a training block (gap begins)
            if (curr_labeled != prev_labeled) {
                split_points.push_back(i);
            }
        }

        if (split_points.size() <= 1) {
            result.push_back(seg);
            continue;
        }

        // Create sub-segments at each split point
        for (std::size_t s = 0; s < split_points.size(); ++s) {
            auto const start = split_points[s];
            auto const end = (s + 1 < split_points.size())
                                     ? split_points[s + 1]
                                     : seg.times.size();

            if (end <= start) {
                continue;
            }

            SequenceSegment sub;
            sub.features = seg.features.cols(
                    static_cast<arma::uword>(start),
                    static_cast<arma::uword>(end - 1));

            sub.times.assign(
                    seg.times.begin() + static_cast<std::ptrdiff_t>(start),
                    seg.times.begin() + static_cast<std::ptrdiff_t>(end));

            result.push_back(std::move(sub));
        }
    }

    return result;
}

}// anonymous namespace

// ============================================================================
// Pipeline implementation
// ============================================================================

ClassificationPipelineResult runClassificationPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        ClassificationPipelineConfig const & config,
        PipelineProgressCallback const & progress) {
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

            } else if constexpr (std::is_same_v<T, LabelFromEvents>) {
                auto event_series = dm.getData<DigitalEventSeries>(config.label_event_key);
                if (!event_series) {
                    throw std::runtime_error(
                            "Label event series '" + config.label_event_key +
                            "' not found in DataManager");
                }
                auto time_frame = dm.getTime(TimeKey(config.time_key_str));
                if (!time_frame) {
                    throw std::runtime_error(
                            "TimeFrame '" + config.time_key_str + "' not found");
                }
                return assembleLabelsFromEvents(
                        *event_series, *time_frame, valid_row_times, label_cfg);

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
    // Stage 3b: Filter to training region (optional)
    // ========================================================================
    // When a training interval key is specified, restrict training data to
    // only the rows whose times fall within the training intervals. Rows
    // outside the training region are excluded from model fitting but may
    // still be predicted on later (Stage 6).
    if (!config.training_interval_key.empty()) {
        auto training_intervals = dm.getData<DigitalIntervalSeries>(
                config.training_interval_key);
        if (!training_intervals) {
            return makeFailure(ClassificationStage::AssemblingLabels,
                               "Training interval series '" +
                                       config.training_interval_key +
                                       "' not found in DataManager");
        }
        if (training_intervals->size() == 0) {
            return makeFailure(ClassificationStage::AssemblingLabels,
                               "Training interval series '" +
                                       config.training_interval_key +
                                       "' is empty — no training region defined");
        }

        auto filtered = filterTrainingRowsToIntervals(
                train_features, train_labels, train_row_times,
                *training_intervals);

        if (filtered.features.n_cols == 0) {
            return makeFailure(ClassificationStage::AssemblingLabels,
                               "No training observations remain after filtering "
                               "to training region '" +
                                       config.training_interval_key + "'");
        }

        reportProgress(progress, ClassificationStage::AssemblingLabels,
                       "Filtered to training region: " +
                               std::to_string(filtered.features.n_cols) +
                               " of " + std::to_string(train_features.n_cols) +
                               " observations");

        train_features = std::move(filtered.features);
        train_labels = std::move(filtered.labels);
        train_row_times = std::move(filtered.times);
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
    // Stage 5: Create model & optional sequence segmentation
    // ========================================================================
    auto model = registry.create(config.model_name);
    if (!model) {
        return makeFailure(ClassificationStage::Training,
                           "Failed to create model '" + config.model_name + "'");
    }

    bool const is_sequence_model = model->isSequenceModel();
    std::vector<SequenceSegment> train_segments;

    if (is_sequence_model) {
        reportProgress(progress, ClassificationStage::SegmentingSequences,
                       "Segmenting " + std::to_string(train_row_times.size()) +
                               " observations into contiguous sequences");

        // Sequence models require time-aligned training data (no balancing),
        // because balancing destroys temporal ordering.
        // Use the pre-balance features/labels/times for segmentation.
        auto const & seg_features = (was_balanced) ? converted.matrix : train_features;
        auto const & seg_labels = (was_balanced) ? labels.labels : train_labels;
        auto const & seg_times = (was_balanced) ? valid_row_times : train_row_times;

        // Filter out unlabeled observations for segmentation if needed
        arma::mat labeled_features;
        arma::Row<std::size_t> labeled_labels;
        std::vector<TimeFrameIndex> labeled_times;

        if (labels.unlabeled_count > 0 && was_balanced) {
            // Pre-balance data still has unlabeled rows — filter them
            labeled_features = seg_features;
            labeled_labels = seg_labels;
            labeled_times = seg_times;

            std::vector<arma::uword> labeled_cols;
            for (arma::uword i = 0; i < labeled_labels.n_elem; ++i) {
                if (labeled_labels[i] < labels.num_classes) {
                    labeled_cols.push_back(i);
                }
            }
            arma::uvec col_indices(labeled_cols.size());
            for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
                col_indices[i] = labeled_cols[i];
            }
            labeled_features = labeled_features.cols(col_indices);

            arma::Row<std::size_t> filtered_lb(labeled_cols.size());
            std::vector<TimeFrameIndex> filtered_tm;
            filtered_tm.reserve(labeled_cols.size());
            for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
                filtered_lb[i] = labeled_labels[labeled_cols[i]];
                filtered_tm.push_back(labeled_times[labeled_cols[i]]);
            }
            labeled_features = labeled_features;
            labeled_labels = filtered_lb;
            labeled_times = filtered_tm;
        } else {
            labeled_features = seg_features;
            labeled_labels = seg_labels;
            labeled_times = seg_times;
        }

        train_segments = SequenceAssembler::segment(
                labeled_features, labeled_labels, labeled_times);

        if (train_segments.empty()) {
            return makeFailure(ClassificationStage::SegmentingSequences,
                               "No contiguous sequences found meeting the minimum "
                               "length requirement");
        }
    }

    // ========================================================================
    // Stage 5b: Train model
    // ========================================================================
    reportProgress(progress, ClassificationStage::Training,
                   "Training '" + config.model_name + "' on " +
                           std::to_string(train_features.n_cols) + " observations × " +
                           std::to_string(train_features.n_rows) + " features");

    bool train_ok = false;
    if (is_sequence_model) {
        // Extract feature/label vectors from segments
        std::vector<arma::mat> feature_seqs;
        std::vector<arma::Row<std::size_t>> label_seqs;
        feature_seqs.reserve(train_segments.size());
        label_seqs.reserve(train_segments.size());
        for (auto const & seg: train_segments) {
            feature_seqs.push_back(seg.features);
            label_seqs.push_back(seg.labels);
        }
        train_ok = model->trainSequences(feature_seqs, label_seqs, config.model_params);
    } else {
        train_ok = model->train(train_features, train_labels, config.model_params);
    }

    if (!train_ok || !model->isTrained()) {
        return makeFailure(ClassificationStage::Training,
                           "Model training failed for '" + config.model_name + "'");
    }

    // ========================================================================
    // Stage 5c: Leave-one-interval-out cross-validation (optional)
    // ========================================================================
    // When max_cv_folds > 0 and training intervals are available, evaluate
    // generalization by holding out one training interval per fold.
    // The main model (trained above on ALL data) is kept — CV only produces
    // metrics and does not alter the final trained model.
    std::optional<BinaryClassificationMetrics> cv_binary_metrics;
    std::optional<MultiClassMetrics> cv_multi_metrics;
    std::size_t cv_folds_run = 0;
    std::vector<double> cv_per_fold_accuracy;

    if (config.max_cv_folds > 0 && !config.training_interval_key.empty() &&
        is_sequence_model) {
        auto cv_train_intervals = dm.getData<DigitalIntervalSeries>(
                config.training_interval_key);

        if (cv_train_intervals && cv_train_intervals->size() > 1) {
            // Collect all training intervals
            std::vector<Interval> all_train_ivs;
            all_train_ivs.reserve(cv_train_intervals->size());
            for (auto const & iwid: cv_train_intervals->view()) {
                all_train_ivs.push_back(iwid.interval);
            }
            std::sort(all_train_ivs.begin(), all_train_ivs.end());

            std::size_t const num_folds =
                    std::min(config.max_cv_folds, all_train_ivs.size());

            reportProgress(progress, ClassificationStage::Training,
                           "Running " + std::to_string(num_folds) +
                                   "-fold leave-one-interval-out CV");

            // Accumulators for averaging metrics
            std::vector<double> fold_accuracies;
            fold_accuracies.reserve(num_folds);

            // For averaging binary metrics
            double sum_accuracy = 0.0;
            double sum_sensitivity = 0.0;
            double sum_specificity = 0.0;
            double sum_dice = 0.0;
            std::size_t binary_folds_counted = 0;

            // For averaging multi-class metrics
            double sum_mc_accuracy = 0.0;
            std::size_t mc_folds_counted = 0;
            // Per-class accumulators (sized on first successful fold)
            std::vector<double> sum_precision;
            std::vector<double> sum_recall;
            std::vector<double> sum_f1;

            // Use labeled pre-balance data for CV (same as main segmentation)
            auto const & cv_features = (was_balanced) ? converted.matrix : train_features;
            auto const & cv_labels_row = (was_balanced) ? labels.labels : train_labels;
            auto const & cv_times = (was_balanced) ? valid_row_times : train_row_times;

            // Filter to labeled only (same logic as main path)
            arma::mat cv_labeled_feats;
            arma::Row<std::size_t> cv_labeled_labels;
            std::vector<TimeFrameIndex> cv_labeled_times;

            if (labels.unlabeled_count > 0) {
                std::vector<arma::uword> labeled_cols;
                for (arma::uword i = 0; i < cv_labels_row.n_elem; ++i) {
                    if (cv_labels_row[i] < labels.num_classes) {
                        labeled_cols.push_back(i);
                    }
                }
                arma::uvec col_idx(labeled_cols.size());
                for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
                    col_idx[i] = labeled_cols[i];
                }
                cv_labeled_feats = cv_features.cols(col_idx);
                cv_labeled_labels.set_size(labeled_cols.size());
                cv_labeled_times.reserve(labeled_cols.size());
                for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
                    cv_labeled_labels[i] = cv_labels_row[labeled_cols[i]];
                    cv_labeled_times.push_back(cv_times[labeled_cols[i]]);
                }
            } else {
                cv_labeled_feats = cv_features;
                cv_labeled_labels = cv_labels_row;
                cv_labeled_times = cv_times;
            }

            for (std::size_t fold = 0; fold < num_folds; ++fold) {
                auto const & held_out_iv = all_train_ivs[fold];

                // --- Split data: fold-train vs held-out ---
                std::vector<arma::uword> fold_train_cols;
                std::vector<arma::uword> fold_test_cols;
                fold_train_cols.reserve(cv_labeled_times.size());
                fold_test_cols.reserve(cv_labeled_times.size());

                for (std::size_t i = 0; i < cv_labeled_times.size(); ++i) {
                    auto const t = cv_labeled_times[i].getValue();
                    bool const in_held_out =
                            (t >= held_out_iv.start && t <= held_out_iv.end);

                    // Only include in fold-train if in one of the other
                    // training intervals (not just "not held out")
                    if (in_held_out) {
                        fold_test_cols.push_back(static_cast<arma::uword>(i));
                    } else {
                        // Check that this time is in one of the training intervals
                        for (auto const & iv: all_train_ivs) {
                            if (&iv == &held_out_iv) continue;
                            if (t >= iv.start && t <= iv.end) {
                                fold_train_cols.push_back(
                                        static_cast<arma::uword>(i));
                                break;
                            }
                        }
                    }
                }

                if (fold_train_cols.empty() || fold_test_cols.empty()) {
                    continue;// Skip degenerate fold
                }

                // Extract fold subsets
                arma::uvec ft_idx(fold_train_cols.size());
                for (std::size_t i = 0; i < fold_train_cols.size(); ++i) {
                    ft_idx[i] = fold_train_cols[i];
                }
                arma::mat const fold_feats = cv_labeled_feats.cols(ft_idx);
                arma::Row<std::size_t> fold_labels(fold_train_cols.size());
                std::vector<TimeFrameIndex> fold_times;
                fold_times.reserve(fold_train_cols.size());
                for (std::size_t i = 0; i < fold_train_cols.size(); ++i) {
                    fold_labels[i] = cv_labeled_labels[fold_train_cols[i]];
                    fold_times.push_back(cv_labeled_times[fold_train_cols[i]]);
                }

                arma::uvec ftest_idx(fold_test_cols.size());
                for (std::size_t i = 0; i < fold_test_cols.size(); ++i) {
                    ftest_idx[i] = fold_test_cols[i];
                }
                arma::mat const test_feats = cv_labeled_feats.cols(ftest_idx);
                arma::Row<std::size_t> test_labels(fold_test_cols.size());
                std::vector<TimeFrameIndex> test_times;
                test_times.reserve(fold_test_cols.size());
                for (std::size_t i = 0; i < fold_test_cols.size(); ++i) {
                    test_labels[i] = cv_labeled_labels[fold_test_cols[i]];
                    test_times.push_back(cv_labeled_times[fold_test_cols[i]]);
                }

                // --- Segment fold-train data ---
                auto fold_segments = SequenceAssembler::segment(
                        fold_feats, fold_labels, fold_times);
                if (fold_segments.empty()) continue;

                // --- Train fold model ---
                auto fold_model = registry.create(config.model_name);
                if (!fold_model) continue;

                std::vector<arma::mat> fold_feat_seqs;
                std::vector<arma::Row<std::size_t>> fold_label_seqs;
                fold_feat_seqs.reserve(fold_segments.size());
                fold_label_seqs.reserve(fold_segments.size());
                for (auto const & seg: fold_segments) {
                    fold_feat_seqs.push_back(seg.features);
                    fold_label_seqs.push_back(seg.labels);
                }

                bool const fold_train_ok = fold_model->trainSequences(
                        fold_feat_seqs, fold_label_seqs, config.model_params);
                if (!fold_train_ok || !fold_model->isTrained()) continue;

                // --- Predict held-out interval ---
                // Segment held-out test data, then predict with boundary
                // constraints from the fold-train label map
                auto test_segments = SequenceAssembler::segment(
                        test_feats, test_times);
                if (test_segments.empty()) continue;

                // Build label map from fold-train data for constraints
                std::unordered_map<std::int64_t, std::size_t> fold_label_map;
                fold_label_map.reserve(fold_times.size());
                for (std::size_t i = 0; i < fold_times.size(); ++i) {
                    fold_label_map[fold_times[i].getValue()] = fold_labels[i];
                }

                // Split test segments at label boundaries (using fold-train labels)
                if (config.prediction_region.constrained_decoding) {
                    test_segments = splitSegmentsAtLabelBoundaries(
                            test_segments, fold_label_map);
                    if (test_segments.empty()) continue;
                }

                std::vector<arma::mat> test_feat_seqs;
                test_feat_seqs.reserve(test_segments.size());
                for (auto const & seg: test_segments) {
                    test_feat_seqs.push_back(seg.features);
                }

                std::vector<arma::Row<std::size_t>> test_pred_seqs;
                bool fold_pred_ok = false;

                if (config.prediction_region.constrained_decoding) {
                    // Build initial/terminal constraints from fold labels
                    std::vector<std::optional<std::size_t>> init_c;
                    std::vector<std::optional<std::size_t>> term_c;
                    init_c.reserve(test_segments.size());
                    term_c.reserve(test_segments.size());

                    for (auto const & seg: test_segments) {
                        auto const ft = seg.times.front().getValue();
                        if (auto it = fold_label_map.find(ft);
                            it != fold_label_map.end()) {
                            init_c.emplace_back(it->second);
                        } else if (auto it2 = fold_label_map.find(ft - 1);
                                   it2 != fold_label_map.end()) {
                            init_c.emplace_back(it2->second);
                        } else {
                            init_c.emplace_back(std::nullopt);
                        }

                        auto const lt = seg.times.back().getValue();
                        if (auto it = fold_label_map.find(lt);
                            it != fold_label_map.end()) {
                            term_c.emplace_back(it->second);
                        } else if (auto it2 = fold_label_map.find(lt + 1);
                                   it2 != fold_label_map.end()) {
                            term_c.emplace_back(it2->second);
                        } else {
                            term_c.emplace_back(std::nullopt);
                        }
                    }

                    fold_pred_ok =
                            fold_model->predictSequencesBidirectionalConstrained(
                                    test_feat_seqs, test_pred_seqs, init_c, term_c);
                } else {
                    fold_pred_ok = fold_model->predictSequences(
                            test_feat_seqs, test_pred_seqs);
                }

                if (!fold_pred_ok) continue;

                // Reassemble fold predictions
                std::size_t total_fold_preds = 0;
                for (auto const & seq: test_pred_seqs) {
                    total_fold_preds += seq.n_elem;
                }

                // Build time→label lookup for held-out ground truth
                std::unordered_map<std::int64_t, std::size_t> test_truth_map;
                test_truth_map.reserve(test_times.size());
                for (std::size_t i = 0; i < test_times.size(); ++i) {
                    test_truth_map[test_times[i].getValue()] = test_labels[i];
                }

                arma::Row<std::size_t> fold_preds(total_fold_preds);
                arma::Row<std::size_t> fold_truth(total_fold_preds);
                std::size_t off = 0;
                for (std::size_t s = 0; s < test_pred_seqs.size(); ++s) {
                    auto const & sp = test_pred_seqs[s];
                    auto const & st = test_segments[s];
                    for (std::size_t j = 0; j < sp.n_elem; ++j) {
                        fold_preds[off + j] = sp[j];
                        auto const t_val = st.times[j].getValue();
                        if (auto it = test_truth_map.find(t_val);
                            it != test_truth_map.end()) {
                            fold_truth[off + j] = it->second;
                        } else {
                            fold_truth[off + j] = sp[j];// fallback
                        }
                    }
                    off += sp.n_elem;
                }

                // Compute fold accuracy
                std::size_t correct = 0;
                for (std::size_t i = 0; i < fold_preds.n_elem; ++i) {
                    if (fold_preds[i] == fold_truth[i]) ++correct;
                }
                double const fold_acc =
                        static_cast<double>(correct) /
                        static_cast<double>(fold_preds.n_elem);
                fold_accuracies.push_back(fold_acc);

                // Binary metrics
                if (labels.num_classes == 2) {
                    auto bm = computeBinaryMetrics(fold_preds, fold_truth);
                    sum_accuracy += bm.accuracy;
                    sum_sensitivity += bm.sensitivity;
                    sum_specificity += bm.specificity;
                    sum_dice += bm.dice_score;
                    ++binary_folds_counted;
                }

                // Multi-class metrics
                auto mcm = computeMultiClassMetrics(
                        fold_preds, fold_truth, labels.num_classes);
                sum_mc_accuracy += mcm.overall_accuracy;
                if (sum_precision.empty()) {
                    sum_precision.resize(labels.num_classes, 0.0);
                    sum_recall.resize(labels.num_classes, 0.0);
                    sum_f1.resize(labels.num_classes, 0.0);
                }
                for (std::size_t c = 0; c < labels.num_classes; ++c) {
                    sum_precision[c] += mcm.per_class_precision[c];
                    sum_recall[c] += mcm.per_class_recall[c];
                    sum_f1[c] += mcm.per_class_f1[c];
                }
                ++mc_folds_counted;

                ++cv_folds_run;
            }// end fold loop

            cv_per_fold_accuracy = std::move(fold_accuracies);

            // Average binary metrics
            if (binary_folds_counted > 0) {
                BinaryClassificationMetrics avg_bin;
                auto const n = static_cast<double>(binary_folds_counted);
                avg_bin.accuracy = sum_accuracy / n;
                avg_bin.sensitivity = sum_sensitivity / n;
                avg_bin.specificity = sum_specificity / n;
                avg_bin.dice_score = sum_dice / n;
                cv_binary_metrics = avg_bin;
            }

            // Average multi-class metrics
            if (mc_folds_counted > 0) {
                MultiClassMetrics avg_mc;
                auto const n = static_cast<double>(mc_folds_counted);
                avg_mc.overall_accuracy = sum_mc_accuracy / n;
                avg_mc.num_classes = labels.num_classes;
                avg_mc.per_class_precision.resize(labels.num_classes);
                avg_mc.per_class_recall.resize(labels.num_classes);
                avg_mc.per_class_f1.resize(labels.num_classes);
                for (std::size_t c = 0; c < labels.num_classes; ++c) {
                    avg_mc.per_class_precision[c] = sum_precision[c] / n;
                    avg_mc.per_class_recall[c] = sum_recall[c] / n;
                    avg_mc.per_class_f1[c] = sum_f1[c] / n;
                }
                cv_multi_metrics = avg_mc;
            }

            reportProgress(progress, ClassificationStage::Training,
                           "CV complete: " + std::to_string(cv_folds_run) +
                                   " folds");
        }
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
                for (unsigned long const valid_row_indice: pred_converted.valid_row_indices) {
                    predict_row_times.emplace_back(
                            static_cast<std::int64_t>(valid_row_indice));
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
    arma::Row<std::size_t> raw_predictions;// Before GT substitution — used for metrics
    std::optional<arma::mat> probabilities;
    std::size_t prediction_count = 0;

    // Track the prediction interval series for possible output filtering
    std::shared_ptr<DigitalIntervalSeries> prediction_intervals;

    // Apply bounding span filtering when a prediction interval key is specified
    if (do_prediction &&
        !config.prediction_region.prediction_interval_key.empty()) {
        prediction_intervals = dm.getData<DigitalIntervalSeries>(
                config.prediction_region.prediction_interval_key);
        if (!prediction_intervals) {
            return makeFailure(ClassificationStage::Predicting,
                               "Prediction interval series '" +
                                       config.prediction_region.prediction_interval_key +
                                       "' not found in DataManager");
        }

        auto pred_bounds = computeIntervalBounds(*prediction_intervals);
        if (pred_bounds) {
            // Optionally include training label intervals in the span
            auto span = *pred_bounds;
            if (!config.label_interval_key.empty()) {
                auto train_intervals = dm.getData<DigitalIntervalSeries>(
                        config.label_interval_key);
                if (train_intervals) {
                    auto train_bounds = computeIntervalBounds(*train_intervals);
                    if (train_bounds) {
                        span = mergeSpans(span, *train_bounds);
                    }
                }
            }

            auto filtered = filterRowsToSpan(
                    predict_features, predict_row_times, span);
            predict_features = std::move(filtered.features);
            predict_row_times = std::move(filtered.times);

            if (predict_features.n_cols == 0) {
                return makeFailure(ClassificationStage::Predicting,
                                   "No prediction rows remain after bounding-span "
                                   "filtering to [" +
                                           std::to_string(span.min_time.getValue()) +
                                           ", " +
                                           std::to_string(span.max_time.getValue()) +
                                           "]");
            }
        }
    }

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

        bool pred_ok = false;
        if (is_sequence_model) {
            // Segment prediction data into contiguous sequences
            auto pred_segments = SequenceAssembler::segment(
                    predict_features, predict_row_times);

            if (pred_segments.empty()) {
                return makeFailure(ClassificationStage::Predicting,
                                   "No contiguous prediction sequences found "
                                   "meeting the minimum length requirement");
            }

            // Build label_map early — used for both segment splitting and
            // initial-state constraint lookup during constrained decoding
            std::unordered_map<std::int64_t, std::size_t> label_map;
            bool const use_constrained =
                    config.prediction_region.constrained_decoding &&
                    !train_row_times.empty();

            if (use_constrained) {
                label_map.reserve(train_row_times.size());
                for (std::size_t i = 0; i < train_row_times.size(); ++i) {
                    label_map[train_row_times[i].getValue()] = train_labels[i];
                }

                // Split segments at labeled-block boundaries so each
                // sub-segment gets its own initial-state constraint,
                // improving prediction accuracy for longer sequences.
                pred_segments = splitSegmentsAtLabelBoundaries(
                        pred_segments, label_map);

                if (pred_segments.empty()) {
                    return makeFailure(ClassificationStage::Predicting,
                                       "No prediction sequences remain after "
                                       "label-boundary splitting");
                }
            }

            // Build feature sequence vectors for predictSequences
            std::vector<arma::mat> pred_feature_seqs;
            pred_feature_seqs.reserve(pred_segments.size());
            for (auto const & seg: pred_segments) {
                pred_feature_seqs.push_back(seg.features);
            }

            std::vector<arma::Row<std::size_t>> pred_sequences;

            // Constraint vectors — populated only when constrained decoding
            // is active, but declared here so they remain in scope for the
            // per-segment probability pass below.
            std::vector<std::optional<std::size_t>> initial_constraints;
            std::vector<std::optional<std::size_t>> terminal_constraints;

            if (use_constrained) {
                // For each prediction segment, determine initial and terminal
                // state constraints from the label map
                initial_constraints.reserve(pred_segments.size());
                terminal_constraints.reserve(pred_segments.size());

                for (auto const & seg: pred_segments) {
                    // --- Initial constraint ---
                    auto const first_time = seg.times.front().getValue();
                    if (auto it = label_map.find(first_time); it != label_map.end()) {
                        initial_constraints.emplace_back(it->second);
                    } else if (auto it2 = label_map.find(first_time - 1);
                               it2 != label_map.end()) {
                        initial_constraints.emplace_back(it2->second);
                    } else {
                        initial_constraints.emplace_back(std::nullopt);
                    }

                    // --- Terminal constraint ---
                    auto const last_time = seg.times.back().getValue();
                    if (auto it = label_map.find(last_time); it != label_map.end()) {
                        terminal_constraints.emplace_back(it->second);
                    } else if (auto it2 = label_map.find(last_time + 1);
                               it2 != label_map.end()) {
                        terminal_constraints.emplace_back(it2->second);
                    } else {
                        terminal_constraints.emplace_back(std::nullopt);
                    }
                }

                pred_ok = model->predictSequencesBidirectionalConstrained(
                        pred_feature_seqs, pred_sequences,
                        initial_constraints, terminal_constraints);
            } else {
                pred_ok = model->predictSequences(pred_feature_seqs, pred_sequences);
            }

            if (pred_ok) {
                // Reassemble predictions and times in segment order
                std::size_t total_preds = 0;
                for (auto const & seq: pred_sequences) {
                    total_preds += seq.n_elem;
                }
                predictions.set_size(total_preds);
                std::vector<TimeFrameIndex> reassembled_times;
                reassembled_times.reserve(total_preds);

                std::size_t offset = 0;
                for (std::size_t s = 0; s < pred_sequences.size(); ++s) {
                    auto const & seq_preds = pred_sequences[s];
                    auto const & seg_times = pred_segments[s].times;
                    for (std::size_t j = 0; j < seq_preds.n_elem; ++j) {
                        predictions[offset + j] = seq_preds[j];
                        reassembled_times.push_back(seg_times[j]);
                    }
                    offset += seq_preds.n_elem;
                }
                predict_row_times = std::move(reassembled_times);

                // Save raw model predictions before GT substitution.
                // Metrics are always computed on raw_predictions so they
                // reflect actual model performance rather than 100%.
                raw_predictions = predictions;

                // Ground-truth substitution: overwrite predictions for
                // labeled frames with their known labels so the final
                // output is exact on training data.
                for (std::size_t i = 0; i < predict_row_times.size(); ++i) {
                    if (auto it = label_map.find(predict_row_times[i].getValue());
                        it != label_map.end()) {
                        predictions[i] = it->second;
                    }
                }

                // Per-segment constrained probability estimates
                if (use_constrained) {
                    std::vector<arma::mat> prob_seqs;
                    if (model->predictProbabilitiesPerSequence(
                                pred_feature_seqs, prob_seqs,
                                initial_constraints, terminal_constraints)) {
                        // Reassemble per-segment probability matrices into a
                        // single (num_states × total_obs) matrix in segment order
                        std::size_t const num_states =
                                prob_seqs.empty() ? 0 : prob_seqs.front().n_rows;
                        arma::mat full_prob(num_states, predictions.n_elem);
                        std::size_t col_offset = 0;
                        for (auto const & pm: prob_seqs) {
                            full_prob.cols(col_offset, col_offset + pm.n_cols - 1) = pm;
                            col_offset += pm.n_cols;
                        }

                        // Set delta probabilities for ground-truth frames
                        for (std::size_t i = 0; i < predict_row_times.size(); ++i) {
                            if (auto it = label_map.find(
                                        predict_row_times[i].getValue());
                                it != label_map.end()) {
                                full_prob.col(i).zeros();
                                full_prob(it->second, i) = 1.0;
                            }
                        }

                        probabilities = std::move(full_prob);
                    }
                }
            }
        } else {
            pred_ok = model->predict(predict_features, predictions);
        }

        if (!pred_ok) {
            return makeFailure(ClassificationStage::Predicting,
                               "Model prediction failed");
        }
        prediction_count = predictions.n_elem;

        // For non-sequence or unconstrained paths, raw_predictions wasn't set
        // above (GT substitution only happens in the constrained sequence path).
        if (raw_predictions.is_empty()) {
            raw_predictions = predictions;
        }

        // Try to get probability estimates (fallback for non-sequence models
        // or when per-segment probabilities were not computed above)
        if (!probabilities.has_value()) {
            arma::mat prob_mat;
            if (model->predictProbabilities(predict_features, prob_mat)) {
                probabilities = std::move(prob_mat);
            }
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

    // Attach cross-validation results (computed in Stage 5c)
    result.binary_cv_metrics = cv_binary_metrics;
    result.multi_class_cv_metrics = cv_multi_metrics;
    result.cv_folds_run = cv_folds_run;
    result.cv_per_fold_accuracy = std::move(cv_per_fold_accuracy);

    if (do_prediction && prediction_count > 0) {
        reportProgress(progress, ClassificationStage::ComputingMetrics,
                       "Computing metrics on " + std::to_string(prediction_count) +
                               " predictions");

        // Metrics use raw_predictions (before GT substitution) so they
        // reflect actual model performance rather than artificial 100%.
        if (config.prediction_region.predict_all_rows) {
            // Assemble ground-truth labels on the predicted rows for comparison
            try {
                AssembledLabels const pred_labels = std::visit(
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

                            } else if constexpr (std::is_same_v<T, LabelFromEvents>) {
                                auto event_series = dm.getData<DigitalEventSeries>(
                                        config.label_event_key);
                                auto time_frame = dm.getTime(TimeKey(config.time_key_str));
                                return assembleLabelsFromEvents(
                                        *event_series, *time_frame, predict_row_times, label_cfg);

                            } else {
                                static_assert(sizeof(T) == 0, "Unhandled label config variant");
                            }
                        },
                        config.label_config);

                // Helper: check if time t falls within any of the sorted intervals
                auto const isInIntervals =
                        [](std::int64_t t,
                           std::vector<Interval> const & sorted_ivs) -> bool {
                    for (auto const & iv: sorted_ivs) {
                        if (t >= iv.start && t <= iv.end) {
                            return true;
                        }
                        if (iv.start > t) {
                            break;
                        }
                    }
                    return false;
                };

                // Build sorted training intervals (for partitioning)
                std::vector<Interval> sorted_train_ivs;
                if (!config.training_interval_key.empty()) {
                    auto training_intervals = dm.getData<DigitalIntervalSeries>(
                            config.training_interval_key);
                    if (training_intervals && training_intervals->size() > 0) {
                        sorted_train_ivs.reserve(training_intervals->size());
                        for (auto const & iwid: training_intervals->view()) {
                            sorted_train_ivs.push_back(iwid.interval);
                        }
                        std::sort(sorted_train_ivs.begin(), sorted_train_ivs.end());
                    }
                }

                // Build sorted validation intervals (for partitioning)
                std::vector<Interval> sorted_val_ivs;
                if (!config.validation_interval_key.empty()) {
                    auto validation_intervals = dm.getData<DigitalIntervalSeries>(
                            config.validation_interval_key);
                    if (validation_intervals && validation_intervals->size() > 0) {
                        sorted_val_ivs.reserve(validation_intervals->size());
                        for (auto const & iwid: validation_intervals->view()) {
                            sorted_val_ivs.push_back(iwid.interval);
                        }
                        std::sort(sorted_val_ivs.begin(), sorted_val_ivs.end());
                    }
                }

                if (!sorted_train_ivs.empty()) {
                    // Partition frames into training / validation / other
                    std::vector<arma::uword> train_indices;
                    std::vector<arma::uword> val_indices;
                    train_indices.reserve(predict_row_times.size());
                    val_indices.reserve(predict_row_times.size());

                    for (std::size_t i = 0; i < predict_row_times.size(); ++i) {
                        auto const t = predict_row_times[i].getValue();
                        if (isInIntervals(t, sorted_train_ivs)) {
                            train_indices.push_back(static_cast<arma::uword>(i));
                        } else if (!sorted_val_ivs.empty() &&
                                   isInIntervals(t, sorted_val_ivs)) {
                            val_indices.push_back(static_cast<arma::uword>(i));
                        }
                        // else: frame is in neither region — no metrics
                    }

                    // Train metrics on raw predictions within training region
                    if (!train_indices.empty()) {
                        arma::uvec train_col_idx(train_indices.size());
                        for (std::size_t i = 0; i < train_indices.size(); ++i) {
                            train_col_idx[i] = train_indices[i];
                        }
                        arma::Row<std::size_t> const train_preds =
                                raw_predictions.cols(train_col_idx);
                        arma::Row<std::size_t> const train_truth =
                                pred_labels.labels.cols(train_col_idx);

                        if (labels.num_classes == 2) {
                            result.binary_train_metrics =
                                    computeBinaryMetrics(train_preds, train_truth);
                        }
                        result.multi_class_train_metrics =
                                computeMultiClassMetrics(train_preds, train_truth,
                                                         labels.num_classes);
                    }

                    // Validation metrics on raw predictions within validation region
                    if (!val_indices.empty()) {
                        arma::uvec val_col_idx(val_indices.size());
                        for (std::size_t i = 0; i < val_indices.size(); ++i) {
                            val_col_idx[i] = val_indices[i];
                        }
                        arma::Row<std::size_t> const val_preds =
                                raw_predictions.cols(val_col_idx);
                        arma::Row<std::size_t> const val_truth =
                                pred_labels.labels.cols(val_col_idx);

                        result.validation_observations = val_indices.size();

                        if (labels.num_classes == 2) {
                            result.binary_validation_metrics =
                                    computeBinaryMetrics(val_preds, val_truth);
                        }
                        result.multi_class_validation_metrics =
                                computeMultiClassMetrics(val_preds, val_truth,
                                                         labels.num_classes);
                    }
                } else {
                    // No training region — compute metrics on all predicted frames
                    if (labels.num_classes == 2) {
                        result.binary_train_metrics = computeBinaryMetrics(
                                raw_predictions, pred_labels.labels);
                    }
                    result.multi_class_train_metrics = computeMultiClassMetrics(
                            raw_predictions, pred_labels.labels, labels.num_classes);
                }

            } catch (...) {
                // Metrics are best-effort — don't fail the pipeline
            }
        }

        // ====================================================================
        // Stage 8: Write output
        // ====================================================================
        reportProgress(progress, ClassificationStage::WritingOutput,
                       "Writing predictions to DataManager");

        // Filter output to prediction intervals if specified
        arma::Row<std::size_t> output_predictions = predictions;
        std::optional<arma::mat> output_probabilities = probabilities;
        std::vector<TimeFrameIndex> output_times = predict_row_times;

        if (prediction_intervals && prediction_intervals->size() > 0) {
            auto filtered = filterPredictionsToIntervals(
                    predictions, probabilities, predict_row_times,
                    *prediction_intervals);
            output_predictions = std::move(filtered.predictions);
            output_probabilities = std::move(filtered.probabilities);
            output_times = std::move(filtered.times);
            result.prediction_observations = output_predictions.n_elem;
        }

        try {
            PredictionOutput pred_output;
            pred_output.class_predictions = std::move(output_predictions);
            pred_output.class_probabilities = std::move(output_probabilities);
            pred_output.prediction_times = std::move(output_times);

            if (config.defer_dm_writes) {
                // Store output for the caller to write on the main thread.
                // This avoids DataManager observer callbacks firing from
                // a background thread, which would be undefined behavior
                // for Qt widget observers.
                result.deferred_output = std::move(pred_output);
                result.deferred_output_config = config.output_config;
            } else {
                auto writer_result = writePredictions(
                        dm, pred_output, labels.class_names, config.output_config);
                result.writer_result = std::move(writer_result);
            }
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
