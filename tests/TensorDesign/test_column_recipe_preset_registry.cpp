/**
 * @file test_column_recipe_preset_registry.cpp
 * @brief Tests for TensorDesign column recipe preset expansion.
 */

#include "TensorDesign/ColumnRecipePresetRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

using Neuralyzer::TensorDesign::ColumnAggregatorRegistry;
using Neuralyzer::TensorDesign::ColumnRecipePresetArgs;
using Neuralyzer::TensorDesign::ColumnRecipePresetSource;
using Neuralyzer::TensorDesign::createBuiltInColumnAggregatorRegistry;
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

TEST_CASE("mean_value aggregator expands to raw ColumnRecipe", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnAggregatorRegistry();

    auto const * descriptor = registry.find("mean_value");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->id == "mean_value");
    CHECK(descriptor->display_name == "Mean Value");
    CHECK(descriptor->source == ColumnRecipePresetSource::BuiltIn);
    REQUIRE(descriptor->parameters.field("output_name") != nullptr);
    REQUIRE(descriptor->parameters.field("source_key") != nullptr);

    auto expansion = requireValue(descriptor->expand(
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

TEST_CASE("event_count aggregator expands to raw ColumnRecipe", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnAggregatorRegistry();

    auto const * descriptor = registry.find("event_count");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
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

TEST_CASE("window_around_interval_start RowModifier expands to row pipeline json", "[TensorDesign][presets]") {
    auto registry = Neuralyzer::TensorDesign::createBuiltInRowModifierRegistry();
    auto const * descriptor = registry.find("window_around_interval_start");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
            ColumnRecipePresetArgs{
                    .pre = 2,
                    .post = 3}));

    CHECK(expansion.row_pipeline_json ==
          R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}, {"step_id": "window", "transform_name": "EventToInterval", "parameters": {"pre_expansion": 2, "post_expansion": 3}}]})");
}

TEST_CASE("point_xy aggregator expands to x and y ColumnRecipes", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnAggregatorRegistry();
    auto const * descriptor = registry.find("point_xy");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
            ColumnRecipePresetArgs{
                    .source_key = "Nose",
                    .name_prefix = "nose"}));

    REQUIRE(expansion.columns.size() == 2);

    CHECK(expansion.columns[0].column_name == "nose_x");
    CHECK(expansion.columns[0].source_key == "Nose");
    CHECK(expansion.columns[0].pipeline_json ==
          R"({"steps": [{"step_id": "x", "transform_name": "PointCoordinateReduction", "parameters": {"coordinate": "X", "reduction": "First"}}]})");

    CHECK(expansion.columns[1].column_name == "nose_y");
    CHECK(expansion.columns[1].source_key == "Nose");
    CHECK(expansion.columns[1].pipeline_json ==
          R"({"steps": [{"step_id": "y", "transform_name": "PointCoordinateReduction", "parameters": {"coordinate": "Y", "reduction": "First"}}]})");
}

TEST_CASE("multi_point_xy aggregator expands multiple keys to x and y ColumnRecipes",
          "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnAggregatorRegistry();
    auto const * descriptor = registry.find("multi_point_xy");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
            ColumnRecipePresetArgs{
                    .source_keys = {"Nose", "Paw"}}));

    REQUIRE(expansion.columns.size() == 4);
    CHECK(expansion.columns[0].column_name == "Nose_x");
    CHECK(expansion.columns[0].source_key == "Nose");
    CHECK(expansion.columns[1].column_name == "Nose_y");
    CHECK(expansion.columns[1].source_key == "Nose");
    CHECK(expansion.columns[2].column_name == "Paw_x");
    CHECK(expansion.columns[2].source_key == "Paw");
    CHECK(expansion.columns[3].column_name == "Paw_y");
    CHECK(expansion.columns[3].source_key == "Paw");
}

