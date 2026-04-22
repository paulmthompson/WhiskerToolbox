/**
 * @file TensorWhitening.test.cpp
 * @brief Catch2 tests for the TensorWhitening container transform.
 */

#include "TransformsV2/algorithms/TensorWhitening/TensorWhitening.hpp"

#include "Tensors/TensorData.hpp"
#include "Tensors/storage/TensorStorageBase.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

namespace {

auto makeCtx() -> ComputeContext {
    return ComputeContext{};
}

auto makeTimeFrame(std::size_t count) -> std::shared_ptr<TimeFrame> {
    std::vector<int> times(count);
    for (std::size_t index = 0; index < count; ++index) {
        times[index] = static_cast<int>(index);
    }
    return std::make_shared<TimeFrame>(times);
}

auto makeDenseTimeStorage(std::size_t count) -> std::shared_ptr<TimeIndexStorage> {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

auto makeCorrelatedFlat(std::size_t num_rows) -> std::vector<float> {
    std::vector<float> flat(num_rows * 3U);
    for (std::size_t row = 0; row < num_rows; ++row) {
        auto const t = static_cast<double>(row);
        double const latent_a = std::sin(0.11 * t);
        double const latent_b = std::cos(0.07 * t);
        double const latent_c = std::sin(0.19 * t + 0.3);

        flat[row * 3U + 0U] = static_cast<float>(latent_a + 0.25 * latent_b);
        flat[row * 3U + 1U] = static_cast<float>(2.0 * latent_a - 0.4 * latent_b + 0.08 * latent_c);
        flat[row * 3U + 2U] = static_cast<float>(-0.7 * latent_a + 0.6 * latent_b + 0.15 * latent_c);
    }
    return flat;
}

auto makeTimeSeriesTensor(
        std::vector<float> const & flat,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<std::string> column_names = {}) -> TensorData {
    return TensorData::createTimeSeries2D(
            flat,
            num_rows,
            num_cols,
            makeDenseTimeStorage(num_rows),
            makeTimeFrame(num_rows + 16U),
            std::move(column_names));
}

auto makeIntervalTensor(
        std::vector<float> const & flat,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<std::string> column_names = {}) -> TensorData {
    std::vector<TimeFrameInterval> intervals;
    intervals.reserve(num_rows);
    for (std::size_t row = 0; row < num_rows; ++row) {
        auto const start = TimeFrameIndex(static_cast<int64_t>(row * 10U));
        auto const end = TimeFrameIndex(static_cast<int64_t>(row * 10U + 9U));
        intervals.push_back({start, end});
    }

    return TensorData::createFromIntervals(
            flat,
            num_rows,
            num_cols,
            std::move(intervals),
            makeTimeFrame(num_rows * 10U + 20U),
            std::move(column_names));
}

auto computeCovariance(
        TensorData const & tensor,
        std::vector<std::size_t> const & channels,
        std::size_t row_count = 0) -> std::vector<double> {
    auto const total_rows = tensor.numRows();
    auto const rows_to_use = row_count == 0 ? total_rows : std::min(row_count, total_rows);
    auto const total_cols = tensor.numColumns();
    auto const flat = tensor.materializeFlat();
    auto const num_channels = channels.size();

    std::vector<double> means(num_channels, 0.0);
    for (std::size_t row = 0; row < rows_to_use; ++row) {
        for (std::size_t channel = 0; channel < num_channels; ++channel) {
            means[channel] +=
                    static_cast<double>(flat[row * total_cols + channels[channel]]);
        }
    }
    for (double & mean: means) {
        mean /= static_cast<double>(rows_to_use);
    }

    std::vector<double> covariance(num_channels * num_channels, 0.0);
    for (std::size_t row = 0; row < rows_to_use; ++row) {
        for (std::size_t lhs = 0; lhs < num_channels; ++lhs) {
            double const lhs_value =
                    static_cast<double>(flat[row * total_cols + channels[lhs]]) - means[lhs];
            for (std::size_t rhs = 0; rhs < num_channels; ++rhs) {
                double const rhs_value =
                        static_cast<double>(flat[row * total_cols + channels[rhs]]) - means[rhs];
                covariance[lhs * num_channels + rhs] += lhs_value * rhs_value;
            }
        }
    }

    auto const scale = static_cast<double>(rows_to_use - 1U);
    for (double & value: covariance) {
        value /= scale;
    }
    return covariance;
}

auto covarianceIdentityError(std::vector<double> const & covariance, std::size_t size) -> double {
    double error = 0.0;
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t col = 0; col < size; ++col) {
            double const expected = row == col ? 1.0 : 0.0;
            error += std::abs(covariance[row * size + col] - expected);
        }
    }
    return error;
}

