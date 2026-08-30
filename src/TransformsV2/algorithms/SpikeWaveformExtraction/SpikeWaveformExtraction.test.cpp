/**
 * @file SpikeWaveformExtraction.test.cpp
 * @brief Unit tests for SpikeWaveformExtraction transform.
 */

#include "SpikeWaveformExtraction.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "core/ComputeContext.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace {

[[nodiscard]] std::shared_ptr<TensorData> createSyntheticVoltageData(
        size_t num_samples,
        size_t num_channels,
        std::vector<std::pair<size_t, int>> const & spikes) {

    std::vector<float> flat_data(num_samples * num_channels, 0.0f);

    for (auto const & [t_spike, ch]: spikes) {
        if (t_spike + 10 < num_samples && ch >= 0 && static_cast<size_t>(ch) < num_channels) {
            flat_data[(t_spike + 0) * num_channels + static_cast<size_t>(ch)] = -2.0f;
            flat_data[(t_spike + 1) * num_channels + static_cast<size_t>(ch)] = -15.0f;// Trough
            flat_data[(t_spike + 2) * num_channels + static_cast<size_t>(ch)] = -5.0f;
            flat_data[(t_spike + 3) * num_channels + static_cast<size_t>(ch)] = +8.0f; // Rebound
            flat_data[(t_spike + 4) * num_channels + static_cast<size_t>(ch)] = +2.0f;
        }
    }

    auto time_storage = std::make_shared<DenseTimeIndexStorage>(TimeFrameIndex{0}, num_samples);
    return std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(
                    flat_data, num_samples, num_channels, time_storage, nullptr, {}));
}

}// namespace

TEST_CASE("SpikeWaveformExtraction handles empty inputs", "[TransformsV2][SpikeWaveformExtraction]") {
    TensorData empty_voltage;
    DigitalEventSeries empty_events;
    Neuralyzer::Transforms::V2::SpikeWaveformExtractionParams params;
    Neuralyzer::Transforms::V2::ComputeContext ctx;

    auto result = Neuralyzer::Transforms::V2::extractSpikeWaveforms(
            empty_voltage, empty_events, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->numRows() == 0);
}

TEST_CASE("SpikeWaveformExtraction extracts multichannel waveform tensor", "[TransformsV2][SpikeWaveformExtraction]") {
    size_t const num_samples = 1000;
    size_t const num_channels = 4;
    std::vector<std::pair<size_t, int>> const spikes = {
            {100, 0},
            {300, 1},
            {600, 0}};

    auto const voltage = createSyntheticVoltageData(num_samples, num_channels, spikes);

    std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex{100},
            TimeFrameIndex{300},
            TimeFrameIndex{600}};
    DigitalEventSeries event_series(event_times);

    Neuralyzer::Transforms::V2::SpikeWaveformExtractionParams params;
    params.pre_window_ms = 0.50f; // 15 samples @ 30 kHz
    params.post_window_ms = 1.00f;// 30 samples @ 30 kHz
    params.sampling_rate_hz = 30000.0f;
    params.alignment_mode = Neuralyzer::Transforms::V2::WaveformAlignmentMode::OriginalEventTime;

    Neuralyzer::Transforms::V2::ComputeContext ctx;
    auto const waveforms = Neuralyzer::Transforms::V2::extractSpikeWaveforms(
            *voltage, event_series, params, ctx);

    REQUIRE(waveforms != nullptr);
    REQUIRE(waveforms->numRows() == 3);

    // L = 15 + 30 + 1 = 46 points per channel -> 4 channels * 46 = 184 columns
    size_t const expected_cols = 4 * 46;
    REQUIRE(waveforms->numColumns() == expected_cols);

    // Check column naming
    auto const & col_names = waveforms->columnNames();
    REQUIRE(col_names.size() == expected_cols);
    REQUIRE(col_names[0].starts_with("ch00_"));
    REQUIRE(col_names[46].starts_with("ch01_"));
}

TEST_CASE("SpikeWaveformExtraction filters by channels of interest", "[TransformsV2][SpikeWaveformExtraction]") {
    size_t const num_samples = 1000;
    size_t const num_channels = 4;
    std::vector<std::pair<size_t, int>> const spikes = {
            {100, 0},// On channel 0
            {300, 3},// On channel 3
            {600, 0} // On channel 0
    };

    auto const voltage = createSyntheticVoltageData(num_samples, num_channels, spikes);

    std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex{100},
            TimeFrameIndex{300},
            TimeFrameIndex{600}};
    DigitalEventSeries event_series(event_times);

    Neuralyzer::Transforms::V2::SpikeWaveformExtractionParams params;
    params.pre_window_ms = 0.20f; // 6 samples
    params.post_window_ms = 0.40f;// 12 samples
    params.channels_of_interest = {0, 1};
    params.filter_by_primary_channel = true;

    Neuralyzer::Transforms::V2::ComputeContext ctx;
    auto const waveforms = Neuralyzer::Transforms::V2::extractSpikeWaveforms(
            *voltage, event_series, params, ctx);

    REQUIRE(waveforms != nullptr);
    // Should filter out the spike on channel 3, leaving 2 spikes on channel 0
    REQUIRE(waveforms->numRows() == 2);
    // 2 selected channels * (6 + 12 + 1) = 38 columns
    REQUIRE(waveforms->numColumns() == 2 * 19);
}

TEST_CASE("SpikeWaveformExtractionParams schema extraction", "[TransformsV2][SpikeWaveformExtraction]") {
    auto schema = extractParameterSchema<Neuralyzer::Transforms::V2::SpikeWaveformExtractionParams>();

    auto const * pre_field = schema.field("pre_window_ms");
    REQUIRE(pre_field != nullptr);
    REQUIRE(!pre_field->tooltip.empty());

    auto const * ch_field = schema.field("channels_of_interest");
    REQUIRE(ch_field != nullptr);
}
