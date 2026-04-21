#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager/DataManager.hpp"
#include "OpenGLWidget.hpp"
#include "TransformComposers.hpp"

#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("DataViewer composeAnalogYTransform ignores intrinsic scale", "[DataViewer][Scaling]") {
    CorePlotting::SeriesLayout const layout{
            "analog_0",
            CorePlotting::LayoutTransform{-0.5f, 0.5f},
            0};

    auto const with_intrinsic_1 = DataViewer::composeAnalogYTransform(
            layout,
            0.0f,
            2.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.8f);

    auto const with_intrinsic_small = DataViewer::composeAnalogYTransform(
            layout,
            0.0f,
            2.0f,
            0.001f,
            1.0f,
            0.0f,
            1.0f,
            0.8f);

    REQUIRE_THAT(with_intrinsic_1.gain, WithinAbs(with_intrinsic_small.gain, 1e-6f));
    REQUIRE_THAT(with_intrinsic_1.offset, WithinAbs(with_intrinsic_small.offset, 1e-6f));
}

TEST_CASE("DataViewer composeAnalogYTransform maps ±3σ to lane amplitude", "[DataViewer][Scaling]") {
    CorePlotting::SeriesLayout const layout{
            "analog_0",
            CorePlotting::LayoutTransform{0.0f, 0.25f},
            0};

    auto const transform = DataViewer::composeAnalogYTransform(
            layout,
            0.0f,
            4.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.8f);

    float const y_low = transform.apply(-12.0f);// -3σ
    float const y_high = transform.apply(12.0f);// +3σ
    float const expected_span = 2.0f * layout.y_transform.gain * 0.8f;

    REQUIRE_THAT(y_high - y_low, WithinAbs(expected_span, 1e-5f));
}

TEST_CASE("DataViewer computeEventGlyphSize keeps event height stable", "[DataViewer][Scaling]") {
    CorePlotting::SeriesLayout const layout{
            "event_0",
            CorePlotting::LayoutTransform{0.0f, 0.2f},
            0};

    auto const y_transform_g1 = DataViewer::composeEventYTransform(layout, 0.95f, 1.0f);
    auto const y_transform_g10 = DataViewer::composeEventYTransform(layout, 0.95f, 10.0f);

    float const glyph_size_g1 = DataViewer::computeEventGlyphSize(
            y_transform_g1,
            layout.y_transform.gain,
            0.05f,
            0.95f,
            1.0f);

    float const glyph_size_g10 = DataViewer::computeEventGlyphSize(
            y_transform_g10,
            layout.y_transform.gain,
            0.05f,
            0.95f,
            1.0f);

    float const world_height_g1 = glyph_size_g1 * y_transform_g1.gain;
    float const world_height_g10 = glyph_size_g10 * y_transform_g10.gain;
    float const expected_world_height = 0.05f * 0.95f;

    REQUIRE_THAT(world_height_g1, WithinAbs(expected_world_height, 1e-6f));
    REQUIRE_THAT(world_height_g10, WithinAbs(expected_world_height, 1e-6f));
}