auto meanChannelStd(TensorData const & tensor, std::vector<std::size_t> const & channels) -> double {
    auto const covariance = computeCovariance(tensor, channels);
    double sum_std = 0.0;
    for (std::size_t index = 0; index < channels.size(); ++index) {
        sum_std += std::sqrt(covariance[index * channels.size() + index]);
    }
    return sum_std / static_cast<double>(channels.size());
}

}// namespace

TEST_CASE("TensorWhitening is registered in ElementRegistry", "[TransformsV2][TensorWhitening]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorWhitening"));
}

#ifdef TENSOR_BACKEND_LIBTORCH

TEST_CASE(
        "TensorWhitening whitens included channels on CPU and preserves LibTorch storage",
        "[TransformsV2][TensorWhitening]") {
    auto const input = makeTimeSeriesTensor(
            makeCorrelatedFlat(256),
            256,
            3,
            {"c0", "c1", "c2"});

    TensorWhiteningParams params;
    params.rescale_amplitude = false;
    params.epsilon = 1.0e-6F;
    params.mad_threshold_multiplier = 0.0F;

    auto result = tensorWhitening(input, params, makeCtx());

    REQUIRE(result != nullptr);
    REQUIRE(result->storage().getStorageType() == TensorStorageType::LibTorch);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->columnNames() == input.columnNames());

    auto const covariance = computeCovariance(*result, {0U, 1U, 2U});
    for (std::size_t index = 0; index < 3U; ++index) {
        REQUIRE_THAT(
                covariance[index * 3U + index],
                Catch::Matchers::WithinAbs(1.0, 0.04));
    }
    REQUIRE_THAT(covariance[0U * 3U + 1U], Catch::Matchers::WithinAbs(0.0, 0.03));
    REQUIRE_THAT(covariance[0U * 3U + 2U], Catch::Matchers::WithinAbs(0.0, 0.03));
    REQUIRE_THAT(covariance[1U * 3U + 2U], Catch::Matchers::WithinAbs(0.0, 0.03));
}

TEST_CASE(
        "TensorWhitening preserves interval rows and leaves excluded channels unchanged",
        "[TransformsV2][TensorWhitening]") {
    auto flat = makeCorrelatedFlat(96);
    flat.resize(static_cast<std::size_t>(96U * 4U));
    for (std::size_t row = 0; row < 96U; ++row) {
        flat[row * 4U + 3U] = static_cast<float>(100.0 + 0.5 * static_cast<double>(row));
    }

    auto const input = makeIntervalTensor(
            flat,
            96,
            4,
            {"c0", "c1", "c2", "excluded"});

    TensorWhiteningParams params;
    params.exclude_channels = {3};
    params.rescale_amplitude = false;
    params.mad_threshold_multiplier = 0.0F;

    auto result = tensorWhitening(input, params, makeCtx());

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::Interval);
    REQUIRE(result->storage().getStorageType() == TensorStorageType::LibTorch);
    REQUIRE(result->getTimeFrame().get() == input.getTimeFrame().get());

    auto const input_flat = input.materializeFlat();
    auto const result_flat = result->materializeFlat();
    for (std::size_t row = 0; row < input.numRows(); ++row) {
        REQUIRE_THAT(
                static_cast<double>(result_flat[row * input.numColumns() + 3U]),
                Catch::Matchers::WithinAbs(
                        static_cast<double>(input_flat[row * input.numColumns() + 3U]),
                        1.0e-6));
    }

    auto const covariance = computeCovariance(*result, {0U, 1U, 2U});
    for (std::size_t index = 0; index < 3U; ++index) {
        REQUIRE_THAT(
                covariance[index * 3U + index],
                Catch::Matchers::WithinAbs(1.0, 0.04));
    }
}

