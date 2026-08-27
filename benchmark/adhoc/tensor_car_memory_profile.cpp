/**
 * @file tensor_car_memory_profile.cpp
 * @brief Ad-hoc memory profiling tool for the TensorCAR pipeline.
 *
 * Generates a synthetic 32-channel interleaved int16 binary file, loads it as
 * tensor-backed TensorData via the project's JSON loading API, runs TensorCAR
 * (median subtraction), then creates AnalogTimeSeries views of all 32 channels.
 *
 * Designed to be profiled with heaptrack:
 *   heaptrack ./tensor_car_memory_profile [--cpu-only|--gpu-only]
 *   heaptrack_print heaptrack.tensor_car_memory_profile.*.gz
 *
 * Expected memory budget (32ch × 1M samples, int16 → float32):
 *   Binary file on disk:        ~61 MB  (32 × 1M × 2)
 *   Original TensorData (fmat): ~122 MB (32 × 1M × 4)
 *   CAR result TensorData:      ~122 MB (32 × 1M × 4)
 *   AnalogTimeSeries views:     ~few KB (zero-copy)
 *   Expected steady-state:      ~244 MB (4× binary size)
 *   Expected peak (loading):    ~305 MB (int16 + float + arma coexist)
 */

#include "DataManager/DataManager.hpp"
#include "Tensors/TensorData.hpp"
#include "TransformsV2/algorithms/TensorCAR/TensorCAR.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr std::size_t NUM_CHANNELS = 32;
constexpr std::size_t NUM_SAMPLES = 1'000'000;
constexpr char const * BINARY_PATH = "/tmp/amplifier_heaptrack_test.dat";

/// Generate a synthetic interleaved int16 binary file.
/// Layout: ch0_t0, ch1_t0, ..., ch31_t0, ch0_t1, ch1_t1, ...
/// Content: sine waves at different frequencies per channel + noise.
void generateTestData() {
    std::cerr << "Generating test data: " << NUM_CHANNELS << " channels × "
              << NUM_SAMPLES << " samples → " << BINARY_PATH << "\n";

    auto const start = std::chrono::steady_clock::now();

    std::ofstream out(BINARY_PATH, std::ios::binary);
    if (!out) {
        std::cerr << "ERROR: Cannot open " << BINARY_PATH << " for writing\n";
        std::abort();
    }

    // Write in blocks to avoid per-sample syscalls
    constexpr std::size_t BLOCK_SAMPLES = 4096;
    std::vector<int16_t> block(BLOCK_SAMPLES * NUM_CHANNELS);

    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0F, 50.0F);

    for (std::size_t t_start = 0; t_start < NUM_SAMPLES; t_start += BLOCK_SAMPLES) {
        auto const t_end = std::min(t_start + BLOCK_SAMPLES, NUM_SAMPLES);
        auto const block_len = t_end - t_start;

        for (std::size_t t = 0; t < block_len; ++t) {
            auto const global_t = static_cast<float>(t_start + t);
            for (std::size_t ch = 0; ch < NUM_CHANNELS; ++ch) {
                // Each channel: sine at freq proportional to channel index
                float const freq = 1.0F + static_cast<float>(ch) * 10.0F;
                float const phase = 2.0F * std::numbers::pi_v<float> * freq * global_t / 30000.0F;
                float const value = 1000.0F * std::sin(phase) + noise(rng);
                // Clamp to int16 range
                auto const clamped = std::clamp(value, -32768.0F, 32767.0F);
                block[t * NUM_CHANNELS + ch] = static_cast<int16_t>(clamped);
            }
        }

        out.write(reinterpret_cast<char const *>(block.data()),
                  static_cast<std::streamsize>(block_len * NUM_CHANNELS * sizeof(int16_t)));
    }

    out.close();

    auto const elapsed = std::chrono::steady_clock::now() - start;
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    auto const file_size_mb = static_cast<double>(NUM_CHANNELS * NUM_SAMPLES * sizeof(int16_t)) / (1024.0 * 1024.0);
    std::cerr << "  Generated " << file_size_mb << " MB in " << ms << " ms\n";
}

/// Print a memory summary line.
void printMemorySummary(std::string const & label, std::size_t rows, std::size_t cols) {
    auto const bytes = rows * cols * sizeof(float);
    auto const mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::cerr << "  " << label << ": " << rows << " × " << cols
              << " float32 = " << mb << " MB\n";
}

