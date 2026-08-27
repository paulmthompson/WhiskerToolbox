/**
 * @file tensor_car_gpu_cpu.cpp
 * @brief Ad-hoc benchmark comparing GPU vs CPU Common Average Reference (CAR).
 *
 * Compares three backends (Armadillo CPU, LibTorch CPU, LibTorch CUDA) for
 * both mean-CAR and median-CAR across a matrix of channel counts and sample
 * sizes typical of multi-electrode neural recordings.
 *
 * This is a throwaway design-comparison benchmark — not a regression test.
 *
 * Usage:
 *   ./tensor_car_gpu_cpu --benchmark_format=console
 *   ./tensor_car_gpu_cpu --benchmark_filter="Mean"
 *   ./tensor_car_gpu_cpu --benchmark_filter="Median"
 *   ./tensor_car_gpu_cpu --benchmark_filter="Transfer"
 */

#include <armadillo>
#include <benchmark/benchmark.h>
#include <torch/torch.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// CAR implementations
// ============================================================================

namespace {

/// Armadillo mean-CAR: subtract row-wise mean from every column.
void carMeanArmadillo(arma::fmat const & input, arma::fmat & output) {
    output = input;
    arma::fmat const ref = arma::mean(input, 1);// (rows, 1)
    output.each_col() -= ref;
}

/// Armadillo median-CAR: subtract row-wise median from every column.
void carMedianArmadillo(arma::fmat const & input, arma::fmat & output) {
    output = input;
    arma::fmat const ref = arma::median(input, 1);// (rows, 1)
    output.each_col() -= ref;
}

/// LibTorch mean-CAR on a pre-placed tensor (CPU or CUDA).
/// @param input  shape [rows, cols], already on target device.
/// @return result on the same device.
torch::Tensor carMeanTorch(torch::Tensor const & input) {
    // mean across dim=1 (channels), keepdim for broadcast subtraction
    auto ref = input.mean(/*dim=*/1, /*keepdim=*/true);
    return input - ref;
}

/// LibTorch median-CAR on a pre-placed tensor (CPU or CUDA).
/// Note: torch::median returns the lower-middle for even-length dimensions,
/// while Armadillo returns the average of the two middle values.
/// For benchmarking purposes this difference is acceptable — we validate
/// mean-CAR for correctness and benchmark median-CAR for performance only.
torch::Tensor carMedianTorch(torch::Tensor const & input) {
    // torch::median with dim returns (values, indices); we want values
    auto ref = std::get<0>(input.median(/*dim=*/1, /*keepdim=*/true));
    return input - ref;
}

// ---- In-place variants ----

/// Armadillo mean-CAR in-place: modifies mat directly.
void carMeanArmadilloInPlace(arma::fmat & mat) {
    arma::fmat const ref = arma::mean(mat, 1);
    mat.each_col() -= ref;
}

/// Armadillo median-CAR in-place: modifies mat directly.
void carMedianArmadilloInPlace(arma::fmat & mat) {
    arma::fmat const ref = arma::median(mat, 1);
    mat.each_col() -= ref;
}

/// LibTorch mean-CAR in-place: modifies input tensor directly.
void carMeanTorchInPlace(torch::Tensor & input) {
    auto ref = input.mean(/*dim=*/1, /*keepdim=*/true);
    input.sub_(ref);
}

/// LibTorch median-CAR in-place: modifies input tensor directly.
void carMedianTorchInPlace(torch::Tensor & input) {
    auto ref = std::get<0>(input.median(/*dim=*/1, /*keepdim=*/true));
    input.sub_(ref);
}

/// Create a random arma::fmat with reproducible data.
arma::fmat generateRandomMatrix(std::size_t rows, std::size_t cols, unsigned seed = 42) {
    arma::arma_rng::set_seed(seed);
    return arma::randn<arma::fmat>(rows, cols);
}

/// Convert arma::fmat (column-major) to a row-major torch::Tensor on CPU.
torch::Tensor armaToTorch(arma::fmat const & mat) {
    auto const n_rows = static_cast<int64_t>(mat.n_rows);
    auto const n_cols = static_cast<int64_t>(mat.n_cols);
    // Armadillo stores column-major; use strides to avoid copy, then contiguous()
    auto t = torch::from_blob(
                     const_cast<float *>(mat.memptr()),
                     {n_rows, n_cols},
                     {1, n_rows},// col-major strides
                     torch::kFloat32)
                     .contiguous()// copy to row-major
                     .clone();    // own memory
    return t;
}

/// Report common counters.
void reportCounters(benchmark::State & state, int64_t rows, int64_t cols) {
    auto const elements = static_cast<double>(rows) * static_cast<double>(cols);
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["cols"] = static_cast<double>(cols);
    state.counters["elements"] = elements;
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(elements));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(elements) *
                            static_cast<int64_t>(sizeof(float)));
}

}// namespace

