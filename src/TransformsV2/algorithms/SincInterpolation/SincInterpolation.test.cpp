/**
 * @file SincInterpolation.test.cpp
 * @brief Tests for the sinc interpolation container transform.
 *
 * Tests cover:
 * - Output size contract: (N-1)*factor+1
 * - Identity (factor=1)
 * - DC signal preservation
 * - Original sample pass-through at integer positions
 * - Sine wave reconstruction accuracy
 * - Window type comparison (larger kernel → better reconstruction)
 * - Boundary modes (symmetric vs zero-pad)
 * - Edge cases (empty, single sample, invalid factor)
 * - DataManager pipeline integration via JSON
 */

#include "TransformsV2/algorithms/SincInterpolation/SincInterpolation.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Helper: default ComputeContext
// ============================================================================

namespace {

ComputeContext makeCtx() {
    return ComputeContext{};
}

/// @brief Create an AnalogTimeSeries from a vector of floats.
std::shared_ptr<AnalogTimeSeries> makeATS(std::vector<float> data) {
    auto const n = data.size();
    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

/// @brief Create a sine wave with given frequency/sample_rate/num_samples.
std::vector<float> makeSineWave(float freq_hz, float sample_rate_hz, int num_samples) {
    std::vector<float> out(static_cast<size_t>(num_samples));
    for (int i = 0; i < num_samples; ++i) {
        auto const t = static_cast<float>(i) / sample_rate_hz;
        out[static_cast<size_t>(i)] = std::sin(2.0f * std::numbers::pi_v<float> * freq_hz * t);
    }
    return out;
}

}// anonymous namespace

// ============================================================================
// Output Size Tests
// ============================================================================

TEST_CASE("SincInterpolation output size is (N-1)*factor+1",
          "[transforms][v2][sinc][size]") {
    auto const ctx = makeCtx();
    SincInterpolationParams params;

    SECTION("10 samples, factor 4") {
        params.upsampling_factor = 4;
        auto const input = makeATS(std::vector<float>(10, 1.0f));
        auto const result = sincInterpolation(*input, params, ctx);
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 37);// (10-1)*4+1
    }

    SECTION("5 samples, factor 2") {
        params.upsampling_factor = 2;
        auto const input = makeATS(std::vector<float>(5, 0.0f));
        auto const result = sincInterpolation(*input, params, ctx);
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 9);// (5-1)*2+1
    }

    SECTION("100 samples, factor 60 (500 Hz → 30 kHz)") {
        params.upsampling_factor = 60;
        auto const input = makeATS(std::vector<float>(100, 0.0f));
        auto const result = sincInterpolation(*input, params, ctx);
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5941);// (100-1)*60+1
    }

    SECTION("2 samples, factor 10") {
        params.upsampling_factor = 10;
        auto const input = makeATS({0.0f, 1.0f});
        auto const result = sincInterpolation(*input, params, ctx);
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 11);// (2-1)*10+1
    }
}

// ============================================================================
// Identity / Factor-1 Test
// ============================================================================

TEST_CASE("SincInterpolation factor=1 returns identical data",
          "[transforms][v2][sinc][identity]") {
    auto const ctx = makeCtx();
    SincInterpolationParams params;
    params.upsampling_factor = 1;

    std::vector<float> const data = {1.0f, 2.5f, -3.0f, 0.0f, 7.7f};
    auto const input = makeATS(data);
    auto const result = sincInterpolation(*input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == data.size());

    auto const out = result->getAnalogTimeSeries();
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(out[i], Catch::Matchers::WithinAbs(data[i], 1e-6f));
    }
}

// ============================================================================
// DC Signal Test
// ============================================================================

