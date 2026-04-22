/**
 * @file TensorWhitening.cpp
 * @brief Implementation of robust ZCA whitening for TensorData.
 *
 * The whitening algorithm operates on a 2D time x channel tensor, but only on
 * the subset of channels that are not listed in TensorWhiteningParams::exclude_channels.
 * Excluded channels are carried through unchanged in the output and do not
 * contribute to any whitening statistics.
 *
 * High-level algorithm:
 * 1. Resolve the included channel subset by removing excluded channels from the
 *    full input matrix. The remaining steps work on this reduced channel set.
 * 2. Compute the per-channel median across time. A standard exact median is a
 *    sort-based reduction and is embarrassingly parallel across channels.
 * 3. Subtract the per-channel median to form centered voltages. This is a
 *    time x included-channel working set and is one of the dominant temporary
 *    allocations unless the centering step can be written in-place into an
 *    output or scratch buffer.
 * 4. Compute the per-channel MAD from the absolute centered voltages. This can
 *    require another large temporary for abs(centered). Alternative MAD
 *    estimators may reduce temporary pressure.
 * 5. Build a clean-sample mask over time. For each sample, require all included
 *    channels to satisfy |centered| <= MAD_multiplier * 1.4826 * MAD. This step
 *    is embarrassingly parallel across time once the channel thresholds are
 *    known.
 * 6. Estimate the covariance of the centered voltages using only clean samples
 *    when the MAD mask is enabled. Some backends may need to compact masked rows
 *    into a temporary matrix because masked covariance is not always available
 *    as a primitive.
 * 7. Eigendecompose the included-channel covariance matrix, which is small
 *    (channel x channel) relative to the voltage matrix.
 * 8. Form the ZCA operator W = V (Lambda + epsilon I)^(-1/2) V^T.
 * 9. Project the centered voltages with W. This is another large matrix
 *    multiply over the included channels.
 * 10. Optionally rescale the whitened voltages by a robust amplitude statistic.
 *     This is naturally an in-place multiply.
 * 11. Write the whitened included channels back into the full output tensor,
 *     leaving excluded channels unchanged.
 *
 * Memory-sensitive stages are therefore the centered-voltage buffer, the
 * absolute-value / MAD buffer, any row-compacted masked covariance source, and
 * the final projection buffer. The channel-subset semantics matter here: the
 * working matrix is conceptually a reduced view of the full input, while the
 * persisted output still has the full original channel set.
 */

#include "TensorWhitening.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/TensorStorageBase.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifdef TENSOR_BACKEND_LIBTORCH
#include "Tensors/storage/LibTorchTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"

#pragma push_macro("CHECK")
#include <torch/torch.h>
#pragma pop_macro("CHECK")
#endif

namespace WhiskerToolbox::Transforms::V2::Examples {

#ifdef TENSOR_BACKEND_LIBTORCH

namespace {

auto validateIncludedChannels(
        std::vector<int> const & excluded_channels,
        std::size_t num_cols,
        ComputeContext const & ctx,
        std::vector<int64_t> & included_channels) -> bool {
    std::vector<bool> excluded(num_cols, false);
    for (int const channel_index: excluded_channels) {
        if (channel_index < 0 || static_cast<std::size_t>(channel_index) >= num_cols) {
            ctx.logMessage(
                    "TensorWhitening: exclude_channels contains out-of-range channel index " +
                    std::to_string(channel_index));
            return false;
        }
        excluded[static_cast<std::size_t>(channel_index)] = true;
    }

    included_channels.clear();
    included_channels.reserve(num_cols);
    for (std::size_t channel_index = 0; channel_index < num_cols; ++channel_index) {
        if (!excluded[channel_index]) {
            included_channels.push_back(static_cast<int64_t>(channel_index));
        }
    }

    if (included_channels.empty()) {
        ctx.logMessage("TensorWhitening: all channels are excluded");
        return false;
    }

    return true;
}

auto medianAlongDim(
        torch::Tensor const & values,
        int64_t dim,
        bool keepdim) -> torch::Tensor {
    auto sorted = std::get<0>(values.sort(dim));
    auto const axis_size = sorted.size(dim);

    torch::Tensor median;
    if ((axis_size % 2) == 0) {
        auto const upper_index = axis_size / 2;
        auto const lower_index = upper_index - 1;
        auto lower = sorted.narrow(dim, lower_index, 1);
        auto upper = sorted.narrow(dim, upper_index, 1);
        median = (lower + upper) * 0.5;
    } else {
        median = sorted.narrow(dim, axis_size / 2, 1);
    }

    if (!keepdim) {
        median = median.squeeze(dim);
    }
    return median;
}

auto buildResultFromTorch(
        torch::Tensor const & result_tensor,
        TensorData const & input,
        std::vector<std::string> const & column_names) -> std::shared_ptr<TensorData> {
    assert(result_tensor.device().is_cpu());
    assert(result_tensor.scalar_type() == torch::kFloat32);

    auto const & row_desc = input.rows();
    auto storage = TensorStorageWrapper{
            LibTorchTensorStorage{result_tensor.contiguous()}};

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex:
            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2DFromStorage(
                            std::move(storage),
                            row_desc.timeStoragePtr(),
                            input.getTimeFrame(),
                            column_names));