// ============================================================================
// Parameterised fixture
// ============================================================================

/// Parameters are encoded as two benchmark args: [rows, cols].
/// We register specific combinations below.

// ============================================================================
// Mean-CAR benchmarks
// ============================================================================

static void BM_MeanCAR_Armadillo(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const input = generateRandomMatrix(rows, cols);
    arma::fmat output;

    for (auto _: state) {
        carMeanArmadillo(input, output);
        benchmark::DoNotOptimize(output.memptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MeanCAR_TorchCPU(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const input = armaToTorch(arma_input).to(torch::kCPU);

    for (auto _: state) {
        auto result = carMeanTorch(input);
        benchmark::DoNotOptimize(result.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MeanCAR_TorchCUDA(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const input = armaToTorch(arma_input).to(torch::kCUDA);

    // Warm up GPU
    for (int i = 0; i < 5; ++i) {
        auto warmup = carMeanTorch(input);
    }
    torch::cuda::synchronize();

    for (auto _: state) {
        auto result = carMeanTorch(input);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(result.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

// ============================================================================
// Median-CAR benchmarks
// ============================================================================

static void BM_MedianCAR_Armadillo(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const input = generateRandomMatrix(rows, cols);
    arma::fmat output;

    for (auto _: state) {
        carMedianArmadillo(input, output);
        benchmark::DoNotOptimize(output.memptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MedianCAR_TorchCPU(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const input = armaToTorch(arma_input).to(torch::kCPU);

    for (auto _: state) {
        auto result = carMedianTorch(input);
        benchmark::DoNotOptimize(result.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MedianCAR_TorchCUDA(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const input = armaToTorch(arma_input).to(torch::kCUDA);

    for (int i = 0; i < 5; ++i) {
        auto warmup = carMedianTorch(input);
    }
    torch::cuda::synchronize();

    for (auto _: state) {
        auto result = carMedianTorch(input);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(result.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

// ============================================================================
// Transfer overhead benchmarks (CPU ↔ GPU)
// ============================================================================

// ============================================================================
// In-place CAR benchmarks
// ============================================================================

static void BM_MeanCAR_Armadillo_InPlace(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const original = generateRandomMatrix(rows, cols);
    arma::fmat working;

    for (auto _: state) {
        working = original;// restore original each iteration
        carMeanArmadilloInPlace(working);
        benchmark::DoNotOptimize(working.memptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MeanCAR_TorchCPU_InPlace(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const original = armaToTorch(arma_input).to(torch::kCPU);
    torch::Tensor working;

    for (auto _: state) {
        working = original.clone();
        carMeanTorchInPlace(working);
        benchmark::DoNotOptimize(working.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MeanCAR_TorchCUDA_InPlace(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const original = armaToTorch(arma_input).to(torch::kCUDA);
    torch::Tensor working;

    for (int i = 0; i < 5; ++i) {
        working = original.clone();
        carMeanTorchInPlace(working);
    }
    torch::cuda::synchronize();

    for (auto _: state) {
        working = original.clone();
        carMeanTorchInPlace(working);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(working.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MedianCAR_Armadillo_InPlace(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const original = generateRandomMatrix(rows, cols);
    arma::fmat working;

    for (auto _: state) {
        working = original;
        carMedianArmadilloInPlace(working);
        benchmark::DoNotOptimize(working.memptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MedianCAR_TorchCPU_InPlace(benchmark::State & state) {
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const original = armaToTorch(arma_input).to(torch::kCPU);
    torch::Tensor working;

    for (auto _: state) {
        working = original.clone();
        carMedianTorchInPlace(working);
        benchmark::DoNotOptimize(working.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_MedianCAR_TorchCUDA_InPlace(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<std::size_t>(state.range(0));
    auto const cols = static_cast<std::size_t>(state.range(1));
    arma::fmat const arma_input = generateRandomMatrix(rows, cols);
    torch::Tensor const original = armaToTorch(arma_input).to(torch::kCUDA);
    torch::Tensor working;

    for (int i = 0; i < 5; ++i) {
        working = original.clone();
        carMedianTorchInPlace(working);
    }
    torch::cuda::synchronize();

    for (auto _: state) {
        working = original.clone();
        carMedianTorchInPlace(working);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(working.data_ptr());
        benchmark::ClobberMemory();
    }
    reportCounters(state, static_cast<int64_t>(rows), static_cast<int64_t>(cols));
}

static void BM_Transfer_CPUtoGPU(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<int64_t>(state.range(0));
    auto const cols = static_cast<int64_t>(state.range(1));
    torch::Tensor const cpu_tensor = torch::randn({rows, cols}, torch::kFloat32);

    // Warm up
    {
        auto warmup = cpu_tensor.to(torch::kCUDA);
        torch::cuda::synchronize();
    }

    for (auto _: state) {
        auto gpu_tensor = cpu_tensor.to(torch::kCUDA);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(gpu_tensor.data_ptr());
    }
    auto const bytes = rows * cols * static_cast<int64_t>(sizeof(float));
    state.SetBytesProcessed(state.iterations() * bytes);
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["cols"] = static_cast<double>(cols);
}

static void BM_Transfer_GPUtoCPU(benchmark::State & state) {
    if (!torch::cuda::is_available()) {
        state.SkipWithMessage("CUDA not available");
        return;
    }
    auto const rows = static_cast<int64_t>(state.range(0));
    auto const cols = static_cast<int64_t>(state.range(1));
    torch::Tensor const gpu_tensor = torch::randn({rows, cols},
                                                  torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCUDA));

    // Warm up
    {
        auto warmup = gpu_tensor.to(torch::kCPU);
        torch::cuda::synchronize();
    }

    for (auto _: state) {
        auto cpu_tensor = gpu_tensor.to(torch::kCPU);
        torch::cuda::synchronize();
        benchmark::DoNotOptimize(cpu_tensor.data_ptr());
    }
    auto const bytes = rows * cols * static_cast<int64_t>(sizeof(float));
    state.SetBytesProcessed(state.iterations() * bytes);
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["cols"] = static_cast<double>(cols);
}

// ============================================================================
// Correctness validation (runs once at startup)
// ============================================================================

/// Validate that Armadillo and LibTorch produce the same results.
/// Called from a global constructor so it runs before any benchmark.
namespace {

struct CorrectnessValidator {
    CorrectnessValidator() {
        constexpr std::size_t rows = 100;
        constexpr std::size_t cols = 32;
        constexpr float tolerance = 1e-4F;

        arma::fmat const arma_input = generateRandomMatrix(rows, cols, 123);
        torch::Tensor const torch_input = armaToTorch(arma_input);

        // --- Mean CAR ---
        arma::fmat arma_mean_out;
        carMeanArmadillo(arma_input, arma_mean_out);
        torch::Tensor const torch_mean_out = carMeanTorch(torch_input);
        torch::Tensor const torch_mean_cpu = torch_mean_out.contiguous();

        float const * arma_ptr = arma_mean_out.memptr();
        float const * torch_ptr = torch_mean_cpu.data_ptr<float>();

        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < rows; ++r) {
                // Armadillo is col-major: index = r + c * rows
                // Torch is row-major: index = r * cols + c
                float const a = arma_ptr[r + c * rows];
                float const t = torch_ptr[r * cols + c];
                if (std::abs(a - t) > tolerance) {
                    fprintf(stderr,
                            "VALIDATION FAILURE (Mean): [%zu,%zu] arma=%.6f torch=%.6f diff=%.6e\n",
                            r, c, a, t, std::abs(a - t));
                    std::abort();
                }
            }
        }

        // --- Median CAR ---
        // Note: Armadillo and LibTorch compute median differently for
        // even-length dimensions (avg of two middle vs lower-middle).
        // We only verify that both produce reasonable, non-NaN output.
        arma::fmat arma_median_out;
        carMedianArmadillo(arma_input, arma_median_out);
        torch::Tensor const torch_median_out = carMedianTorch(torch_input);
        torch::Tensor const torch_median_cpu = torch_median_out.contiguous();

        // Verify no NaNs in either output
        if (arma_median_out.has_nan()) {
            fprintf(stderr, "VALIDATION FAILURE: Armadillo median-CAR produced NaN\n");
            std::abort();
        }
        if (torch_median_cpu.isnan().any().item<bool>()) {
            fprintf(stderr, "VALIDATION FAILURE: LibTorch median-CAR produced NaN\n");
            std::abort();
        }

        fprintf(stdout, "[OK] Mean-CAR correctness validated "
                        "(Armadillo vs LibTorch, %zux%zu, tol=%.0e)\n",
                rows, cols, tolerance);
        fprintf(stdout, "[OK] Median-CAR sanity check passed "
                        "(no NaN, both backends, %zux%zu)\n",
                rows, cols);
    }
};

// NOLINTNEXTLINE(cert-err58-cpp)
CorrectnessValidator const validator{};

}// namespace

// ============================================================================
// Benchmark registration — parameter matrix
// ============================================================================

// Channel counts × sample counts representing neural recording scenarios:
//   32ch  = small probe / tetrode array
//   64ch  = single-shank linear probe
//   128ch = dual-shank or high-density probe
//   256ch = Neuropixels 1.0 subset
//   384ch = full Neuropixels 1.0

// clang-format off
#define REGISTER_CAR_ARGS(BM_FUNC)            \
    BENCHMARK(BM_FUNC)                         \
        ->Args({1000,   32})                   \
        ->Args({1000,   64})                   \
        ->Args({1000,  128})                   \
        ->Args({1000,  256})                   \
        ->Args({1000,  384})                   \
        ->Args({10000,  32})                   \
        ->Args({10000,  64})                   \
        ->Args({10000, 128})                   \
        ->Args({10000, 256})                   \
        ->Args({10000, 384})                   \
        ->Args({30000,  32})                   \
        ->Args({30000,  64})                   \
        ->Args({30000, 128})                   \
        ->Args({30000, 256})                   \
        ->Args({30000, 384})                   \
        ->Unit(benchmark::kMicrosecond)

// Extended block sizes for GPU throughput scaling analysis.
// Focus on 32ch and 384ch to bracket the realistic range.
// 100k = 3.3s, 300k = 10s, 900k = 30s at 30 kHz.
#define REGISTER_SCALING_ARGS(BM_FUNC)         \
    BENCHMARK(BM_FUNC)                         \
        ->Args({100000,   32})                 \
        ->Args({100000,  384})                 \
        ->Args({300000,   32})                 \
        ->Args({300000,  384})                 \
        ->Args({900000,   32})                 \
        ->Args({900000,  384})                 \
        ->Unit(benchmark::kMicrosecond)
// clang-format on

REGISTER_CAR_ARGS(BM_MeanCAR_Armadillo);
REGISTER_CAR_ARGS(BM_MeanCAR_TorchCPU);
REGISTER_CAR_ARGS(BM_MeanCAR_TorchCUDA);

REGISTER_CAR_ARGS(BM_MedianCAR_Armadillo);
REGISTER_CAR_ARGS(BM_MedianCAR_TorchCPU);
REGISTER_CAR_ARGS(BM_MedianCAR_TorchCUDA);

// In-place variants — only at 30k rows, bracketing channel counts
#define REGISTER_INPLACE_ARGS(BM_FUNC)        \
    BENCHMARK(BM_FUNC)                         \
        ->Args({30000,  32})                   \
        ->Args({30000, 384})                   \
        ->Unit(benchmark::kMicrosecond)

REGISTER_INPLACE_ARGS(BM_MeanCAR_Armadillo_InPlace);
REGISTER_INPLACE_ARGS(BM_MeanCAR_TorchCPU_InPlace);
REGISTER_INPLACE_ARGS(BM_MeanCAR_TorchCUDA_InPlace);
REGISTER_INPLACE_ARGS(BM_MedianCAR_Armadillo_InPlace);
REGISTER_INPLACE_ARGS(BM_MedianCAR_TorchCPU_InPlace);
REGISTER_INPLACE_ARGS(BM_MedianCAR_TorchCUDA_InPlace);

REGISTER_CAR_ARGS(BM_Transfer_CPUtoGPU);
REGISTER_CAR_ARGS(BM_Transfer_GPUtoCPU);

// Scaling analysis — larger blocks, GPU and Torch CPU only
REGISTER_SCALING_ARGS(BM_MeanCAR_TorchCPU);
REGISTER_SCALING_ARGS(BM_MeanCAR_TorchCUDA);
REGISTER_SCALING_ARGS(BM_MedianCAR_TorchCPU);
REGISTER_SCALING_ARGS(BM_MedianCAR_TorchCUDA);
REGISTER_SCALING_ARGS(BM_Transfer_CPUtoGPU);
REGISTER_SCALING_ARGS(BM_Transfer_GPUtoCPU);

BENCHMARK_MAIN();