TEST_CASE("SincInterpolation preserves DC signal",
          "[transforms][v2][sinc][dc]") {
    auto const ctx = makeCtx();
    SincInterpolationParams params;
    params.upsampling_factor = 4;

    float const dc_value = 42.0f;
    auto const input = makeATS(std::vector<float>(20, dc_value));
    auto const result = sincInterpolation(*input, params, ctx);

    REQUIRE(result != nullptr);
    auto const out = result->getAnalogTimeSeries();

    // With kernel normalization, DC should be perfectly preserved everywhere
    for (size_t i = 0; i < out.size(); ++i) {
        INFO("Sample " << i);
        REQUIRE_THAT(out[i], Catch::Matchers::WithinAbs(dc_value, 1e-4f));
    }
}

// ============================================================================
// Original Samples Pass-Through
// ============================================================================

TEST_CASE("SincInterpolation passes through original samples exactly",
          "[transforms][v2][sinc][passthrough]") {
    auto const ctx = makeCtx();
    SincInterpolationParams params;
    params.upsampling_factor = 4;

    std::vector<float> const data = {0.0f, 1.0f, -1.0f, 0.5f, 2.0f, -0.5f, 0.0f, 3.0f};
    auto const input = makeATS(data);
    auto const result = sincInterpolation(*input, params, ctx);
    REQUIRE(result != nullptr);

    auto const out = result->getAnalogTimeSeries();
    for (size_t i = 0; i < data.size(); ++i) {
        size_t const out_idx = i * 4;
        INFO("Original sample " << i << " at output index " << out_idx);
        REQUIRE_THAT(out[out_idx], Catch::Matchers::WithinAbs(data[i], 1e-6f));
    }
}

// ============================================================================
// Sine Wave Reconstruction
// ============================================================================

TEST_CASE("SincInterpolation reconstructs band-limited sine wave",
          "[transforms][v2][sinc][sine]") {
    auto const ctx = makeCtx();

    // 50 Hz sine sampled at 500 Hz → well below Nyquist (250 Hz)
    // Upsample 4x to 2000 Hz
    float const freq = 50.0f;
    float const input_rate = 500.0f;
    int const n_samples = 100;
    int const factor = 4;

    auto const sine_data = makeSineWave(freq, input_rate, n_samples);
    auto const input = makeATS(sine_data);

    SincInterpolationParams params;
    params.upsampling_factor = factor;
    params.kernel_half_width = 8;

    auto const result = sincInterpolation(*input, params, ctx);
    REQUIRE(result != nullptr);

    auto const out = result->getAnalogTimeSeries();
    float const output_rate = input_rate * static_cast<float>(factor);

    // Compare against analytical sine at upsampled positions
    // Skip first and last K samples where boundary effects dominate
    int const skip = 8 * factor;// kernel_half_width * factor
    double max_error = 0.0;
    int count = 0;

    for (auto m = static_cast<size_t>(skip); m < out.size() - static_cast<size_t>(skip); ++m) {
        auto const t = static_cast<float>(m) / output_rate;
        auto const expected = std::sin(2.0f * std::numbers::pi_v<float> * freq * t);
        auto const error = std::abs(static_cast<double>(out[m]) - static_cast<double>(expected));
        max_error = std::max(max_error, error);
        ++count;
    }

    INFO("Max reconstruction error: " << max_error << " over " << count << " samples");
    // For a 50 Hz signal well below Nyquist, reconstruction should be very good
    REQUIRE(max_error < 0.01);
}

// ============================================================================
// Kernel Width Comparison
// ============================================================================

