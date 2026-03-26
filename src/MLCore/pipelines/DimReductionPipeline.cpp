/**
 * @file DimReductionPipeline.cpp
 * @brief Implementation of the end-to-end dimensionality reduction pipeline
 */

#include "DimReductionPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "features/FeatureConverter.hpp"
#include "features/FeatureValidator.hpp"
#include "models/MLDimReductionOperation.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

// ============================================================================
// Stage names
// ============================================================================

std::string toString(DimReductionStage stage) {
    switch (stage) {
        case DimReductionStage::ValidatingFeatures:
            return "Validating features";
        case DimReductionStage::ConvertingFeatures:
            return "Converting features";
        case DimReductionStage::FittingAndTransforming:
            return "Fitting and transforming";
        case DimReductionStage::WritingOutput:
            return "Writing output";
        case DimReductionStage::Complete:
            return "Complete";
        case DimReductionStage::Failed:
            return "Failed";
    }
    return "Unknown";
}

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

void reportProgress(
        DimReductionProgressCallback const & cb,
        DimReductionStage stage,
        std::string const & msg) {
    if (cb) {
        cb(stage, msg);
    }
}

DimReductionPipelineResult makeFailure(
        DimReductionStage stage,
        std::string const & msg) {
    DimReductionPipelineResult result;
    result.success = false;
    result.error_message = msg;
    result.failed_stage = stage;
    return result;
}

/**
 * @brief Extract TimeFrameIndex values from a TensorData RowDescriptor
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
 * @brief Filter row times to keep only indices that survived NaN dropping
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
 * @brief Build output TensorData from reduced arma::mat, preserving row structure
 *
 * Handles TimeFrameIndex, Interval, and Ordinal row types, filtering rows
 * to match the valid indices after NaN dropping.
 */
std::shared_ptr<TensorData> buildOutputTensor(
        arma::mat const & reduced,
        TensorData const & input,
        std::vector<std::size_t> const & valid_row_indices,
        std::size_t n_components,
        std::vector<std::string> const & col_names) {

    // reduced is (n_components × observations) in mlpack convention.
    // Transpose to (observations × n_components) for the logical shape.
    arma::fmat output_fmat = arma::conv_to<arma::fmat>::from(reduced.t());
    auto const num_rows = static_cast<std::size_t>(output_fmat.n_rows);

    // Armadillo stores column-major, but TensorData factories expect row-major flat data.
    // Extract elements in row-major order.
    auto const n_cols = static_cast<std::size_t>(output_fmat.n_cols);
    std::vector<float> flat_data(output_fmat.n_elem);
    for (arma::uword r = 0; r < output_fmat.n_rows; ++r) {
        for (arma::uword c = 0; c < output_fmat.n_cols; ++c) {
            flat_data[r * n_cols + c] = output_fmat(r, c);
        }
    }

    auto const & row_desc = input.rows();

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            // Build a filtered TimeIndexStorage from valid indices
            auto all_row_times = extractRowTimes(input);
            auto filtered_times = filterRowTimes(all_row_times, valid_row_indices);

            auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(
                    std::move(filtered_times));

            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            flat_data,
                            num_rows,
                            n_components,
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
                            n_components,
                            std::move(filtered_intervals),
                            row_desc.timeFrame(),
                            col_names));
        }

        case RowType::Ordinal:
            return std::make_shared<TensorData>(
                    TensorData::createOrdinal2D(
                            flat_data,
                            num_rows,
                            n_components,
                            col_names));
    }

    return nullptr;
}

}// anonymous namespace

// ============================================================================
// Pipeline implementation
// ============================================================================

