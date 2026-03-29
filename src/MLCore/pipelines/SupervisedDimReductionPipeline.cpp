/**
 * @file SupervisedDimReductionPipeline.cpp
 * @brief Implementation of the supervised dimensionality reduction pipeline
 */

#include "SupervisedDimReductionPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "features/FeatureConverter.hpp"
#include "features/FeatureValidator.hpp"
#include "features/LabelAssembler.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLSupervisedDimReductionOperation.hpp"
#include "models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace MLCore {

// ============================================================================
// Stage names
// ============================================================================

std::string toString(SupervisedDimReductionStage stage) {
    switch (stage) {
        case SupervisedDimReductionStage::ValidatingFeatures:
            return "Validating features";
        case SupervisedDimReductionStage::ConvertingFeatures:
            return "Converting features";
        case SupervisedDimReductionStage::AssemblingLabels:
            return "Assembling labels";
        case SupervisedDimReductionStage::FittingAndTransforming:
            return "Fitting and transforming";
        case SupervisedDimReductionStage::WritingOutput:
            return "Writing output";
        case SupervisedDimReductionStage::Complete:
            return "Complete";
        case SupervisedDimReductionStage::Failed:
            return "Failed";
    }
    return "Unknown";
}

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

void reportProgress(
        SupervisedDimReductionProgressCallback const & cb,
        SupervisedDimReductionStage stage,
        std::string const & msg) {
    if (cb) {
        cb(stage, msg);
    }
}

SupervisedDimReductionPipelineResult makeFailure(
        SupervisedDimReductionStage stage,
        std::string const & msg) {
    SupervisedDimReductionPipelineResult result;
    result.success = false;
    result.error_message = msg;
    result.failed_stage = stage;
    return result;
}

/// Extract TimeFrameIndex values from a TensorData RowDescriptor.
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

/// Return only those entries of all_times whose index is in valid_indices.
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
 * @brief Build a list of column indices for labeled-only rows
 *
 * When the label assembler returns a sentinel (num_classes) for unlabeled rows,
 * this function collects the column indices of rows that have a valid label.
 * For interval-based labeling, all rows have labels (no sentinel), so this
 * returns all indices.
 */
std::vector<arma::uword> collectLabeledCols(
        arma::Row<std::size_t> const & labels,
        std::size_t num_classes) {
    std::vector<arma::uword> cols;
    cols.reserve(labels.n_elem);
    for (arma::uword i = 0; i < labels.n_elem; ++i) {
        if (labels[i] < num_classes) {
            cols.push_back(i);
        }
    }
    return cols;
}

/**
 * @brief Build the output TensorData from a reduced (C × N) arma::mat
 *
 * Mirrors the implementation in DimReductionPipeline: handles
 * TimeFrameIndex, Interval, and Ordinal row types, preserving only
 * rows that survived NaN conversion.
 */
std::shared_ptr<TensorData> buildOutputTensor(
        arma::mat const & reduced,
        TensorData const & input,
        std::vector<std::size_t> const & valid_row_indices,
        std::size_t n_dims,
        std::vector<std::string> const & col_names) {

    // reduced is (n_dims × N) in mlpack convention; transpose to (N × n_dims).
    arma::fmat output_fmat = arma::conv_to<arma::fmat>::from(reduced.t());
    auto const num_rows = static_cast<std::size_t>(output_fmat.n_rows);
    auto const n_cols = static_cast<std::size_t>(output_fmat.n_cols);

    // Convert to row-major flat data for TensorData factory methods.
    std::vector<float> flat_data(output_fmat.n_elem);
    for (arma::uword r = 0; r < output_fmat.n_rows; ++r) {
        for (arma::uword c = 0; c < output_fmat.n_cols; ++c) {
            flat_data[r * n_cols + c] = output_fmat(r, c);
        }
    }

    auto const & row_desc = input.rows();

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            auto all_row_times = extractRowTimes(input);
            auto filtered_times = filterRowTimes(all_row_times, valid_row_indices);

            auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(
                    std::move(filtered_times));

            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            flat_data,
                            num_rows,
                            n_dims,
                            time_storage,
                            row_desc.timeFrame(),
                            col_names));
        }

        case RowType::Interval: {
            auto const & all_intervals = row_desc.intervals();
            std::vector<TimeFrameInterval> filtered_intervals;
            filtered_intervals.reserve(valid_row_indices.size());
            for (auto idx: valid_row_indices) {
                filtered_intervals.push_back(all_intervals[idx]);
            }
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            flat_data,
                            num_rows,
                            n_dims,
                            std::move(filtered_intervals),
                            row_desc.timeFrame(),
                            col_names));
        }

        case RowType::Ordinal:
            return std::make_shared<TensorData>(
                    TensorData::createOrdinal2D(
                            flat_data,
                            num_rows,
                            n_dims,
                            col_names));
    }

    return nullptr;
}

}// anonymous namespace

