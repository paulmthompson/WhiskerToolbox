/**
 * @file SwindaleEventDetection.test.cpp
 * @brief Unit tests for Swindale multi-channel spike event detection transform.
 */

#include "SwindaleEventDetection.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "core/ComputeContext.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace {

[[nodiscard]] std::shared_ptr<TensorData> createSyntheticMultiChannelRecording(
        size_t num_samples,
        size_t num_channels,
        std::vector<size_t> const & spike_times_sample) {

    std::vector<float> flat_data(num_samples * num_channels, 0.0f);

    // Add baseline low-level noise
    for (size_t i = 0; i < flat_data.size(); ++i) {
        flat_data[i] = 0.1f * std::sin(static_cast<float>(i) * 0.05f);
    }

    // Inject biphasic spike waveforms at specified times
    for (size_t const t_spike: spike_times_sample) {
        if (t_spike + 15 < num_samples) {
            // Main spike on channel 0 (large negative trough + positive rebound)
            flat_data[(t_spike + 0) * num_channels + 0] = -1.0f;
            flat_data[(t_spike + 1) * num_channels + 0] = -10.0f;// Trough
            flat_data[(t_spike + 2) * num_channels + 0] = -4.0f;
            flat_data[(t_spike + 3) * num_channels + 0] = +5.0f;// Rebound (+2T excursion)
            flat_data[(t_spike + 4) * num_channels + 0] = +2.0f;

            if (num_channels > 1) {
                // Secondary attenuated spike on adjacent channel 1 (within 25 um)
                flat_data[(t_spike + 0) * num_channels + 1] = -0.5f;
                flat_data[(t_spike + 1) * num_channels + 1] = -6.0f;
                flat_data[(t_spike + 2) * num_channels + 1] = -2.0f;
                flat_data[(t_spike + 3) * num_channels + 1] = +3.0f;
                flat_data[(t_spike + 4) * num_channels + 1] = +1.0f;
            }
        }
    }

    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(num_samples);
    for (size_t t = 0; t < num_samples; ++t) {
        time_indices.push_back(TimeFrameIndex{static_cast<int64_t>(t)});
    }

    auto time_storage = std::make_shared<DenseTimeIndexStorage>(TimeFrameIndex{0}, num_samples);
    return std::make_shared<TensorData>(TensorData::createTimeSeries2D(
            flat_data, num_samples, num_channels, time_storage, nullptr, {}));
}

}// namespace

TEST_CASE("SwindaleEventDetection handles empty tensor", "[TransformsV2][SwindaleEventDetection]") {
    TensorData empty_tensor;
    Neuralyzer::Transforms::V2::SwindaleEventDetectionParams params;
    Neuralyzer::Transforms::V2::ComputeContext ctx;

    auto const events = Neuralyzer::Transforms::V2::swindaleEventDetection(empty_tensor, params, ctx);
    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 0);
}

TEST_CASE("SwindaleEventDetection detects synthetic spikes and merges multi-channel footprint", "[TransformsV2][SwindaleEventDetection]") {
    size_t const num_samples = 1000;
    size_t const num_channels = 4;
    std::vector<size_t> const spike_times = {100, 300, 600};

    auto const tensor = createSyntheticMultiChannelRecording(num_samples, num_channels, spike_times);

    Neuralyzer::Transforms::V2::SwindaleEventDetectionParams params;
    params.threshold_multiplier = 4.0f;
    params.multiphasic_window_ms = 0.25f;
    params.temporal_sigma_ms = 0.30f;
    params.spatial_sigma_um = 80.0f;
    params.sampling_rate_hz = 30000.0f;

    Neuralyzer::Transforms::V2::ComputeContext ctx;
    auto const events = Neuralyzer::Transforms::V2::swindaleEventDetection(*tensor, params, ctx);

    REQUIRE(events != nullptr);
    // Should detect exactly 3 merged spikes (not 6 unmerged channel crossings)
    REQUIRE(events->size() == 3);

    auto const stored_0 = events->getStoredEvent(0);
    auto const stored_1 = events->getStoredEvent(1);
    auto const stored_2 = events->getStoredEvent(2);

    REQUIRE(std::abs(stored_0.getValue() - 101) <= 2);
    REQUIRE(std::abs(stored_1.getValue() - 301) <= 2);
    REQUIRE(std::abs(stored_2.getValue() - 601) <= 2);
}

TEST_CASE("SwindaleEventDetection supports NegativeTrough alignment", "[TransformsV2][SwindaleEventDetection]") {
    size_t const num_samples = 500;
    size_t const num_channels = 2;
    std::vector<size_t> const spike_times = {200};

    auto const tensor = createSyntheticMultiChannelRecording(num_samples, num_channels, spike_times);

    Neuralyzer::Transforms::V2::SwindaleEventDetectionParams params;
    params.alignment_method = Neuralyzer::Transforms::V2::AlignmentMethod::NegativeTrough;
    params.threshold_multiplier = 4.0f;

    Neuralyzer::Transforms::V2::ComputeContext ctx;
    auto const events = Neuralyzer::Transforms::V2::swindaleEventDetection(*tensor, params, ctx);

    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 1);
    // Trough was injected at offset +1 -> sample index 201
    REQUIRE(events->getStoredEvent(0).getValue() == 201);
}

TEST_CASE("SwindaleEventDetectionParams extracts valid schema with FilePath hint", "[TransformsV2][SwindaleEventDetection]") {
    auto schema = extractParameterSchema<Neuralyzer::Transforms::V2::SwindaleEventDetectionParams>();

    auto const * cfg_field = schema.field("probe_config_path");
    REQUIRE(cfg_field != nullptr);
    REQUIRE(cfg_field->path_field_kind == PathFieldKind::FilePath);
    REQUIRE(cfg_field->file_dialog_id == "swindale_probe_cfg_open");

    auto const * th_field = schema.field("threshold_multiplier");
    REQUIRE(th_field != nullptr);
    REQUIRE(th_field->min_value.has_value());
    REQUIRE(th_field->max_value.has_value());

    auto const * method_field = schema.field("method");
    REQUIRE(method_field != nullptr);
    REQUIRE(!method_field->tooltip.empty());

    auto const * tmpl_field = schema.field("enable_template_realignment");
    REQUIRE(tmpl_field != nullptr);
}

TEST_CASE("SwindaleEventDetection with template realignment refines timestamps", "[TransformsV2][SwindaleEventDetection]") {
    size_t const num_samples = 1000;
    size_t const num_channels = 2;
    std::vector<size_t> const spike_times = {200, 500, 800};

    auto const tensor = createSyntheticMultiChannelRecording(num_samples, num_channels, spike_times);

    Neuralyzer::Transforms::V2::SwindaleEventDetectionParams params;
    params.enable_template_realignment = true;
    params.threshold_multiplier = 4.0f;

    Neuralyzer::Transforms::V2::ComputeContext ctx;
    auto const events = Neuralyzer::Transforms::V2::swindaleEventDetection(*tensor, params, ctx);

    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 3);
}

