/**
 * @file test_tensor_whisker_contact_scenario.cpp
 * @brief Integration tests building tensors from the whisker-contact scenario via TensorDesign.
 */

#include "TensorDesign/TensorDesignBuilder.hpp"

#include "fixtures/whisker_contact_test_fixture.hpp"

#include "Tensors/TensorData.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>

using namespace Neuralyzer::TensorDesign;

namespace {

template<typename T>
T requireValue(std::optional<T> const & opt) {
    if (!opt.has_value()) {
        throw std::runtime_error("expected optional value");
    }
    return opt.value();
}

std::string whiskerContactDesignJson(
        std::string const & contact_key,
        std::string const & curvature_key,
        std::string const & spikes_key) {
    return R"({
  "tensor_key": "contact_features",
  "row_source": {
    "data_key": ")" +
           contact_key + R"(",
    "row_type": "interval"
  },
  "columns": [
    {
      "name": "mean_curvature",
      "source_key": ")" +
           curvature_key + R"(",
      "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"MeanValue\"}}"
    },
    {
      "name": "spike_count",
      "source_key": ")" +
           spikes_key + R"(",
      "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
    },
    {
      "name": "onset_spike_present",
      "source_key": ")" +
           spikes_key + R"(",
      "row_pipeline_json": "{\"steps\": [{\"step_id\": \"contact_start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"onset_window\", \"transform_name\": \"EventToInterval\", \"parameters\": {\"pre_expansion\": 0, \"post_expansion\": 1}}]}",
      "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventPresence\"}}"
    }
  ]
})";
}

/**
 * @brief Compute whether the scenario has any spike in the first contact frame.
 * @pre scenario.contact and scenario.spikes must have valid TimeFrames.
 * @param scenario Generated whisker-contact data.
 * @param contact_interval Contact interval expressed on the scenario Time clock.
 * @return 1.0 when at least one spike falls in [start, start + 1], otherwise 0.0.
 * @post Does not mutate scenario data.
 */
[[nodiscard]] float expectedOnsetSpikePresence(
        WhiskerContactScenario const & scenario,
        Interval const & contact_interval) {
    auto const time_frame = scenario.contact->getTimeFrame();
    if (!time_frame) {
        throw std::runtime_error("expected contact TimeFrame");
    }

    auto const onset_events = scenario.spikes->viewInRange(
            TimeFrameIndex(contact_interval.start),
            TimeFrameIndex(contact_interval.start + 1),
            *time_frame);

    for ([[maybe_unused]] auto const & event: onset_events) {
        return 1.0f;
    }
    return 0.0f;
}

}// namespace

TEST_CASE("Whisker contact scenario builds tensor from design JSON",
          "[TensorDesign][whisker_contact]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 10.0;
    WhiskerContactTestFixture fixture(cfg);

    auto const & scenario = fixture.scenario();
    auto const num_rows = scenario.contact->size();
    REQUIRE(num_rows > 0);

    auto const json = whiskerContactDesignJson(
            scenario.config.contact_key,
            scenario.config.curvature_key,
            scenario.config.spikes_key);

    auto const built = requireValue(buildTensorFromDesignJson(fixture.dm(), json));
    REQUIRE(built.numRows() == num_rows);
    REQUIRE(built.numColumns() == 3);

    auto const mean_curvature = built.getColumn(0);
    auto const spike_counts = built.getColumn(1);
    auto const onset_spike_present = built.getColumn(2);
    REQUIRE(mean_curvature.size() == num_rows);
    REQUIRE(spike_counts.size() == num_rows);
    REQUIRE(onset_spike_present.size() == num_rows);

    double onset_spike_contacts = 0.0;
    for (std::size_t row_index = 0; row_index < num_rows; ++row_index) {
        REQUIRE(std::isfinite(mean_curvature[row_index]));
        REQUIRE(spike_counts[row_index] >= 0.0f);
        REQUIRE((onset_spike_present[row_index] == 0.0f ||
                 onset_spike_present[row_index] == 1.0f));
    }

    std::size_t row_index = 0;
    for (auto const & contact_with_id: scenario.contact->view()) {
        auto const expected_presence = expectedOnsetSpikePresence(
                scenario, contact_with_id.interval);
        REQUIRE(onset_spike_present[row_index] == expected_presence);
        onset_spike_contacts += static_cast<double>(onset_spike_present[row_index]);
        ++row_index;
    }
    REQUIRE(row_index == num_rows);

    double const total_spikes = std::accumulate(
            spike_counts.begin(), spike_counts.end(), 0.0);
    REQUIRE(total_spikes > 0.0);

    double const mean_spikes_per_contact = total_spikes / static_cast<double>(num_rows);
    REQUIRE(mean_spikes_per_contact > 1.0);
    REQUIRE(onset_spike_contacts > 0.0);
}