// ============================================================================
// Pipeline implementation
// ============================================================================

SupervisedDimReductionPipelineResult runSupervisedDimReductionPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        SupervisedDimReductionPipelineConfig const & config,
        SupervisedDimReductionProgressCallback const & progress) {

    // ========================================================================
    // Stage 1: Validate features
    // ========================================================================
    reportProgress(progress, SupervisedDimReductionStage::ValidatingFeatures,
                   "Validating feature tensor '" + config.feature_tensor_key + "'");

    auto feature_tensor = dm.getData<TensorData>(config.feature_tensor_key);
    if (!feature_tensor) {
        return makeFailure(SupervisedDimReductionStage::ValidatingFeatures,
                           "Feature tensor '" + config.feature_tensor_key +
                                   "' not found in DataManager");
    }

    auto tensor_validation = validateFeatureTensor(*feature_tensor);
    if (!tensor_validation.valid) {
        return makeFailure(SupervisedDimReductionStage::ValidatingFeatures,
                           "Feature tensor validation failed: " +
                                   tensor_validation.message);
    }

    // Label assembly requires time-indexed rows for alignment.
    if (feature_tensor->rows().type() != RowType::TimeFrameIndex) {
        return makeFailure(SupervisedDimReductionStage::ValidatingFeatures,
                           "Feature tensor must have RowType::TimeFrameIndex rows "
                           "for supervised dimensionality reduction");
    }

    if (!registry.hasModel(config.model_name)) {
        return makeFailure(SupervisedDimReductionStage::ValidatingFeatures,
                           "Model '" + config.model_name + "' not found in registry");
    }

    auto task_type = registry.getTaskType(config.model_name);
    if (task_type && *task_type != MLTaskType::SupervisedDimensionalityReduction) {
        return makeFailure(SupervisedDimReductionStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' is not a supervised dimensionality reduction model "
                                   "(task type: " +
                                   toString(*task_type) + ")");
    }

    // Extract all row times for label alignment.
    auto all_row_times = extractRowTimes(*feature_tensor);

    // ========================================================================
    // Stage 2: Convert features
    // ========================================================================
    reportProgress(progress, SupervisedDimReductionStage::ConvertingFeatures,
                   "Converting " + std::to_string(feature_tensor->numRows()) + "×" +
                           std::to_string(feature_tensor->numColumns()) +
                           " tensor to arma::mat");

    ConvertedFeatures converted;
    try {
        converted = convertTensorToArma(*feature_tensor, config.conversion_config);
    } catch (std::exception const & e) {
        return makeFailure(SupervisedDimReductionStage::ConvertingFeatures,
                           std::string("Feature conversion failed: ") + e.what());
    }

    if (converted.matrix.n_cols == 0) {
        return makeFailure(SupervisedDimReductionStage::ConvertingFeatures,
                           "All rows were dropped during feature conversion "
                           "(all contain NaN/Inf values)");
    }

    // valid_row_times corresponds 1:1 with columns of converted.matrix.
    auto valid_row_times = filterRowTimes(all_row_times, converted.valid_row_indices);

    // ========================================================================
    // Stage 3: Assemble labels
    // ========================================================================
    reportProgress(progress, SupervisedDimReductionStage::AssemblingLabels,
                   "Assembling labels for " +
                           std::to_string(valid_row_times.size()) + " observations");

    AssembledLabels labels;
    try {
        labels = std::visit(
                [&](auto const & label_cfg) -> AssembledLabels {
                    using T = std::decay_t<decltype(label_cfg)>;

                    if constexpr (std::is_same_v<T, LabelFromIntervals>) {
                        auto intervals =
                                dm.getData<DigitalIntervalSeries>(config.label_interval_key);
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
                        auto event_series =
                                dm.getData<DigitalEventSeries>(config.label_event_key);
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
        return makeFailure(SupervisedDimReductionStage::AssemblingLabels,
                           std::string("Label assembly failed: ") + e.what());
    }

    if (labels.num_classes < 2) {
        return makeFailure(SupervisedDimReductionStage::AssemblingLabels,
                           "Need at least 2 classes for supervised dim reduction, got " +
                                   std::to_string(labels.num_classes));
    }

    // Extract the subset of rows that have valid labels for training.
    // Group-based modes mark unlabeled rows with sentinel = num_classes.
    auto labeled_cols = collectLabeledCols(labels.labels, labels.num_classes);

    if (labeled_cols.empty()) {
        return makeFailure(SupervisedDimReductionStage::AssemblingLabels,
                           "No labeled observations found — all rows are unlabeled");
    }

    arma::mat train_features;
    arma::Row<std::size_t> train_labels;

    if (labels.unlabeled_count > 0) {
        // Subset to labeled rows only for training.
        arma::uvec col_uword(labeled_cols.size());
        for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
            col_uword[i] = labeled_cols[i];
        }
        train_features = converted.matrix.cols(col_uword);

        train_labels.set_size(labeled_cols.size());
        for (std::size_t i = 0; i < labeled_cols.size(); ++i) {
            train_labels[i] = labels.labels[labeled_cols[i]];
        }
    } else {
        // All rows are labeled — use the full converted matrix.
        train_features = converted.matrix;
        train_labels = labels.labels;
    }

    // ========================================================================
    // Stage 4: Fit + Transform
    // ========================================================================
    reportProgress(progress, SupervisedDimReductionStage::FittingAndTransforming,
                   "Fitting '" + config.model_name + "' on " +
                           std::to_string(train_features.n_cols) + " labeled observations × " +
                           std::to_string(train_features.n_rows) + " features");

    auto model = registry.create(config.model_name);
    if (!model) {
        return makeFailure(SupervisedDimReductionStage::FittingAndTransforming,
                           "Failed to create model '" + config.model_name + "'");
    }

    auto * sup_op = dynamic_cast<MLSupervisedDimReductionOperation *>(model.get());
    if (!sup_op) {
        return makeFailure(SupervisedDimReductionStage::FittingAndTransforming,
                           "Model '" + config.model_name +
                                   "' does not implement MLSupervisedDimReductionOperation");
    }

    // Fit on labeled rows (training).
    arma::mat train_logits;
    if (!sup_op->fitTransform(train_features, train_labels, config.model_params,
                              train_logits)) {
        return makeFailure(SupervisedDimReductionStage::FittingAndTransforming,
                           "Supervised dim reduction fitting failed for '" +
                                   config.model_name + "'");
    }

    // Transform ALL valid rows (labeled + unlabeled) for the complete output.
    arma::mat all_logits;
    if (!sup_op->transform(converted.matrix, all_logits)) {
        return makeFailure(SupervisedDimReductionStage::FittingAndTransforming,
                           "Supervised dim reduction transform failed for '" +
                                   config.model_name + "'");
    }

    std::size_t const n_dims = sup_op->outputDimensions();

    // Build column names using actual class names: "Logit:<ClassName>".
    std::vector<std::string> col_names;
    col_names.reserve(n_dims);
    for (auto const & class_name: labels.class_names) {
        col_names.push_back("Logit:" + class_name);
    }

    // ========================================================================
    // Stage 5: Write output
    // ========================================================================
    reportProgress(progress, SupervisedDimReductionStage::WritingOutput,
                   "Creating output TensorData: " +
                           std::to_string(converted.matrix.n_cols) + " rows × " +
                           std::to_string(n_dims) + " dimensions");

    auto output_tensor = buildOutputTensor(
            all_logits,
            *feature_tensor,
            converted.valid_row_indices,
            n_dims,
            col_names);

    if (!output_tensor) {
        return makeFailure(SupervisedDimReductionStage::WritingOutput,
                           "Failed to build output TensorData");
    }

    SupervisedDimReductionPipelineResult result;
    result.success = true;
    result.num_observations = converted.matrix.n_cols;
    result.num_training_observations = train_features.n_cols;
    result.num_input_features = converted.matrix.n_rows;
    result.num_output_dimensions = n_dims;
    result.rows_dropped_nan = converted.rows_dropped;
    result.unlabeled_count = labels.unlabeled_count;
    result.num_classes = labels.num_classes;
    result.class_names = labels.class_names;
    result.fitted_model = std::move(model);

    TimeKey const time_key = dm.getTimeKey(config.feature_tensor_key);

    if (config.defer_dm_writes) {
        result.deferred_output = std::move(output_tensor);
        result.deferred_output_key = config.output_key;
        result.deferred_time_key = time_key.str();
    } else {
        dm.setData(config.output_key, output_tensor, time_key);
        result.output_key = config.output_key;
    }

    reportProgress(progress, SupervisedDimReductionStage::Complete,
                   "Pipeline completed: " + std::to_string(n_dims) +
                           " dimensions from " + std::to_string(labels.num_classes) +
                           " classes");

    return result;
}

}// namespace MLCore