TEST_CASE("Larger sinc kernel yields better reconstruction",
          "[transforms][v2][sinc][kernel]") {
    auto const ctx = makeCtx();

    float const freq = 100.0f;
    float const input_rate = 500.0f;
    int const n_samples = 200;
    int const factor = 4;

    auto const sine_data = makeSineWave(freq, input_rate, n_samples);
    auto const input = makeATS(sine_data);

    // Small kernel (K=2) vs default kernel (K=8)
    SincInterpolationParams small_params;
    small_params.upsampling_factor = factor;
    small_params.kernel_half_width = 2;

    SincInterpolationParams large_params;
    large_params.upsampling_factor = factor;
    large_params.kernel_half_width = 16;

    auto const small_result = sincInterpolation(*input, small_params, ctx);
    auto const large_result = sincInterpolation(*input, large_params, ctx);

    REQUIRE(small_result != nullptr);
    REQUIRE(large_result != nullptr);

    float const output_rate = input_rate * static_cast<float>(factor);

    // Measure max error in the safe interior region
    int const skip = 16 * factor;
    double small_max_error = 0.0;
    double large_max_error = 0.0;

    auto const small_out = small_result->getAnalogTimeSeries();
    auto const large_out = large_result->getAnalogTimeSeries();

    for (auto m = static_cast<size_t>(skip); m < small_out.size() - static_cast<size_t>(skip); ++m) {
        auto const t = static_cast<float>(m) / output_rate;
        auto const expected = std::sin(2.0f * std::numbers::pi_v<float> * freq * t);
        small_max_error = std::max(small_max_error,
                                   std::abs(static_cast<double>(small_out[m]) - static_cast<double>(expected)));
        large_max_error = std::max(large_max_error,
                                   std::abs(static_cast<double>(large_out[m]) - static_cast<double>(expected)));
    }

    INFO("Small kernel (K=2) max error: " << small_max_error);
    INFO("Large kernel (K=16) max error: " << large_max_error);
    REQUIRE(large_max_error < small_max_error);
}

// ============================================================================
// Boundary Mode Comparison
// ============================================================================

TEST_CASE("Symmetric boundary has smaller edge artifacts than zero padding",
          "[transforms][v2][sinc][boundary]") {
    auto const ctx = makeCtx();

    // Create a constant signal — any deviation is a boundary artifact
    float const dc = 5.0f;
    auto const input = makeATS(std::vector<float>(50, dc));

    SincInterpolationParams sym_params;
    sym_params.upsampling_factor = 4;
    sym_params.boundary_mode = "SymmetricExtension";

    SincInterpolationParams zero_params;
    zero_params.upsampling_factor = 4;
    zero_params.boundary_mode = "ZeroPad";

    auto const sym_result = sincInterpolation(*input, sym_params, ctx);
    auto const zero_result = sincInterpolation(*input, zero_params, ctx);

    REQUIRE(sym_result != nullptr);
    REQUIRE(zero_result != nullptr);

    auto const sym_out = sym_result->getAnalogTimeSeries();
    auto const zero_out = zero_result->getAnalogTimeSeries();

    // Measure max deviation from DC at the edges (first 32 output samples)
    double sym_edge_error = 0.0;
    double zero_edge_error = 0.0;

    for (size_t i = 0; i < 32 && i < sym_out.size(); ++i) {
        sym_edge_error = std::max(sym_edge_error,
                                  std::abs(static_cast<double>(sym_out[i]) - static_cast<double>(dc)));
        zero_edge_error = std::max(zero_edge_error,
                                   std::abs(static_cast<double>(zero_out[i]) - static_cast<double>(dc)));
    }

    INFO("Symmetric edge error: " << sym_edge_error);
    INFO("ZeroPad edge error: " << zero_edge_error);
    REQUIRE(sym_edge_error <= zero_edge_error);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("SincInterpolation edge cases",
          "[transforms][v2][sinc][edge]") {
    auto const ctx = makeCtx();
    SincInterpolationParams params;
    params.upsampling_factor = 4;

    SECTION("Empty input returns nullptr") {
        auto const input = makeATS({});
        REQUIRE(sincInterpolation(*input, params, ctx) == nullptr);
    }

    SECTION("Single sample returns single sample") {
        auto const input = makeATS({42.0f});
        auto const result = sincInterpolation(*input, params, ctx);
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getAnalogTimeSeries()[0],
                     Catch::Matchers::WithinAbs(42.0f, 1e-6f));
    }

    SECTION("Invalid factor (0) returns nullptr") {
        params.upsampling_factor = 0;
        auto const input = makeATS({1.0f, 2.0f});
        REQUIRE(sincInterpolation(*input, params, ctx) == nullptr);
    }

    SECTION("Invalid factor (negative) returns nullptr") {
        params.upsampling_factor = -3;
        auto const input = makeATS({1.0f, 2.0f});
        REQUIRE(sincInterpolation(*input, params, ctx) == nullptr);
    }
}

