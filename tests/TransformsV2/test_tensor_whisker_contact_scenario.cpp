/**
 * @file test_tensor_whisker_contact_scenario.cpp
 * @brief Integration tests building tensors from the whisker-contact scenario fixture.
 */

#include "TensorDesign/TensorDesignBuilder.hpp"

#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include "fixtures/whisker_contact_test_fixture.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include "TransformsV2/core/TransformPipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <any>
#include <cmath>
#include <numeric>
#include <vector>

using namespace Neuralyzer::TensorBuilders;
using namespace Neuralyzer::TensorDesign;
using Neuralyzer::Transforms::V2::TransformPipeline;

TEST_CASE("Whisker contact scenario builds cross-timeframe tensor",
          "[TensorColumnBuilders][whisker_contact]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 10.0;
    WhiskerContactTestFixture fixture(cfg);

    auto const & scenario = fixture.scenario();
    auto const num_rows = scenario.contact->size();
    REQUIRE(num_rows > 0);

    auto mean_pipeline = TransformPipeline();
    mean_pipeline.setRangeReductionErased("MeanValue", std::any{});
    auto mean_provider = buildIntervalPipelineProvider(
            fixture.dm(),
            scenario.config.curvature_key,
            scenario.contact,
            std::move(mean_pipeline));

    auto count_pipeline = TransformPipeline();
    count_pipeline.setRangeReductionErased("EventCount", std::any{});
    auto spike_count_provider = buildIntervalPipelineProvider(
            fixture.dm(),
            scenario.config.spikes_key,
            scenario.contact,
            std::move(count_pipeline));

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"mean_curvature", std::move(mean_provider), {}});
    columns.push_back(ColumnSource{"spike_count", std::move(spike_count_provider), {}});

    auto wiring = buildInvalidationWiringFn(
            fixture.dm(),
            {scenario.config.curvature_key, scenario.config.spikes_key});

    std::vector<TimeFrameInterval> tfi_intervals;
    tfi_intervals.reserve(num_rows);
    for (auto const & iw: scenario.contact->view()) {
        tfi_intervals.push_back(TimeFrameInterval{
                TimeFrameIndex(iw.interval.start),
                TimeFrameIndex(iw.interval.end)});
    }
    auto row_desc = RowDescriptor::fromIntervals(
            std::move(tfi_intervals), scenario.contact->getTimeFrame());

    auto tensor = TensorData::createFromLazyColumns(
            num_rows,
            std::move(columns),
            std::move(row_desc),
            wiring);

    REQUIRE(tensor.numRows() == num_rows);
    REQUIRE(tensor.numColumns() == 2);

    auto const mean_curvature = tensor.getColumn(0);
    auto const spike_counts = tensor.getColumn(1);
    REQUIRE(mean_curvature.size() == num_rows);
    REQUIRE(spike_counts.size() == num_rows);

    for (std::size_t row = 0; row < num_rows; ++row) {
        REQUIRE(std::isfinite(mean_curvature[row]));
        REQUIRE(spike_counts[row] >= 0.0f);
    }

    double const total_spikes = std::accumulate(
            spike_counts.begin(), spike_counts.end(), 0.0);
    REQUIRE(total_spikes > 0.0);

    double const mean_spikes_per_contact = total_spikes / static_cast<double>(num_rows);
    REQUIRE(mean_spikes_per_contact > 1.0);
}

TEST_CASE("Whisker contact scenario curvature mean differs across intervals",
          "[TensorColumnBuilders][whisker_contact]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 20.0;
    WhiskerContactTestFixture fixture(cfg);

    auto const & scenario = fixture.scenario();
    auto const num_rows = scenario.contact->size();
    REQUIRE(num_rows > 5);

    auto mean_pipeline = TransformPipeline();
    mean_pipeline.setRangeReductionErased("MeanValue", std::any{});
    auto mean_provider = buildIntervalPipelineProvider(
            fixture.dm(),
            scenario.config.curvature_key,
            scenario.contact,
            std::move(mean_pipeline));

    auto values = mean_provider();
    REQUIRE(values.size() == num_rows);

    float min_val = values.front();
    float max_val = values.front();
    for (float const v: values) {
        min_val = std::min(min_val, v);
        max_val = std::max(max_val, v);
    }
    REQUIRE(max_val - min_val > 0.01f);
}