TEST_CASE(
        "TensorWhitening robust sample mask reduces outlier influence on whitening",
        "[TransformsV2][TensorWhitening]") {
    auto flat = makeCorrelatedFlat(128);
    flat[(127U * 3U) + 0U] += 5000.0F;
    flat[(127U * 3U) + 1U] -= 4000.0F;
    flat[(127U * 3U) + 2U] += 3500.0F;
    auto const input = makeTimeSeriesTensor(flat, 128, 3);

    TensorWhiteningParams robust_params;
    robust_params.rescale_amplitude = false;
    robust_params.mad_threshold_multiplier = 4.0F;

    TensorWhiteningParams unmasked_params = robust_params;
    unmasked_params.mad_threshold_multiplier = 0.0F;

    auto robust_result = tensorWhitening(input, robust_params, makeCtx());
    auto unmasked_result = tensorWhitening(input, unmasked_params, makeCtx());

    REQUIRE(robust_result != nullptr);
    REQUIRE(unmasked_result != nullptr);

    auto const robust_covariance = computeCovariance(*robust_result, {0U, 1U, 2U}, 127U);
    auto const unmasked_covariance = computeCovariance(*unmasked_result, {0U, 1U, 2U}, 127U);

    auto const robust_error = covarianceIdentityError(robust_covariance, 3U);
    auto const unmasked_error = covarianceIdentityError(unmasked_covariance, 3U);

    REQUIRE(robust_error < unmasked_error);
    REQUIRE(robust_error < 1.0);
}

TEST_CASE(
        "TensorWhitening rescales amplitude by default",
        "[TransformsV2][TensorWhitening]") {
    auto flat = makeCorrelatedFlat(160);
    for (float & value: flat) {
        value *= 25.0F;
    }
    auto const input = makeTimeSeriesTensor(flat, 160, 3);

    TensorWhiteningParams const default_params;
    TensorWhiteningParams unscaled_params;
    unscaled_params.rescale_amplitude = false;

    auto default_result = tensorWhitening(input, default_params, makeCtx());
    auto unscaled_result = tensorWhitening(input, unscaled_params, makeCtx());

    REQUIRE(default_result != nullptr);
    REQUIRE(unscaled_result != nullptr);
    REQUIRE(default_params.rescale_amplitude);

    auto const default_std = meanChannelStd(*default_result, {0U, 1U, 2U});
    auto const unscaled_std = meanChannelStd(*unscaled_result, {0U, 1U, 2U});
    REQUIRE(default_std > unscaled_std * 10.0);
}

TEST_CASE("TensorWhitening validates basic failure cases", "[TransformsV2][TensorWhitening]") {
    auto const single_row = makeTimeSeriesTensor(makeCorrelatedFlat(1), 1, 3);
    TensorWhiteningParams params;
    REQUIRE(tensorWhitening(single_row, params, makeCtx()) == nullptr);

    auto const input = makeTimeSeriesTensor(makeCorrelatedFlat(32), 32, 3);
    params.exclude_channels = {0, 1, 2};
    REQUIRE(tensorWhitening(input, params, makeCtx()) == nullptr);

    params.exclude_channels.clear();
    params.epsilon = 0.0F;
    REQUIRE(tensorWhitening(input, params, makeCtx()) == nullptr);
}

TEST_CASE("TensorWhitening CPU and GPU agree when CUDA is available", "[TransformsV2][TensorWhitening]") {
    auto const input = makeTimeSeriesTensor(makeCorrelatedFlat(192), 192, 3);

    TensorWhiteningParams cpu_params;
    cpu_params.rescale_amplitude = false;

    TensorWhiteningParams gpu_params = cpu_params;
    gpu_params.use_gpu = true;

    auto cpu_result = tensorWhitening(input, cpu_params, makeCtx());
    auto gpu_result = tensorWhitening(input, gpu_params, makeCtx());

    REQUIRE(cpu_result != nullptr);
    REQUIRE(gpu_result != nullptr);
    REQUIRE(gpu_result->storage().getStorageType() == TensorStorageType::LibTorch);

    auto const cpu_flat = cpu_result->materializeFlat();
    auto const gpu_flat = gpu_result->materializeFlat();
    REQUIRE(cpu_flat.size() == gpu_flat.size());
    for (std::size_t index = 0; index < cpu_flat.size(); ++index) {
        REQUIRE_THAT(
                static_cast<double>(cpu_flat[index]),
                Catch::Matchers::WithinAbs(static_cast<double>(gpu_flat[index]), 1.0e-3));
    }
}

#else

TEST_CASE(
        "TensorWhitening returns null when LibTorch backend is unavailable",
        "[TransformsV2][TensorWhitening]") {
    auto const input = makeTimeSeriesTensor(makeCorrelatedFlat(32), 32, 3);
    TensorWhiteningParams params;
    REQUIRE(tensorWhitening(input, params, makeCtx()) == nullptr);
}

#endif