/**
 * @file mask_connected_component_timing.cpp
 * @brief Ad-hoc GoogleTest driver to measure `remove_small_connected_components` on dense 256×256 masks.
 *
 * Builds a `MaskData` whose single time slice enumerates every foreground pixel (sparse storage of a
 * solid binary tile), then times the public transform entry point and optional breakdown timings
 * (`mask_to_binary_image`, `remove_small_clusters`, `binary_image_to_mask`).
 *
 * Build (requires `ENABLE_BENCHMARK=ON` so `benchmark/adhoc` is part of the project):
 *   cmake --preset my-clang-release -DENABLE_BENCHMARK=ON > config_log.txt 2>&1
 *   cmake --build --preset my-clang-release --target mask_connected_component_timing > build_log.txt 2>&1
 *
 * Run directly (ctest is not used for this target):
 *   ./out/build/Clang/Release/bin/adhoc/mask_connected_component_timing
 *   ./out/build/Clang/Release/bin/adhoc/mask_connected_component_timing --gtest_filter='*EndToEnd*'
 *
 * Profiling:
 *   perf record -g --call-graph dwarf ./out/build/Clang/Release/bin/adhoc/mask_connected_component_timing
 *   perf report
 *   heaptrack ./out/build/Clang/Release/bin/adhoc/mask_connected_component_timing
 */

#include "mask_connected_component.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/utils/connected_component.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <vector>

constexpr int kDim = 256;

/// @brief Median wall-clock duration of @p fn over @p sample_count runs after @p warmup_runs discarded runs.
template<typename Func>
auto median_seconds(Func && fn, int warmup_runs, std::size_t sample_count) -> double {
    for (int i = 0; i < warmup_runs; ++i) {
        fn();
    }
    std::vector<double> samples;
    samples.reserve(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        auto const t0 = std::chrono::steady_clock::now();
        fn();
        auto const t1 = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double>(t1 - t0).count());
    }
    auto const mid = samples.begin() + static_cast<std::ptrdiff_t>(samples.size() / 2U);
    std::nth_element(samples.begin(), mid, samples.end());
    return *mid;
}

auto make_solid_square_mask_data() -> std::shared_ptr<MaskData> {
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize({kDim, kDim});

    Mask2D dense;
    dense.reserve(static_cast<std::size_t>(kDim) * static_cast<std::size_t>(kDim));
    for (int y = 0; y < kDim; ++y) {
        for (int x = 0; x < kDim; ++x) {
            dense.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
        }
    }
    mask_data->addAtTime(TimeFrameIndex(0), dense, NotifyObservers::No);
    return mask_data;
}

auto getenv_int(char const * name, int default_value) -> int {
    char const * raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return default_value;
    }
    errno = 0;
    char * end_ptr = nullptr;
    long const value = std::strtol(raw, &end_ptr, 10);
    if (errno != 0 || end_ptr == raw || *end_ptr != '\0' || value < static_cast<long>(INT_MIN) ||
        value > static_cast<long>(INT_MAX)) {
        return default_value;
    }
    return static_cast<int>(value);
}



TEST(MaskConnectedComponentTiming, EndToEndSolid256DefaultThreshold) {
    auto const input = make_solid_square_mask_data();
    MaskConnectedComponentParameters params{};
    params.threshold = 10;

    int const warmup = getenv_int("MASK_CC_TIMING_WARMUP", 2);
    int const reps_i = getenv_int("MASK_CC_TIMING_REPS", 11);
    std::size_t const sample_count = reps_i > 0 ? static_cast<std::size_t>(reps_i) : 1U;

    auto const median_sec = median_seconds(
            [&]() {
                auto out = remove_small_connected_components(input.get(), &params);
                ASSERT_NE(out, nullptr);
                ASSERT_FALSE(out->getTimesWithData().empty());
            },
            warmup,
            sample_count);

    double const ms = median_sec * 1000.0;
    std::cerr << "[MaskConnectedComponentTiming] End-to-end solid " << kDim << "×" << kDim
              << " median=" << ms << " ms (warmup=" << warmup << " reps=" << sample_count << ")\n";

    SUCCEED();
}

TEST(MaskConnectedComponentTiming, BreakdownSolid256) {
    auto const input = make_solid_square_mask_data();
    auto const & mask = input->getAtTime(TimeFrameIndex(0))[0];
    ImageSize const size{kDim, kDim};

    int const warmup = getenv_int("MASK_CC_TIMING_WARMUP", 2);
    int const reps_i = getenv_int("MASK_CC_TIMING_REPS", 11);
    std::size_t const sample_count = reps_i > 0 ? static_cast<std::size_t>(reps_i) : 1U;

    Image binary;
    auto const t_raster = median_seconds(
            [&]() { binary = mask_to_binary_image(mask, size); }, warmup, sample_count);

    MaskConnectedComponentParameters params{};
    params.threshold = 10;
    Image filtered;
    auto const t_cc = median_seconds(
            [&]() { filtered = remove_small_clusters(binary, params.threshold); }, warmup, sample_count);

    Mask2D out_mask;
    auto const t_vectorize = median_seconds(
            [&]() { out_mask = binary_image_to_mask(filtered); }, warmup, sample_count);

    std::cerr << "[MaskConnectedComponentTiming] Breakdown (median seconds): "
              << "mask_to_binary_image=" << (t_raster * 1000.0) << " ms, "
              << "remove_small_clusters=" << (t_cc * 1000.0) << " ms, "
              << "binary_image_to_mask=" << (t_vectorize * 1000.0) << " ms\n";

    ASSERT_GT(out_mask.size(), 0U);
    SUCCEED();
}
