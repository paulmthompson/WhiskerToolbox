/**
 * @file test_design_preset_registry.cpp
 * @brief Tests for TensorDesign design authoring preset expansion.
 */

#include "TensorDesign/DesignPresetRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

using Neuralyzer::TensorDesign::createBuiltInDesignPresetRegistry;
using Neuralyzer::TensorDesign::DesignPresetArgs;
using Neuralyzer::TensorDesign::DesignPresetSource;
using Neuralyzer::TensorDesign::RowType;

namespace {

template<typename T>
T requireValue(std::optional<T> const & opt) {
    if (!opt.has_value()) {
        throw std::runtime_error("expected optional value");
    }
    return opt.value();
}

}// namespace

TEST_CASE("whisker_contact_feature_table expands to raw TensorDesignSpec", "[TensorDesign][design-presets]") {
    auto registry = createBuiltInDesignPresetRegistry();

    auto const * descriptor = registry.find("whisker_contact_feature_table");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->display_name == "Whisker contact feature table");
    CHECK(descriptor->source == DesignPresetSource::BuiltIn);
    REQUIRE(descriptor->parameters.field("row_source_key") != nullptr);
    REQUIRE(descriptor->parameters.field("keypoint_source_keys") != nullptr);

    auto expansion = requireValue(registry.expand(
            "whisker_contact_feature_table",
            DesignPresetArgs{
                    .row_source_key = "contacts",
                    .curvature_source_key = "curvature",
                    .spike_source_key = "spikes",
                    .angle_source_key = "angle",
                    .keypoint_source_keys = {"tip"},
                    .onset_pre = 2,
                    .onset_post = 3}));

    CHECK(expansion.spec.row_type == RowType::Interval);
    CHECK(expansion.spec.row_source_key == "contacts");
    REQUIRE(expansion.spec.columns.size() == 6);
    CHECK(expansion.spec.columns[0].column_name == "mean_curvature");
    CHECK(expansion.spec.columns[0].source_key == "curvature");
    CHECK(expansion.spec.columns[1].column_name == "spike_count");
    CHECK(expansion.spec.columns[1].source_key == "spikes");
    CHECK(expansion.spec.columns[2].column_name == "spike_presence_at_onset");
    CHECK(expansion.spec.columns[2].row_pipeline_json.find("EventToInterval") != std::string::npos);
    CHECK(expansion.spec.columns[3].column_name == "angle_at_onset");
    CHECK(expansion.spec.columns[3].row_pipeline_json.find("IntervalToEvent") != std::string::npos);
    CHECK(expansion.spec.columns[4].column_name == "tip_x");
    CHECK(expansion.spec.columns[5].column_name == "tip_y");
}

TEST_CASE("whisker_contact_feature_table rejects missing required keys", "[TensorDesign][design-presets]") {
    auto registry = createBuiltInDesignPresetRegistry();

    CHECK_FALSE(registry.expand("whisker_contact_feature_table", DesignPresetArgs{}).has_value());
}