/// Create a ComputeContext that logs to stderr.
auto makeContext() -> Neuralyzer::Transforms::V2::ComputeContext {
    return Neuralyzer::Transforms::V2::ComputeContext{
            .progress = [](int p) {
                if (p == 0 || p == 50 || p == 100) {
                    std::cerr << "    progress: " << p << "%\n";
                }
            },
            .is_cancelled = []() { return false; },
            .log = [](std::string const & msg) { std::cerr << "    [LOG] " << msg << "\n"; }};
}

enum class Mode { Both, CpuOnly, GpuOnly };

Mode parseArgs(int argc, char ** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cpu-only") == 0) return Mode::CpuOnly;
        if (std::strcmp(argv[i], "--gpu-only") == 0) return Mode::GpuOnly;
    }
    return Mode::Both;
}

}// namespace

int main(int argc, char ** argv) {
    auto const mode = parseArgs(argc, argv);

    std::cerr << "=== TensorCAR Memory Profiling Tool ===\n";
    std::cerr << "Mode: " << (mode == Mode::CpuOnly ? "CPU only" : mode == Mode::GpuOnly ? "GPU only"
                                                                                          : "Both CPU and GPU")
              << "\n\n";

    // ---- Step 1: Generate test data ----
    generateTestData();

    auto const binary_size_mb = static_cast<double>(NUM_CHANNELS * NUM_SAMPLES * sizeof(int16_t)) / (1024.0 * 1024.0);

    // ---- Step 2: Load via JSON config (exercises the full IO pipeline) ----
    std::cerr << "\n--- Loading data via JSON config ---\n";

    auto dm = std::make_unique<DataManager>();

    nlohmann::json config = nlohmann::json::array();
    config.push_back({{"filepath", BINARY_PATH},
                      {"data_type", "analog"},
                      {"name", "voltage"},
                      {"format", "binary"},
                      {"binary_data_type", "int16"},
                      {"header_size", 0},
                      {"num_channels", static_cast<int>(NUM_CHANNELS)},
                      {"clock", "master"},
                      {"use_tensor_backed", true},
                      {"use_memory_mapped", false}});

    auto const load_start = std::chrono::steady_clock::now();
    auto data_info = load_data_from_json_config(dm.get(), config, "/tmp");
    auto const load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - load_start)
                                 .count();

    std::cerr << "  Loaded " << data_info.size() << " data objects in " << load_ms << " ms\n";
    for (auto const & info : data_info) {
        std::cerr << "    key=\"" << info.key << "\" class=" << info.data_class << "\n";
    }

    // Get the TensorData
    auto voltage_tensor = dm->getData<TensorData>("voltage");
    if (!voltage_tensor) {
        std::cerr << "ERROR: Failed to load TensorData under key 'voltage'\n";
        return 1;
    }

    std::cerr << "\n--- Loaded TensorData ---\n";
    printMemorySummary("voltage", voltage_tensor->numRows(), voltage_tensor->numColumns());

    // ---- Step 3: TensorCAR (CPU path) ----
    if (mode != Mode::GpuOnly) {
        std::cerr << "\n--- TensorCAR (CPU, Median) ---\n";

        using namespace Neuralyzer::Transforms::V2::Examples;
        TensorCARParams params_cpu{
                .method = CARMethod::Median,
                .exclude_channels = {},
                .use_gpu = false};

        auto ctx = makeContext();

        auto const car_start = std::chrono::steady_clock::now();
        auto car_result_cpu = tensorCAR(*voltage_tensor, params_cpu, ctx);
        auto const car_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - car_start)
                                    .count();

        if (!car_result_cpu) {
            std::cerr << "ERROR: TensorCAR (CPU) returned nullptr\n";
            return 1;
        }

        std::cerr << "  Completed in " << car_ms << " ms\n";
        printMemorySummary("CAR result (CPU)", car_result_cpu->numRows(), car_result_cpu->numColumns());

        // Store in DataManager
        dm->setData<TensorData>("voltage_car_cpu", car_result_cpu, TimeKey("master"));

        // ---- Step 4: Create AnalogTimeSeries views from CAR result ----
        std::cerr << "\n--- Creating AnalogTimeSeries views (CPU CAR result) ---\n";

        TensorToAnalogParams view_params{
                .output_key_prefix = "voltage_car_cpu",
                .columns = {}};// empty = all columns

        auto views = tensorToAnalog(*car_result_cpu, view_params, ctx);
        std::cerr << "  Created " << views.size() << " AnalogTimeSeries views\n";

        // Register views in DataManager
        for (std::size_t ch = 0; ch < views.size(); ++ch) {
            auto key = "voltage_car_cpu_ch_" + std::to_string(ch);
            dm->setData<AnalogTimeSeries>(key, views[ch], TimeKey("master"));
        }

        // Verify a view has data
        if (!views.empty()) {
            auto const & first_view = views[0];
            std::cerr << "  View[0] size: " << first_view->getNumSamples()
                      << " samples (should be " << NUM_SAMPLES << ")\n";
        }
    }

    // ---- Step 5: TensorCAR (GPU path) ----
    if (mode != Mode::CpuOnly) {
        std::cerr << "\n--- TensorCAR (GPU, Median) ---\n";

        using namespace Neuralyzer::Transforms::V2::Examples;
        TensorCARParams params_gpu{
                .method = CARMethod::Median,
                .exclude_channels = {},
                .use_gpu = true};

        auto ctx = makeContext();

        auto const car_start = std::chrono::steady_clock::now();
        auto car_result_gpu = tensorCAR(*voltage_tensor, params_gpu, ctx);
        auto const car_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - car_start)
                                    .count();

        if (!car_result_gpu) {
            std::cerr << "ERROR: TensorCAR (GPU) returned nullptr\n";
            return 1;
        }

        std::cerr << "  Completed in " << car_ms << " ms\n";
        printMemorySummary("CAR result (GPU)", car_result_gpu->numRows(), car_result_gpu->numColumns());

        // Store in DataManager
        dm->setData<TensorData>("voltage_car_gpu", car_result_gpu, TimeKey("master"));

        // Create views for GPU result too
        std::cerr << "\n--- Creating AnalogTimeSeries views (GPU CAR result) ---\n";

        TensorToAnalogParams view_params_gpu{
                .output_key_prefix = "voltage_car_gpu",
                .columns = {}};

        auto views_gpu = tensorToAnalog(*car_result_gpu, view_params_gpu, ctx);
        std::cerr << "  Created " << views_gpu.size() << " AnalogTimeSeries views\n";

        for (std::size_t ch = 0; ch < views_gpu.size(); ++ch) {
            auto key = "voltage_car_gpu_ch_" + std::to_string(ch);
            dm->setData<AnalogTimeSeries>(key, views_gpu[ch], TimeKey("master"));
        }
    }

    // ---- Step 6: Memory summary ----
    std::cerr << "\n=== MEMORY SUMMARY ===\n";
    std::cerr << "Binary file size:   " << binary_size_mb << " MB (int16)\n";

    auto const tensor_mb = static_cast<double>(NUM_CHANNELS * NUM_SAMPLES * sizeof(float)) / (1024.0 * 1024.0);
    std::cerr << "Per-tensor size:    " << tensor_mb << " MB (float32)\n";

    int tensor_count = 1;// original
    if (mode != Mode::GpuOnly) tensor_count++;
    if (mode != Mode::CpuOnly) tensor_count++;
    auto const total_tensor_mb = tensor_mb * tensor_count;

    std::cerr << "Tensor count:       " << tensor_count
              << " (original + CAR results)\n";
    std::cerr << "Expected total:     " << total_tensor_mb << " MB\n";
    std::cerr << "Ratio to binary:    " << (total_tensor_mb / binary_size_mb) << "×\n";
    std::cerr << "\nExpected peak (during loading): ~"
              << (binary_size_mb + 2.0 * tensor_mb) << " MB\n";
    std::cerr << "  (int16 vectors + float vectors + arma::fmat all coexist)\n";
    std::cerr << "\nAnalogTimeSeries views are zero-copy (shared_ptr to TensorData columns)\n";

    // ---- Cleanup: remove temp file ----
    std::filesystem::remove(BINARY_PATH);
    std::cerr << "\nCleaned up " << BINARY_PATH << "\n";
    std::cerr << "=== Done. Run heaptrack_print on the output to analyze. ===\n";

    return 0;
}
