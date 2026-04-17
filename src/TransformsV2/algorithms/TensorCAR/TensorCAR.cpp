/**
 * @file TensorCAR.cpp
 * @brief Implementation of Common Average Reference container transform
 */

#include "TensorCAR.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/TensorStorageBase.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <armadillo>

#include <cassert>
#include <cstddef>
#include <set>
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

/**
 * @brief Build a TensorData result from a CPU torch::Tensor preserving the
 *        RowDescriptor and column names from the original input.
 *
 * @pre result_tensor is contiguous, float32, on CPU, shape [num_rows, num_cols]
 */
auto buildResultFromTorch(
        torch::Tensor result_tensor,
        TensorData const & input,
        std::vector<std::string> const & col_names) -> std::shared_ptr<TensorData> {

    auto const & row_desc = input.rows();
    auto const num_rows = input.numRows();
    auto const num_cols = input.numColumns();

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            // Zero-copy: wrap the torch::Tensor in LibTorchTensorStorage
            auto storage = TensorStorageWrapper{
                    LibTorchTensorStorage{std::move(result_tensor)}};
            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2DFromStorage(
                            std::move(storage),
                            row_desc.timeStoragePtr(),
                            row_desc.timeFrame(),
                            std::vector<std::string>(col_names)));
        }
        case RowType::Interval: {
            // Materialize to flat vector (single memcpy from contiguous tensor)
            auto const * ptr = result_tensor.data_ptr<float>();
            std::vector<float> const flat(ptr, ptr + num_rows * num_cols);
            auto const intervals_span = row_desc.intervals();
            std::vector<TimeFrameInterval> intervals(
                    intervals_span.begin(), intervals_span.end());
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            flat,
                            num_rows,
                            num_cols,
                            std::move(intervals),
                            row_desc.timeFrame(),
                            std::vector<std::string>(col_names)));
        }
        case RowType::Ordinal:
        default: {
            // Materialize to flat vector, build arma::fmat, use createFromArmadillo
            auto const * ptr = result_tensor.data_ptr<float>();
            arma::fmat result_arma(num_rows, num_cols);
            for (std::size_t r = 0; r < num_rows; ++r) {
                for (std::size_t c = 0; c < num_cols; ++c) {
                    result_arma(r, c) = ptr[r * num_cols + c];
                }
            }
            return std::make_shared<TensorData>(
                    TensorData::createFromArmadillo(
                            std::move(result_arma),
                            std::vector<std::string>(col_names)));
        }
    }
}

/**
 * @brief LibTorch compute path for Common Average Reference.
 *
 * If the input is already LibTorch-backed, uses its tensor directly (no
 * conversion). Otherwise converts via toLibTorch(). Optionally moves to
 * CUDA when use_gpu is true.
 *
 * @param input      2D TensorData
 * @param params     CAR parameters
 * @param ctx        Compute context
 * @param use_cuda   Whether to attempt CUDA device transfer
 */
