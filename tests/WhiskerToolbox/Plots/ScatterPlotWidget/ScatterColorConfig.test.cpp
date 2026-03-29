/**
 * @file ScatterColorConfig.test.cpp
 * @brief Tests for ScatterColorConfigData serialization and ScatterPlotState color config
 *
 * Verifies JSON round-trip of ScatterColorConfigData both standalone (via rfl::json)
 * and embedded within ScatterPlotState (via toJson/fromJson).
 *
 * @see ScatterColorConfig.hpp
 * @see ScatterPlotState
 */

#include "Core/ScatterColorConfig.hpp"
#include "Core/ScatterPlotState.hpp"

#include "CorePlotting/FeatureColor/FeatureColorSourceDescriptor.hpp"

#include <rfl/json.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

// =============================================================================
// ScatterColorConfigData standalone round-trip
// =============================================================================

TEST_CASE("ScatterColorConfigData default round-trip", "[ScatterColorConfig][Serialization]") {
    ScatterColorConfigData const original;

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<ScatterColorConfigData>(json);
    REQUIRE(result);

    auto const & restored = *result;
    REQUIRE(restored.color_mode == "fixed");
    REQUIRE_FALSE(restored.color_source.has_value());
    REQUIRE(restored.mapping_mode == "continuous");
    REQUIRE(restored.colormap_preset == "Viridis");
    REQUIRE(restored.color_range_mode == "auto");
    REQUIRE(restored.color_range_vmin == Catch::Approx(0.0f));
    REQUIRE(restored.color_range_vmax == Catch::Approx(1.0f));
    REQUIRE(restored.threshold == Catch::Approx(0.5));
    REQUIRE(restored.above_color == "#FF4444");
    REQUIRE(restored.below_color == "#3388FF");
    REQUIRE(restored.above_alpha == Catch::Approx(0.9f));
    REQUIRE(restored.below_alpha == Catch::Approx(0.9f));
}

TEST_CASE("ScatterColorConfigData continuous mode round-trip",
          "[ScatterColorConfig][Serialization]") {
    ScatterColorConfigData original;
    original.color_mode = "by_feature";
    original.color_source = CorePlotting::FeatureColor::FeatureColorSourceDescriptor{
            .data_key = "velocity",
            .tensor_column_name = std::nullopt,
            .tensor_column_index = std::nullopt};
    original.mapping_mode = "continuous";
    original.colormap_preset = "Inferno";
    original.color_range_mode = "manual";
    original.color_range_vmin = -3.5f;
    original.color_range_vmax = 12.0f;

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<ScatterColorConfigData>(json);
    REQUIRE(result);

    auto const & r = *result;
    REQUIRE(r.color_mode == "by_feature");
    REQUIRE(r.color_source.has_value());
    REQUIRE(r.color_source->data_key == "velocity");
    REQUIRE_FALSE(r.color_source->tensor_column_name.has_value());
    REQUIRE_FALSE(r.color_source->tensor_column_index.has_value());
    REQUIRE(r.mapping_mode == "continuous");
    REQUIRE(r.colormap_preset == "Inferno");
    REQUIRE(r.color_range_mode == "manual");
    REQUIRE(r.color_range_vmin == Catch::Approx(-3.5f));
    REQUIRE(r.color_range_vmax == Catch::Approx(12.0f));
}

TEST_CASE("ScatterColorConfigData threshold mode round-trip",
          "[ScatterColorConfig][Serialization]") {
    ScatterColorConfigData original;
    original.color_mode = "by_feature";
    original.color_source = CorePlotting::FeatureColor::FeatureColorSourceDescriptor{
            .data_key = "reward_intervals",
            .tensor_column_name = std::nullopt,
            .tensor_column_index = std::nullopt};
    original.mapping_mode = "threshold";
    original.threshold = 0.5;
    original.above_color = "#00FF00";
    original.below_color = "#FF0000";
    original.above_alpha = 0.7f;
    original.below_alpha = 0.4f;

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<ScatterColorConfigData>(json);
    REQUIRE(result);

    auto const & r = *result;
    REQUIRE(r.color_mode == "by_feature");
    REQUIRE(r.color_source.has_value());
    REQUIRE(r.color_source->data_key == "reward_intervals");
    REQUIRE(r.mapping_mode == "threshold");
    REQUIRE(r.threshold == Catch::Approx(0.5));
    REQUIRE(r.above_color == "#00FF00");
    REQUIRE(r.below_color == "#FF0000");
    REQUIRE(r.above_alpha == Catch::Approx(0.7f));
    REQUIRE(r.below_alpha == Catch::Approx(0.4f));
}