DimReductionPipelineResult runDimReductionPipeline(
        DataManager & dm,
        MLModelRegistry const & registry,
        DimReductionPipelineConfig const & config,
        DimReductionProgressCallback const & progress) {

    // ========================================================================
    // Stage 1: Validate features
    // ========================================================================
    reportProgress(progress, DimReductionStage::ValidatingFeatures,
                   "Validating feature tensor '" + config.feature_tensor_key + "'");

    auto feature_tensor = dm.getData<TensorData>(config.feature_tensor_key);
    if (!feature_tensor) {
        return makeFailure(DimReductionStage::ValidatingFeatures,
                           "Feature tensor '" + config.feature_tensor_key +
                                   "' not found in DataManager");
    }

    auto tensor_validation = validateFeatureTensor(*feature_tensor);
    if (!tensor_validation.valid) {
        return makeFailure(DimReductionStage::ValidatingFeatures,
                           "Feature tensor validation failed: " +
                                   tensor_validation.message);
    }

    if (!registry.hasModel(config.model_name)) {
        return makeFailure(DimReductionStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' not found in registry");
    }

    auto task_type = registry.getTaskType(config.model_name);
    if (task_type && *task_type != MLTaskType::DimensionalityReduction) {
        return makeFailure(DimReductionStage::ValidatingFeatures,
                           "Model '" + config.model_name +
                                   "' is not a dimensionality reduction model (task type: " +
                                   toString(*task_type) + ")");
    }

    // ========================================================================
    // Stage 2: Convert features (TensorData → arma::mat)
    // ========================================================================
    reportProgress(progress, DimReductionStage::ConvertingFeatures,
                   "Converting " + std::to_string(feature_tensor->numRows()) + "x" +
                           std::to_string(feature_tensor->numColumns()) + " tensor to arma::mat");

    ConvertedFeatures converted;
    try {
        converted = convertTensorToArma(*feature_tensor, config.conversion_config);
    } catch (std::exception const & e) {
        return makeFailure(DimReductionStage::ConvertingFeatures,
                           std::string("Feature conversion failed: ") + e.what());
    }

    if (converted.matrix.n_cols == 0) {
        return makeFailure(DimReductionStage::ConvertingFeatures,
                           "All rows were dropped during feature conversion "
                           "(all contain NaN/Inf values)");
    }

    // ========================================================================
    // Stage 3: Fit + Transform
    // ========================================================================
    reportProgress(progress, DimReductionStage::FittingAndTransforming,
                   "Running '" + config.model_name + "' on " +
                           std::to_string(converted.matrix.n_cols) + " observations x " +
                           std::to_string(converted.matrix.n_rows) + " features");

    auto model = registry.create(config.model_name);
    if (!model) {
        return makeFailure(DimReductionStage::FittingAndTransforming,
                           "Failed to create model '" + config.model_name + "'");
    }

    // Cast to DimReductionOperation
    auto * dim_op = dynamic_cast<MLDimReductionOperation *>(model.get());
    if (!dim_op) {
        return makeFailure(DimReductionStage::FittingAndTransforming,
                           "Model '" + config.model_name +
                                   "' does not implement MLDimReductionOperation");
    }

    arma::mat reduced;
    if (!dim_op->fitTransform(converted.matrix, config.model_params, reduced)) {
        return makeFailure(DimReductionStage::FittingAndTransforming,
                           "Dimensionality reduction failed for '" + config.model_name + "'");
    }

    auto const n_components = dim_op->outputDimensions();

    // Generate column names (PC1, PC2, ... for PCA; Dim1, Dim2, ... for others)
    std::vector<std::string> col_names;
    col_names.reserve(n_components);
    bool const is_pca = (config.model_name.find("PCA") != std::string::npos);
    for (std::size_t i = 0; i < n_components; ++i) {
        if (is_pca) {
            col_names.push_back("PC" + std::to_string(i + 1));
        } else {
            col_names.push_back("Dim" + std::to_string(i + 1));
        }
    }

    // ========================================================================
    // Stage 4: Write output
    // ========================================================================
    reportProgress(progress, DimReductionStage::WritingOutput,
                   "Creating output TensorData with " +
                           std::to_string(n_components) + " components");

    DimReductionPipelineResult result;
    result.success = true;
    result.num_observations = converted.matrix.n_cols;
    result.num_input_features = converted.matrix.n_rows;
    result.num_output_components = n_components;
    result.rows_dropped_nan = converted.rows_dropped;
    result.explained_variance_ratio = dim_op->explainedVarianceRatio();
    result.fitted_model = std::move(model);

    auto output_tensor = buildOutputTensor(
            reduced, *feature_tensor, converted.valid_row_indices,
            n_components, col_names);

    if (!output_tensor) {
        return makeFailure(DimReductionStage::WritingOutput,
                           "Failed to build output TensorData");
    }

    std::string const & output_key = config.output_config.output_key;
    TimeKey const time_key = dm.getTimeKey(config.feature_tensor_key);

    if (config.defer_dm_writes) {
        result.deferred_output = std::move(output_tensor);
        result.deferred_output_key = output_key;
        result.deferred_time_key = time_key.str();
    } else {
        dm.setData(output_key, output_tensor, time_key);
        result.output_key = output_key;
    }

    reportProgress(progress, DimReductionStage::Complete,
                   "Pipeline completed successfully");

    return result;
}

}// namespace MLCore
