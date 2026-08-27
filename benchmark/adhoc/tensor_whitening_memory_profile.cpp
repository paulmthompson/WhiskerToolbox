/**
 * @file tensor_whitening_memory_profile.cpp
 * @brief Ad-hoc memory profiling tool for the TensorWhitening transform.
 *
 * Generates a synthetic correlated float32 tensor, constructs a 2D time-series
 * TensorData, optionally preconverts the input to LibTorch storage, then runs
 * TensorWhitening on CPU and/or GPU.
 *
 * Designed to be profiled with heaptrack:
 *   heaptrack ./tensor_whitening_memory_profile --cpu-only --rows=1048576
 *   heaptrack ./tensor_whitening_memory_profile --gpu-only --rows=1048576
 *   heaptrack ./tensor_whitening_memory_profile --cpu-only --preconvert-libtorch
 *
 * The default dimensions match the validation dataset reported during feature
 * work: 32 channels x 29,349,632 samples.
 */

#include "Tensors/TensorData.hpp"
#include "Tensors/storage/TensorStorageBase.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/algorithms/TensorWhitening/TensorWhitening.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t DEFAULT_ROWS = 29'349'632U;
constexpr std::size_t DEFAULT_COLS = 32U;

enum class Mode { Both,
                  CpuOnly,
                  GpuOnly };

struct Options {
    Mode mode = Mode::Both;
    std::size_t rows = DEFAULT_ROWS;
    std::size_t cols = DEFAULT_COLS;
    bool preconvert_libtorch = false;
    bool rescale_amplitude = true;
    bool estimate_only = false;
    float mad_threshold_multiplier = 4.0F;
    float epsilon = 1.0e-8F;
    std::vector<int> exclude_channels;
};

struct ProcMemorySnapshot {
    std::size_t vm_rss_kb = 0U;
    std::size_t vm_hwm_kb = 0U;
};