        case RowType::Interval: {
            auto const interval_span = row_desc.intervals();
            std::vector<TimeFrameInterval> intervals(
                    interval_span.begin(), interval_span.end());
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervalsFromStorage(
                            std::move(storage),
                            std::move(intervals),
                            input.getTimeFrame(),
                            column_names));
        }

        case RowType::Ordinal:
        default:
            return std::make_shared<TensorData>(
                    TensorData::createOrdinal2DFromStorage(
                            std::move(storage),
                            input.getTimeFrame(),
                            column_names));
    }
}

auto tensorWhiteningLibTorch(
        TensorData const & input,
        TensorWhiteningParams const & params,
        ComputeContext const & ctx,
        bool use_cuda) -> std::shared_ptr<TensorData> {
    auto const num_cols = input.numColumns();

    std::vector<int64_t> included_channels;
    if (!validateIncludedChannels(
                params.exclude_channels, num_cols, ctx, included_channels)) {
        return nullptr;
    }

    auto const * libtorch_storage =
            input.storage().getAsChecked<LibTorchTensorStorage>(
                    TensorStorageType::LibTorch);

    std::optional<TensorData> converted;
    torch::Device device = torch::kCPU;
    if (use_cuda) {
        device = torch::Device(torch::kCUDA);
        ctx.logMessage("TensorWhitening: using CUDA device");
    }

    torch::Tensor input_tensor;
    if (libtorch_storage != nullptr) {
        input_tensor = libtorch_storage->tensor();
        if (input_tensor.device() != device) {
            input_tensor = input_tensor.to(device);
        }
    } else if (use_cuda) {
        converted.emplace(input.toLibTorchStrided());
        libtorch_storage = converted->storage().getAsChecked<LibTorchTensorStorage>(
                TensorStorageType::LibTorch);
        assert(libtorch_storage != nullptr);
        input_tensor = libtorch_storage->tensor().to(device).contiguous();
    } else {
        converted.emplace(input.toLibTorch());
        libtorch_storage = converted->storage().getAsChecked<LibTorchTensorStorage>(
                TensorStorageType::LibTorch);
        assert(libtorch_storage != nullptr);
        input_tensor = libtorch_storage->tensor();
    }

    if (!input_tensor.is_contiguous()) {
        input_tensor = input_tensor.contiguous();
    }
    if (input_tensor.scalar_type() != torch::kFloat32) {
        input_tensor = input_tensor.to(torch::kFloat32);
    }

    ctx.reportProgress(20);
    if (ctx.shouldCancel()) {
        return nullptr;
    }

    auto channel_index_tensor = torch::tensor(
            included_channels,
            torch::TensorOptions().dtype(torch::kLong).device(device));

    if (!torch::isfinite(input_tensor).all().item<bool>()) {
        ctx.logMessage("TensorWhitening: input contains NaN or Inf values");
        return nullptr;
    }

    auto result = input_tensor.clone();
    auto included = input_tensor.index_select(/*dim=*/1, channel_index_tensor)
                            .to(torch::kFloat64);
    auto medians = medianAlongDim(included, /*dim=*/0, /*keepdim=*/true);
    auto centered = included - medians;
    included.reset();
    medians.reset();

    ctx.reportProgress(35);
    if (ctx.shouldCancel()) {
        return nullptr;
    }

    auto robust_std = medianAlongDim(centered.abs(), /*dim=*/0, /*keepdim=*/true) * 1.4826;
    torch::Tensor covariance_source = centered;
    if (params.mad_threshold_multiplier > 0.0F) {
        auto thresholds = robust_std.clamp_min(static_cast<double>(params.epsilon)) *
                          static_cast<double>(params.mad_threshold_multiplier);
        auto clean_mask = (centered.abs() <= thresholds).all(/*dim=*/1);
        auto const clean_count = clean_mask.sum().item<int64_t>();
        if (clean_count >= 2) {
            covariance_source = centered.index({clean_mask});
        } else {
            ctx.logMessage(
                    "TensorWhitening: fewer than 2 clean samples remain; using all samples for covariance");
        }
    }

    ctx.reportProgress(55);
    if (ctx.shouldCancel()) {
        return nullptr;
    }

    auto const sample_count = covariance_source.size(0);
    if (sample_count < 2) {
        ctx.logMessage("TensorWhitening: need at least 2 samples to estimate covariance");
        return nullptr;
    }

    auto covariance = covariance_source.transpose(0, 1).matmul(covariance_source) /
                      static_cast<double>(sample_count - 1);
    auto eigh = torch::linalg_eigh(covariance);
    auto eigenvalues = std::get<0>(eigh);
    auto eigenvectors = std::get<1>(eigh);
    auto inv_sqrt = torch::rsqrt(
            eigenvalues.clamp_min(0.0) + static_cast<double>(params.epsilon));
    auto whitening_matrix = eigenvectors.matmul(torch::diag(inv_sqrt))
                                    .matmul(eigenvectors.transpose(0, 1));

    auto whitened = centered.matmul(whitening_matrix);
    if (params.rescale_amplitude) {
        whitened = whitened * robust_std.mean();
    }
    centered.reset();
    covariance_source.reset();
    robust_std.reset();
    covariance.reset();
    eigenvalues.reset();
    eigenvectors.reset();
    inv_sqrt.reset();
    whitening_matrix.reset();

    ctx.reportProgress(80);
    if (ctx.shouldCancel()) {
        return nullptr;
    }

    using torch::indexing::Slice;
    result.index_put_({Slice(), channel_index_tensor}, whitened.to(torch::kFloat32));
    whitened.reset();
    input_tensor.reset();
    channel_index_tensor.reset();

    if (result.device().type() == torch::kCUDA) {
        result = result.to(torch::kCPU);
    }
    result = result.contiguous();

    ctx.reportProgress(100);

    auto const & source_column_names = input.columnNames();
    std::vector<std::string> const column_names(
            source_column_names.begin(), source_column_names.end());
    return buildResultFromTorch(result, input, column_names);
}

}// namespace

