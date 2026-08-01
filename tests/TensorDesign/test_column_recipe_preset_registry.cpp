/**
 * @file test_column_recipe_preset_registry.cpp
 * @brief Tests for TensorDesign column recipe preset expansion.
 */

#include "TensorDesign/ColumnRecipePresetRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

using Neuralyzer::TensorDesign::ColumnRecipePresetArgs;
using Neuralyzer::TensorDesign::ColumnRecipePresetSource;
using Neuralyzer::TensorDesign::createBuiltInColumnRecipePresetRegistry;

namespace {

template<typename T>
T requireValue(std::optional<T> const & opt) {
    if (!opt.has_value()) {
        throw std::runtime_error("expected optional value");
    }
    return opt.value();
}

}// namespace

TEST_CASE("mean_over_interval preset expands to raw ColumnRecipe", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto const * descriptor = registry.find("mean_over_interval");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->id == "mean_over_interval");
    CHECK(descriptor->display_name == "Mean over interval");
    CHECK(descriptor->source == ColumnRecipePresetSource::BuiltIn);
    REQUIRE(descriptor->parameters.field("output_name") != nullptr);
    REQUIRE(descriptor->parameters.field("source_key") != nullptr);

    auto expansion = requireValue(registry.expand(
            "mean_over_interval",
            ColumnRecipePresetArgs{
                    .output_name = "mean_curvature",
                    .source_key = "Curvature"}));

    REQUIRE(expansion.columns.size() == 1);

    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "mean_curvature");
    CHECK(column.source_key == "Curvature");
    CHECK(column.row_pipeline_json.empty());
    CHECK(column.pipeline_value_bindings.empty());
    CHECK_FALSE(column.interval_property.has_value());
    CHECK(column.pipeline_json == R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})");
}

TEST_CASE("event_count_over_interval preset expands to raw ColumnRecipe", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto expansion = requireValue(registry.expand(
            "event_count_over_interval",
            ColumnRecipePresetArgs{
                    .output_name = "spike_count",
                    .source_key = "Spikes"}));

    REQUIRE(expansion.columns.size() == 1);

    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "spike_count");
    CHECK(column.source_key == "Spikes");
    CHECK(column.row_pipeline_json.empty());
    CHECK(column.pipeline_json == R"({"steps": [], "range_reduction": {"reduction_name": "EventCount"}})");
}

TEST_CASE("analog_sample_at_interval_start preset expands to row pipeline recipe",
          "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto const * descriptor = registry.find("analog_sample_at_interval_start");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->id == "analog_sample_at_interval_start");
    CHECK(descriptor->display_name == "Analog sample at interval start");
    CHECK(descriptor->source == ColumnRecipePresetSource::BuiltIn);
    REQUIRE(descriptor->parameters.field("output_name") != nullptr);
    REQUIRE(descriptor->parameters.field("source_key") != nullptr);

    auto expansion = requireValue(registry.expand(
            "analog_sample_at_interval_start",
            ColumnRecipePresetArgs{
                    .output_name = "angle_at_onset",
                    .source_key = "Angle"}));

    REQUIRE(expansion.columns.size() == 1);

    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "angle_at_onset");
    CHECK(column.source_key == "Angle");
    CHECK(column.row_pipeline_json ==
          R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})");
    CHECK(column.pipeline_json == R"({"steps": []})");
    CHECK(column.pipeline_value_bindings.empty());
    CHECK_FALSE(column.interval_property.has_value());
}

TEST_CASE("event_presence_around_interval_start preset expands to row pipeline recipe",
          "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto expansion = requireValue(registry.expand(
            "event_presence_around_interval_start",
            ColumnRecipePresetArgs{
                    .output_name = "onset_spike_present",
                    .source_key = "Spikes",
                    .pre = 2,
                    .post = 3}));

    REQUIRE(expansion.columns.size() == 1);

    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "onset_spike_present");
    CHECK(column.source_key == "Spikes");
    CHECK(column.row_pipeline_json ==
          R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}, {"step_id": "window", "transform_name": "EventToInterval", "parameters": {"pre_expansion": 2, "post_expansion": 3}}]})");
    CHECK(column.pipeline_json == R"({"steps": [], "range_reduction": {"reduction_name": "EventPresence"}})");
}

TEST_CASE("point_xy preset expands to x and y ColumnRecipes", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto expansion = requireValue(registry.expand(
            "point_xy",
            ColumnRecipePresetArgs{
                    .source_key = "Nose",
                    .name_prefix = "nose"}));

    REQUIRE(expansion.columns.size() == 2);

    CHECK(expansion.columns[0].column_name == "nose_x");
    CHECK(expansion.columns[0].source_key == "Nose");
    CHECK(expansion.columns[0].pipeline_json ==
          R"({"steps": [{"step_id": "x", "transform_name": "PointCoordinate", "parameters": {"coordinate": "X"}}]})");

    CHECK(expansion.columns[1].column_name == "nose_y");
    CHECK(expansion.columns[1].source_key == "Nose");
    CHECK(expansion.columns[1].pipeline_json ==
          R"({"steps": [{"step_id": "y", "transform_name": "PointCoordinate", "parameters": {"coordinate": "Y"}}]})");
}

TEST_CASE("ColumnRecipePresetRegistry rejects duplicate preset ids", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto const * existing = registry.find("mean_over_interval");
    REQUIRE(existing != nullptr);

    CHECK_FALSE(registry.registerPreset(*existing));
}