auto bytesToGiB(std::size_t bytes) -> double {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

auto storageTypeName(TensorStorageType type) -> std::string_view {
    switch (type) {
        case TensorStorageType::Armadillo:
            return "Armadillo";
        case TensorStorageType::Dense:
            return "Dense";
        case TensorStorageType::LibTorch:
            return "LibTorch";
        case TensorStorageType::View:
            return "View";
        case TensorStorageType::Lazy:
            return "Lazy";
        case TensorStorageType::MemoryMapped:
            return "MemoryMapped";
        default:
            return "Unknown";
    }
}

void printUsage(char const * program_name) {
    std::cerr
            << "Usage: " << program_name << " [options]\n"
            << "\nOptions:\n"
            << "  --cpu-only                 Run only the CPU whitening path\n"
            << "  --gpu-only                 Run only the GPU whitening path\n"
            << "  --rows=<count>             Number of rows/samples (default: " << DEFAULT_ROWS << ")\n"
            << "  --cols=<count>             Number of columns/channels (default: " << DEFAULT_COLS << ")\n"
            << "  --exclude=0,31            Comma-separated excluded channel indices\n"
            << "  --mad-threshold=<value>    MAD multiplier for clean-sample masking\n"
            << "  --epsilon=<value>          Positive eigenvalue regularizer\n"
            << "  --no-rescale               Disable amplitude rescaling\n"
            << "  --preconvert-libtorch      Convert input to LibTorch before whitening\n"
            << "  --estimate-only            Print memory estimates without allocating data\n"
            << "  --help                     Show this message\n";
}

auto parseSize(std::string_view text, char const * option_name) -> std::size_t {
    std::size_t value = 0U;
    try {
        value = static_cast<std::size_t>(std::stoull(std::string{text}));
    } catch (std::exception const & ex) {
        throw std::invalid_argument(std::string(option_name) + ": invalid size: " + ex.what());
    }
    return value;
}

auto parseFloat(std::string_view text, char const * option_name) -> float {
    float value = 0.0F;
    try {
        value = std::stof(std::string{text});
    } catch (std::exception const & ex) {
        throw std::invalid_argument(std::string(option_name) + ": invalid float: " + ex.what());
    }
    return value;
}

void parseExcludeChannels(std::string_view text, std::vector<int> & exclude_channels) {
    exclude_channels.clear();
    if (text.empty()) {
        return;
    }

    std::size_t start = 0U;
    while (start < text.size()) {
        auto const end = text.find(',', start);
        auto const token = text.substr(start, end == std::string_view::npos ? text.size() - start
                                                                            : end - start);
        if (!token.empty()) {
            exclude_channels.push_back(std::stoi(std::string{token}));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
}

auto parseArgs(int argc, char ** argv) -> Options {
    Options options;
    for (int index = 1; index < argc; ++index) {
        std::string_view const arg{argv[index]};
        if (arg == "--cpu-only") {
            options.mode = Mode::CpuOnly;
        } else if (arg == "--gpu-only") {
            options.mode = Mode::GpuOnly;
        } else if (arg == "--no-rescale") {
            options.rescale_amplitude = false;
        } else if (arg == "--preconvert-libtorch") {
            options.preconvert_libtorch = true;
        } else if (arg == "--estimate-only") {
            options.estimate_only = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg.starts_with("--rows=")) {
            options.rows = parseSize(arg.substr(7), "--rows");
        } else if (arg.starts_with("--cols=")) {
            options.cols = parseSize(arg.substr(7), "--cols");
        } else if (arg.starts_with("--exclude=")) {
            parseExcludeChannels(arg.substr(10), options.exclude_channels);
        } else if (arg.starts_with("--mad-threshold=")) {
            options.mad_threshold_multiplier = parseFloat(arg.substr(16), "--mad-threshold");
        } else if (arg.starts_with("--epsilon=")) {
            options.epsilon = parseFloat(arg.substr(10), "--epsilon");
        } else {
            throw std::invalid_argument("Unknown argument: " + std::string{arg});
        }
    }

    if (options.rows < 2U) {
        throw std::invalid_argument("--rows must be at least 2");
    }
    if (options.cols == 0U) {
        throw std::invalid_argument("--cols must be at least 1");
    }
    if (options.epsilon <= 0.0F) {
        throw std::invalid_argument("--epsilon must be positive");
    }
    return options;
}

auto checkedMultiply(std::size_t lhs, std::size_t rhs, char const * label) -> std::size_t {
    if (lhs != 0U && rhs > (std::numeric_limits<std::size_t>::max() / lhs)) {
        throw std::overflow_error(std::string(label) + ": size overflow");
    }
    return lhs * rhs;
}

auto readProcMemorySnapshot() -> ProcMemorySnapshot {
    ProcMemorySnapshot snapshot;
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        auto parse_kb = [&line]() -> std::size_t {
            std::istringstream iss(line);
            std::string key;
            std::size_t value = 0U;
            std::string unit;
            iss >> key >> value >> unit;
            return value;
        };

        if (line.starts_with("VmRSS:")) {
            snapshot.vm_rss_kb = parse_kb();
        } else if (line.starts_with("VmHWM:")) {
            snapshot.vm_hwm_kb = parse_kb();
        }
    }
    return snapshot;
}

void printProcMemory(std::string_view label) {
    auto const snapshot = readProcMemorySnapshot();
    std::cerr << "  " << label << ": RSS="
              << std::fixed << std::setprecision(3)
              << bytesToGiB(snapshot.vm_rss_kb * 1024U)
              << " GiB, HWM=" << bytesToGiB(snapshot.vm_hwm_kb * 1024U) << " GiB\n";
}

auto makeContext() -> Neuralyzer::Transforms::V2::ComputeContext {
    return Neuralyzer::Transforms::V2::ComputeContext{
            .progress = [](int progress) {
                if (progress == 0 || progress == 20 || progress == 35 || progress == 55 ||
                    progress == 80 || progress == 100) {
                    std::cerr << "    progress: " << progress << "%\n";
                } },
            .is_cancelled = []() { return false; },
            .log = [](std::string const & message) { std::cerr << "    [LOG] " << message << "\n"; }};
}

auto generateCorrelatedFlat(std::size_t rows, std::size_t cols) -> std::vector<float> {
    auto const element_count = checkedMultiply(rows, cols, "generateCorrelatedFlat");
    std::vector<float> flat(element_count);

    for (std::size_t row = 0; row < rows; ++row) {
        auto const t = static_cast<double>(row);
        double const latent_a = std::sin(0.00037 * t);
        double const latent_b = std::cos(0.00013 * t);
        double const latent_c = std::sin(0.00071 * t + 0.35);

        for (std::size_t col = 0; col < cols; ++col) {
            auto const col_d = static_cast<double>(col);
            double const weight_a = 1.0 + 0.015 * col_d;
            double const weight_b = ((static_cast<int>(col % 7U) - 3) * 0.11);
            double const weight_c = ((static_cast<int>(col % 5U) - 2) * 0.07);
            double const drift = 0.000001 * t * ((col % 3U) + 1U);
            flat[row * cols + col] = static_cast<float>(
                    (weight_a * latent_a) +
                    (weight_b * latent_b) +
                    (weight_c * latent_c) +
                    drift);
        }
    }

    return flat;
}

auto makeInputTensor(std::size_t rows, std::size_t cols) -> TensorData {
    auto flat = generateCorrelatedFlat(rows, cols);
    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);
    return TensorData::createTimeSeries2D(
            flat,
            rows,
            cols,
            std::move(time_storage),
            nullptr,
            {});
}

void printEstimateSummary(Options const & options) {
    auto const total_elements = checkedMultiply(options.rows, options.cols, "estimate");
    auto const included_cols = options.cols - std::min(options.cols, options.exclude_channels.size());
    auto const input32_bytes = checkedMultiply(total_elements, sizeof(float), "input32");
    auto const included32_bytes = checkedMultiply(
            checkedMultiply(options.rows, included_cols, "included elements"),
            sizeof(float),
            "included32");
    auto const included64_bytes = checkedMultiply(
            checkedMultiply(options.rows, included_cols, "included elements"),
            sizeof(double),
            "included64");

    std::cerr << "\n=== ESTIMATED FULL-BUFFER FOOTPRINT ===\n";
    std::cerr << "Input float32 matrix:          " << bytesToGiB(input32_bytes) << " GiB\n";
    std::cerr << "LibTorch input float32 copy:   " << bytesToGiB(input32_bytes) << " GiB\n";
    std::cerr << "result float32 clone:          " << bytesToGiB(input32_bytes) << " GiB\n";
    std::cerr << "included float32 gather:       " << bytesToGiB(included32_bytes) << " GiB\n";
    std::cerr << "included64:                    " << bytesToGiB(included64_bytes) << " GiB\n";
    std::cerr << "centered64:                    " << bytesToGiB(included64_bytes) << " GiB\n";
    std::cerr << "whitened64:                    " << bytesToGiB(included64_bytes) << " GiB\n";

    auto const cpu_peak_estimate = input32_bytes + input32_bytes + input32_bytes +
                                   included32_bytes + included64_bytes + included64_bytes +
                                   included64_bytes;
    std::cerr << "Approx CPU peak if all coexist: " << bytesToGiB(cpu_peak_estimate) << " GiB\n";

    auto const gpu_peak_estimate = input32_bytes + input32_bytes + included32_bytes +
                                   included64_bytes + included64_bytes + included64_bytes;
    std::cerr << "Approx GPU working set:         " << bytesToGiB(gpu_peak_estimate) << " GiB\n";
    std::cerr << "Included columns:               " << included_cols << "/" << options.cols << "\n";
}

void printOptions(Options const & options) {
    std::cerr << "=== TensorWhitening Memory Profiling Tool ===\n";
    std::cerr << "rows=" << options.rows << ", cols=" << options.cols << "\n";
    std::cerr << "mode=";
    switch (options.mode) {
        case Mode::Both:
            std::cerr << "both";
            break;
        case Mode::CpuOnly:
            std::cerr << "cpu-only";
            break;
        case Mode::GpuOnly:
            std::cerr << "gpu-only";
            break;
    }
    std::cerr << ", preconvert_libtorch=" << std::boolalpha << options.preconvert_libtorch
              << ", rescale_amplitude=" << options.rescale_amplitude
              << ", mad_threshold_multiplier=" << options.mad_threshold_multiplier
              << ", epsilon=" << options.epsilon << "\n";
    std::cerr << "excluded_channels=";
    if (options.exclude_channels.empty()) {
        std::cerr << "[]\n";
    } else {
        std::cerr << "[";
        for (std::size_t index = 0; index < options.exclude_channels.size(); ++index) {
            if (index != 0U) {
                std::cerr << ',';
            }
            std::cerr << options.exclude_channels[index];
        }
        std::cerr << "]\n";
    }
}

auto maybePreconvertInput(TensorData const & input, bool use_gpu) -> TensorData {
#ifdef TENSOR_BACKEND_LIBTORCH
    if (use_gpu) {
        return input.toLibTorchStrided();
    }
    return input.toLibTorch();
#else
    (void) use_gpu;
    throw std::runtime_error("TensorWhitening preconversion requires TENSOR_BACKEND_LIBTORCH");
#endif
}

auto runScenario(
        std::string_view label,
        TensorData const & base_input,
        Options const & options,
        bool use_gpu) -> bool {
    using namespace Neuralyzer::Transforms::V2;
    using namespace Neuralyzer::Transforms::V2::Examples;

    std::cerr << "\n--- " << label << " ---\n";
    printProcMemory("before scenario");

    TensorWhiteningParams params;
    params.exclude_channels = options.exclude_channels;
    params.mad_threshold_multiplier = options.mad_threshold_multiplier;
    params.rescale_amplitude = options.rescale_amplitude;
    params.epsilon = options.epsilon;
    params.use_gpu = use_gpu;

    std::optional<TensorData> prepared_input;
    TensorData const * input_ptr = &base_input;
    if (options.preconvert_libtorch) {
        auto const preconvert_start = std::chrono::steady_clock::now();
        prepared_input.emplace(maybePreconvertInput(base_input, use_gpu));
        auto const preconvert_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - preconvert_start)
                                           .count();
        input_ptr = &prepared_input.value();
        std::cerr << "  Preconverted input to "
                  << storageTypeName(input_ptr->storage().getStorageType())
                  << " in " << preconvert_ms << " ms\n";
        printProcMemory("after preconversion");
    }

    auto const ctx = makeContext();
    auto const whitening_start = std::chrono::steady_clock::now();
    auto result = tensorWhitening(*input_ptr, params, ctx);
    auto const whitening_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - whitening_start)
                                      .count();

    if (!result) {
        std::cerr << "  TensorWhitening returned nullptr\n";
        return false;
    }

    std::cerr << "  Completed in " << whitening_ms << " ms\n";
    std::cerr << "  Result storage: " << storageTypeName(result->storage().getStorageType())
              << ", shape=" << result->numRows() << " x " << result->numColumns() << "\n";
    printProcMemory("after whitening");

    result.reset();
    prepared_input.reset();
    printProcMemory("after releasing scenario buffers");
    return true;
}

}// namespace

int main(int argc, char ** argv) {
    try {
        auto const options = parseArgs(argc, argv);
        printOptions(options);
        printEstimateSummary(options);

        if (options.estimate_only) {
            std::cerr << "\nEstimate-only mode selected; skipping allocation and execution.\n";
            return 0;
        }

        std::cerr << "\n--- Generating synthetic input tensor ---\n";
        printProcMemory("startup");
        auto const make_input_start = std::chrono::steady_clock::now();
        auto input = makeInputTensor(options.rows, options.cols);
        auto const make_input_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - make_input_start)
                                           .count();
        std::cerr << "  Created input in " << make_input_ms << " ms\n";
        std::cerr << "  Input storage: " << storageTypeName(input.storage().getStorageType())
                  << ", shape=" << input.numRows() << " x " << input.numColumns() << "\n";
        printProcMemory("after input creation");

        bool ok = true;
        if (options.mode != Mode::GpuOnly) {
            ok = runScenario("TensorWhitening CPU", input, options, false) && ok;
        }
        if (options.mode != Mode::CpuOnly) {
            ok = runScenario("TensorWhitening GPU", input, options, true) && ok;
        }

        std::cerr << "\n=== Done ===\n";
        return ok ? 0 : 1;
    } catch (std::exception const & ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
}