// ============================================================================
// Pipeline Integration Test
// ============================================================================

TEST_CASE("SincInterpolation works via DataManager pipeline JSON",
          "[transforms][v2][sinc][pipeline]") {
    auto dm = std::make_unique<DataManager>();

    // Create a time frame and input data
    std::vector<int> const times = {0, 100, 200, 300, 400};
    auto const tf = std::make_shared<TimeFrame>(times);
    dm->setTime(TimeKey("source_time"), tf);

    // Create AnalogTimeSeries: simple linear ramp [0, 1, 2, 3, 4]
    std::vector<float> const data = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    auto const input = std::make_shared<AnalogTimeSeries>(std::vector<float>(data), data.size());
    dm->setData<AnalogTimeSeries>("ramp", input, TimeKey("source_time"));

    // Create output TimeFrame with the correct upsampled size: (5-1)*4+1 = 17
    std::vector<int> upsampled_times;
    upsampled_times.reserve(17);
    for (int i = 0; i < 17; ++i) {
        upsampled_times.push_back(i * 25);// 0, 25, 50, ..., 400
    }
    auto const upsampled_tf = std::make_shared<TimeFrame>(upsampled_times);
    dm->setTime(TimeKey("upsampled_time"), upsampled_tf);

    // Pipeline JSON
    nlohmann::json const config = {
            {"steps", {{{"step_id", "upsample"}, {"transform_name", "SincInterpolation"}, {"input_key", "ramp"}, {"output_key", "ramp_upsampled"}, {"output_time_key", "upsampled_time"}, {"parameters", {{"upsampling_factor", 4}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto const result = executor.execute();
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    // Verify output exists and has the right size
    auto const output = dm->getData<AnalogTimeSeries>("ramp_upsampled");
    REQUIRE(output != nullptr);
    REQUIRE(output->getNumSamples() == 17);

    // Verify output is stored under the correct TimeKey
    TimeKey const output_tk = dm->getTimeKey("ramp_upsampled");
    REQUIRE(output_tk.str() == "upsampled_time");

    // Verify original samples are preserved at integer positions
    auto const out = output->getAnalogTimeSeries();
    for (size_t i = 0; i < data.size(); ++i) {
        size_t const out_idx = i * 4;
        INFO("Original sample " << i << " at output index " << out_idx);
        REQUIRE_THAT(out[out_idx], Catch::Matchers::WithinAbs(data[i], 1e-6f));
    }
}

// ============================================================================
// Window Type Tests
// ============================================================================

TEST_CASE("SincInterpolation all window types produce valid output",
          "[transforms][v2][sinc][windows]") {
    auto const ctx = makeCtx();

    auto const sine_data = makeSineWave(50.0f, 500.0f, 50);
    auto const input = makeATS(sine_data);

    for (auto const & wt: {"Lanczos", "Hann", "Blackman"}) {
        SECTION(std::string("Window: ") + wt) {
            SincInterpolationParams params;
            params.upsampling_factor = 4;
            params.window_type = std::string(wt);

            auto const result = sincInterpolation(*input, params, ctx);
            REQUIRE(result != nullptr);
            REQUIRE(result->getNumSamples() == 197);// (50-1)*4+1

            // All outputs should be finite
            auto const out = result->getAnalogTimeSeries();
            for (size_t i = 0; i < out.size(); ++i) {
                INFO("Window=" << wt << " sample=" << i);
                REQUIRE(std::isfinite(out[i]));
            }
        }
    }
}