TEST_CASE("ScatterColorConfigData TensorData column name round-trip",
          "[ScatterColorConfig][Serialization]") {
    ScatterColorConfigData original;
    original.color_mode = "by_feature";
    original.color_source = CorePlotting::FeatureColor::FeatureColorSourceDescriptor{
            .data_key = "embeddings",
            .tensor_column_name = "pc_0",
            .tensor_column_index = 0};
    original.mapping_mode = "continuous";
    original.colormap_preset = "Plasma";
    original.color_range_mode = "symmetric";

    auto const json = rfl::json::write(original);
    auto const result = rfl::json::read<ScatterColorConfigData>(json);
    REQUIRE(result);

    auto const & r = *result;
    REQUIRE(r.color_source.has_value());
    REQUIRE(r.color_source->data_key == "embeddings");
    REQUIRE(r.color_source->tensor_column_name.has_value());
    REQUIRE(*r.color_source->tensor_column_name == "pc_0");
    REQUIRE(r.color_source->tensor_column_index.has_value());
    REQUIRE(*r.color_source->tensor_column_index == 0);
    REQUIRE(r.color_range_mode == "symmetric");
}

// =============================================================================
// ScatterPlotState round-trip with color config
// =============================================================================

TEST_CASE("ScatterPlotState color config survives toJson/fromJson",
          "[ScatterPlotState][Serialization][FeatureColor]") {
    auto state1 = std::make_shared<ScatterPlotState>();

    ScatterColorConfigData cc;
    cc.color_mode = "by_feature";
    cc.color_source = CorePlotting::FeatureColor::FeatureColorSourceDescriptor{
            .data_key = "acceleration",
            .tensor_column_name = std::nullopt,
            .tensor_column_index = std::nullopt};
    cc.mapping_mode = "threshold";
    cc.threshold = 2.5;
    cc.above_color = "#AABBCC";
    cc.below_color = "#112233";
    cc.above_alpha = 0.6f;
    cc.below_alpha = 0.3f;
    state1->setColorConfig(cc);

    auto const json = state1->toJson();

    auto state2 = std::make_shared<ScatterPlotState>();
    REQUIRE(state2->fromJson(json));

    auto const & restored = state2->colorConfig();
    REQUIRE(restored.color_mode == "by_feature");
    REQUIRE(restored.color_source.has_value());
    REQUIRE(restored.color_source->data_key == "acceleration");
    REQUIRE(restored.mapping_mode == "threshold");
    REQUIRE(restored.threshold == Catch::Approx(2.5));
    REQUIRE(restored.above_color == "#AABBCC");
    REQUIRE(restored.below_color == "#112233");
    REQUIRE(restored.above_alpha == Catch::Approx(0.6f));
    REQUIRE(restored.below_alpha == Catch::Approx(0.3f));
}

TEST_CASE("ScatterPlotState default color config round-trips as fixed",
          "[ScatterPlotState][Serialization][FeatureColor]") {
    auto state1 = std::make_shared<ScatterPlotState>();
    auto const json = state1->toJson();

    auto state2 = std::make_shared<ScatterPlotState>();
    REQUIRE(state2->fromJson(json));

    auto const & restored = state2->colorConfig();
    REQUIRE(restored.color_mode == "fixed");
    REQUIRE_FALSE(restored.color_source.has_value());
    REQUIRE(restored.mapping_mode == "continuous");
}