auto tensorCARLibTorch(
        TensorData const & input,
        TensorCARParams const & params,
        ComputeContext const & ctx,
        bool use_cuda) -> std::shared_ptr<TensorData> {

    auto const num_cols = input.numColumns();

    // Obtain a torch::Tensor from the input.
    // If already LibTorch-backed, use it directly (zero-copy handle).
    // For CUDA: use toLibTorchStrided() to get a non-contiguous column-major
    // view, then upload + make contiguous on GPU (transpose at ~900 GB/s GPU
    // bandwidth instead of ~30 GB/s CPU bandwidth).
    // For CPU: use toLibTorch() which returns a contiguous row-major tensor.
    auto const * lt_storage =
            input.storage().getAsChecked<LibTorchTensorStorage>(
                    TensorStorageType::LibTorch);

    // Keep converted TensorData alive if we need to create one
    std::optional<TensorData> converted;
    torch::Tensor t;

    auto device = torch::kCPU;
    bool const cuda_available = use_cuda && torch::cuda::is_available();
    if (use_cuda && !cuda_available) {
        ctx.logMessage("TensorCAR: CUDA not available; using LibTorch CPU");
    }

    if (lt_storage) {
        // Already LibTorch-backed — zero-copy handle
        t = lt_storage->tensor();
        if (cuda_available) {
            device = torch::kCUDA;
            t = t.to(device);
        }
    } else if (cuda_available) {
        // Armadillo/Dense → strided column-major view → upload to GPU →
        // make contiguous on GPU (transpose runs on GPU memory bandwidth).
        device = torch::kCUDA;
        converted.emplace(input.toLibTorchStrided());
        lt_storage = converted->storage().getAsChecked<LibTorchTensorStorage>(
                TensorStorageType::LibTorch);
        assert(lt_storage && "toLibTorchStrided() must produce LibTorchTensorStorage");
        t = lt_storage->tensor().to(device).contiguous();
        ctx.logMessage("TensorCAR: Using CUDA device");
    } else {
        // CPU path: contiguous row-major tensor
        converted.emplace(input.toLibTorch());
        lt_storage = converted->storage().getAsChecked<LibTorchTensorStorage>(
                TensorStorageType::LibTorch);
        assert(lt_storage && "toLibTorch() must produce LibTorchTensorStorage");
        t = lt_storage->tensor();
    }

    ctx.reportProgress(20);

    // Build column mask for excluded channels
    std::set<int> const excluded_set(
            params.exclude_channels.begin(),
            params.exclude_channels.end());

    bool const all_included = excluded_set.empty() ||
                              excluded_set.size() >= num_cols;

    // Compute reference signal: shape [num_rows, 1]
    torch::Tensor ref;
    if (all_included) {
        if (params.method == CARMethod::Mean) {
            ref = t.mean(/*dim=*/1, /*keepdim=*/true);
        } else {
            ref = std::get<0>(t.median(/*dim=*/1, /*keepdim=*/true));
        }
    } else {
        // Build index tensor for included columns
        std::vector<int64_t> included_indices;
        included_indices.reserve(num_cols);
        for (std::size_t c = 0; c < num_cols; ++c) {
            if (excluded_set.find(static_cast<int>(c)) == excluded_set.end()) {
                included_indices.push_back(static_cast<int64_t>(c));
            }
        }
        auto idx = torch::tensor(included_indices, torch::kLong).to(device);
        auto ref_source = t.index_select(/*dim=*/1, idx);
        if (params.method == CARMethod::Mean) {
            ref = ref_source.mean(/*dim=*/1, /*keepdim=*/true);
        } else {
            ref = std::get<0>(ref_source.median(/*dim=*/1, /*keepdim=*/true));
        }
    }

    ctx.reportProgress(50);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // Subtract reference from all channels
    torch::Tensor result = t - ref;

    ctx.reportProgress(80);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // Download to CPU if on CUDA
    if (result.device().type() == torch::kCUDA) {
        result = result.to(torch::kCPU);

        // Release intermediate CUDA tensor handles so the caching allocator
        // can reuse the GPU memory on subsequent transforms.
        t.reset();
        ref.reset();
        // NOTE: Do NOT call emptyCache() here. The caching allocator reuses
        // freed blocks instantly; flushing forces expensive cudaMalloc on
        // the next run (5-15s for multi-GB allocations).
    }

    // Preserve column names
    auto const & src_col_names = input.columnNames();
    std::vector<std::string> const col_names(src_col_names.begin(), src_col_names.end());

    ctx.reportProgress(100);

    return buildResultFromTorch(std::move(result), input, col_names);
}

}// namespace

#endif// TENSOR_BACKEND_LIBTORCH