TEST_CASE("interval_start RowModifier expands to row pipeline json", "[TensorDesign][presets]") {
    auto registry = Neuralyzer::TensorDesign::createBuiltInRowModifierRegistry();
    auto const * descriptor = registry.find("interval_start");
    REQUIRE(descriptor != nullptr);
    CHECK(descriptor->id == "interval_start");
    CHECK(descriptor->display_name == "At interval start");

    auto expansion = requireValue(descriptor->expand(ColumnRecipePresetArgs{}));
    CHECK(expansion.row_pipeline_json ==
          R"({"steps": [{"step_id": "interval_start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})");
}

TEST_CASE("raster_events_relative aggregator expands to normalized pipeline",
          "[TensorDesign][presets]") {
    auto registry = Neuralyzer::TensorDesign::createBuiltInColumnAggregatorRegistry();
    auto const * descriptor = registry.find("raster_events_relative");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
            ColumnRecipePresetArgs{
                    .output_name = "relative_spikes",
                    .source_key = "spikes",
                    .store_key = "my_alignment_time"}));

    REQUIRE(expansion.columns.size() == 1);
    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "relative_spikes");
    CHECK(column.source_key == "spikes");
    CHECK(column.pipeline_value_bindings.empty());
    CHECK(column.pipeline_json.find("NormalizeDigitalEventSeriesRelative") != std::string::npos);
    CHECK(column.pipeline_json.find("\"alignment_time\": \"my_alignment_time\"") != std::string::npos);
    CHECK(column.pipeline_json.find("EventCountInWindow") == std::string::npos);
}

TEST_CASE("trial_relative_event_count aggregator expands count-in-window pipeline",
          "[TensorDesign][presets]") {
    auto registry = Neuralyzer::TensorDesign::createBuiltInColumnAggregatorRegistry();
    auto const * descriptor = registry.find("trial_relative_event_count");
    REQUIRE(descriptor != nullptr);

    auto expansion = requireValue(descriptor->expand(
            ColumnRecipePresetArgs{
                    .output_name = "early_spike_count",
                    .source_key = "spikes",
                    .store_key = "my_alignment_time",
                    .window_start = 0.0,
                    .window_end = 15.0}));

    REQUIRE(expansion.columns.size() == 1);
    auto const & column = expansion.columns.front();
    CHECK(column.column_name == "early_spike_count");
    CHECK(column.source_key == "spikes");
    CHECK(column.pipeline_value_bindings.empty());
    CHECK(column.pipeline_json.find("NormalizeDigitalEventSeriesRelative") != std::string::npos);
    CHECK(column.pipeline_json.find("\"alignment_time\": \"my_alignment_time\"") != std::string::npos);
    CHECK(column.pipeline_json.find("EventCountInWindow") != std::string::npos);
    CHECK(column.pipeline_json.find("\"window_start\": 0.000000") != std::string::npos);
    CHECK(column.pipeline_json.find("\"window_end\": 15.000000") != std::string::npos);
}

TEST_CASE("ColumnRecipePresetRegistry rejects duplicate preset ids", "[TensorDesign][presets]") {
    auto registry = createBuiltInColumnRecipePresetRegistry();

    auto const * existing = registry.find("mean_value");
    REQUIRE(existing != nullptr);

    CHECK_FALSE(registry.registerPreset(*existing));
}