#endif// TENSOR_BACKEND_LIBTORCH

auto tensorWhitening(
        TensorData const & input,
        TensorWhiteningParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {
    if (input.ndim() != 2) {
        ctx.logMessage(
                "TensorWhitening: input must be 2D, got " +
                std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    if (input.numRows() < 2) {
        ctx.logMessage("TensorWhitening: input must contain at least 2 rows");
        return nullptr;
    }

    if (input.numColumns() == 0) {
        ctx.logMessage("TensorWhitening: input must contain at least 1 channel");
        return nullptr;
    }

    if (params.epsilon <= 0.0F) {
        ctx.logMessage("TensorWhitening: epsilon must be positive");
        return nullptr;
    }

#ifdef TENSOR_BACKEND_LIBTORCH
    bool const use_cuda = params.use_gpu && torch::cuda::is_available();
    if (params.use_gpu && !use_cuda) {
        ctx.logMessage("TensorWhitening: CUDA not available; using LibTorch CPU");
    }
    return tensorWhiteningLibTorch(input, params, ctx, use_cuda);
#else
    (void) params;
    ctx.logMessage(
            "TensorWhitening: LibTorch backend is required; rebuild with TENSOR_BACKEND_LIBTORCH");
    return nullptr;
#endif
}

}// namespace WhiskerToolbox::Transforms::V2::Examples