auto tensorCAR(
        TensorData const & input,
        TensorCARParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // --- Validation ---
    if (input.ndim() != 2) {
        ctx.logMessage("TensorCAR: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows == 0) {
        ctx.logMessage("TensorCAR: Input has no rows");
        return nullptr;
    }

    if (num_cols == 0) {
        ctx.logMessage("TensorCAR: Input has no channels");
        return nullptr;
    }

    // --- Backend dispatch ---
    // Detect storage type and route to the matching compute path to avoid
    // unnecessary conversions. use_gpu only controls CUDA vs CPU within
    // the LibTorch path.
    auto const storage_type = input.storage().getStorageType();

#ifdef TENSOR_BACKEND_LIBTORCH
    // If data is already LibTorch-backed, always use the LibTorch path
    // (avoids expensive LibTorch → Armadillo materialization)
    if (storage_type == TensorStorageType::LibTorch) {
        bool const use_cuda = params.use_gpu;
        return tensorCARLibTorch(input, params, ctx, use_cuda);
    }
    // If user explicitly requests GPU, convert to LibTorch
    if (params.use_gpu) {
        return tensorCARLibTorch(input, params, ctx, /*use_cuda=*/true);
    }
#else
    if (params.use_gpu) {
        ctx.logMessage("TensorCAR: use_gpu=true but LibTorch not available; "
                       "falling back to CPU (Armadillo).");
    }
#endif
    // For non-Armadillo, non-LibTorch backends (Dense, Lazy, Mmap, etc.),
    // materialize to Armadillo and use the Armadillo CPU path.
    (void) storage_type;

    ctx.reportProgress(0);

    // --- Build included-column index set ---
    // Excluded channels are not used when computing the reference, but
    // the reference is still subtracted from them.
    std::set<int> const excluded_set(
            params.exclude_channels.begin(),
            params.exclude_channels.end());

    std::vector<arma::uword> included_col_indices;
    included_col_indices.reserve(num_cols);
    for (std::size_t c = 0; c < num_cols; ++c) {
        if (excluded_set.find(static_cast<int>(c)) == excluded_set.end()) {
            included_col_indices.push_back(static_cast<arma::uword>(c));
        }
    }

    if (included_col_indices.empty()) {
        ctx.logMessage("TensorCAR: All channels are excluded — no reference to compute");
        return nullptr;
    }

    // --- Retrieve Armadillo matrix ---
    // If already Armadillo-backed, this is zero-copy. Otherwise materializes.
    auto const arma_data = input.toArmadillo();
    arma::fmat const & input_mat = arma_data.asArmadilloMatrix();
    // input_mat shape: (num_rows, num_cols) — column-major

    ctx.reportProgress(10);

    // --- Compute row-wise reference signal ---
    // ref[t] = mean or median of included channels at time point t
    arma::fmat ref_col;// shape: (num_rows, 1)

    bool const all_included = (included_col_indices.size() == num_cols);

    if (all_included) {
        // Operate on the full matrix — no sub-matrix needed
        if (params.method == CARMethod::Mean) {
            ref_col = arma::mean(input_mat, 1);
        } else {
            ref_col = arma::median(input_mat, 1);
        }
    } else {
        arma::uvec const include_uvec(included_col_indices);
        arma::fmat const ref_source = input_mat.cols(include_uvec);
        if (params.method == CARMethod::Mean) {
            ref_col = arma::mean(ref_source, 1);
        } else {
            ref_col = arma::median(ref_source, 1);
        }
    }

    assert(ref_col.n_rows == num_rows && "Reference vector length must equal num_rows");

    ctx.reportProgress(50);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // --- Subtract reference from all channels ---
    // result.each_col() -= ref_col  →  result[t, c] -= ref_col[t]  for all t, c
    arma::fmat result_mat = input_mat;
    result_mat.each_col() -= ref_col;

    ctx.reportProgress(80);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // --- Preserve column names ---
    auto const & src_col_names = input.columnNames();
    std::vector<std::string> const col_names(src_col_names.begin(), src_col_names.end());

    // --- Reconstruct TensorData preserving RowDescriptor ---
    auto const & row_desc = input.rows();

    ctx.reportProgress(100);

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            std::move(result_mat),
                            row_desc.timeStoragePtr(),
                            row_desc.timeFrame(),
                            col_names));
        }
        case RowType::Interval: {
            auto const intervals_span = row_desc.intervals();
            std::vector<TimeFrameInterval> intervals(
                    intervals_span.begin(),
                    intervals_span.end());
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            std::move(result_mat),
                            std::move(intervals),
                            row_desc.timeFrame(),
                            col_names));
        }
        case RowType::Ordinal:
        default: {
            return std::make_shared<TensorData>(
                    TensorData::createFromArmadillo(
                            std::move(result_mat),
                            col_names));
        }
    }
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