TEST_CASE("ColumnAggregatorRegistry getAggregatorsFor filters by EffectiveRowType",
          "[TensorDesign][presets][Phase9f]") {
    auto registry = createBuiltInColumnAggregatorRegistry();

    SECTION("Filters for Interval") {
        auto aggregators = registry.getAggregatorsFor(Neuralyzer::TensorDesign::EffectiveRowType::Interval);
        REQUIRE_FALSE(aggregators.empty());
        // Verify that mean_value is included
        auto it_mean = std::ranges::find_if(aggregators, [](auto const * desc) { return desc->id == "mean_value"; });
        CHECK(it_mean != aggregators.end());
        // Verify that point_xy is NOT included
        auto it_point = std::ranges::find_if(aggregators, [](auto const * desc) { return desc->id == "point_xy"; });
        CHECK(it_point == aggregators.end());
    }

    SECTION("Filters for Timestamp") {
        auto aggregators = registry.getAggregatorsFor(Neuralyzer::TensorDesign::EffectiveRowType::Timestamp);
        REQUIRE_FALSE(aggregators.empty());
        // Verify that point_xy is included
        auto it_point = std::ranges::find_if(aggregators, [](auto const * desc) { return desc->id == "point_xy"; });
        CHECK(it_point != aggregators.end());
        // Verify that event_count is NOT included
        auto it_count = std::ranges::find_if(aggregators, [](auto const * desc) { return desc->id == "event_count"; });
        CHECK(it_count == aggregators.end());
        // Verify that raster_events_relative is NOT included (interval-only gather fragment)
        auto it_raster = std::ranges::find_if(aggregators, [](auto const * desc) {
            return desc->id == "raster_events_relative";
        });
        CHECK(it_raster == aggregators.end());
    }
}

TEST_CASE("ColumnAggregatorRegistry composition rules filter tensor columns",
          "[TensorDesign][presets][Phase9f]") {
    auto modifier_registry = Neuralyzer::TensorDesign::createBuiltInRowModifierRegistry();
    auto aggregator_registry = createBuiltInColumnAggregatorRegistry();

    auto const * bind_modifier = modifier_registry.find("bind_interval_start");
    auto const * window_modifier = modifier_registry.find("window_around_interval_start");
    auto const * interval_start_modifier = modifier_registry.find("interval_start");
    REQUIRE(bind_modifier != nullptr);
    REQUIRE(window_modifier != nullptr);
    REQUIRE(interval_start_modifier != nullptr);

    auto const * raster = aggregator_registry.find("raster_events_relative");
    auto const * trial_count = aggregator_registry.find("trial_relative_event_count");
    REQUIRE(raster != nullptr);
    REQUIRE(trial_count != nullptr);

    SECTION("tensor_column_only excludes raster_events_relative") {
        Neuralyzer::TensorDesign::AggregatorQueryContext const ctx{
                .effective_row_type = Neuralyzer::TensorDesign::EffectiveRowType::Interval,
                .selected_modifier = bind_modifier,
                .tensor_column_only = true};
        auto aggregators = aggregator_registry.getAggregatorsFor(ctx);
        auto it_raster = std::ranges::find_if(aggregators, [](auto const * desc) {
            return desc->id == "raster_events_relative";
        });
        CHECK(it_raster == aggregators.end());
        auto it_trial = std::ranges::find_if(aggregators, [](auto const * desc) {
            return desc->id == "trial_relative_event_count";
        });
        CHECK(it_trial != aggregators.end());
    }

    SECTION("isCompatibleComposition enforces modifier pairing") {
        CHECK(aggregator_registry.isCompatibleComposition(bind_modifier, *trial_count));
        CHECK_FALSE(aggregator_registry.isCompatibleComposition(window_modifier, *raster));
        CHECK_FALSE(aggregator_registry.isCompatibleComposition(interval_start_modifier, *raster));
        CHECK_FALSE(aggregator_registry.isCompatibleComposition(nullptr, *trial_count));
    }

    SECTION("raster_events_relative metadata") {
        CHECK(raster->composition_rules.output_kind ==
              Neuralyzer::TensorDesign::ColumnOutputKind::GatheredDataObject);
        CHECK(raster->composition_rules.gathered_output_type ==
              Neuralyzer::TensorDesign::GatheredOutputType::DigitalEventSeries);
        CHECK(raster->composition_rules.requires_pipeline_value_bindings);
        CHECK(raster->supported_row_types ==
              std::vector<Neuralyzer::TensorDesign::EffectiveRowType>{
                      Neuralyzer::TensorDesign::EffectiveRowType::Interval});
    }
}
