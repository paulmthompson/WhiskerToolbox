/**
 * @file test_tensor_whisker_contact_scenario.cpp
 * @brief Integration tests building tensors from the whisker-contact scenario via TensorDesign.
 */

#include "TensorDesign/TensorDesignBuilder.hpp"

#include "fixtures/whisker_contact_test_fixture.hpp"

#include "Tensors/TensorData.hpp"

#include <catch2/catch_test_macros.hpp>

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
    }
  ]
})";
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
    REQUIRE(built.numColumns() == 2);

    auto const mean_curvature = built.getColumn(0);
    auto const spike_counts = built.getColumn(1);
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
