#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_decoding/TensorToLine2D.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("TensorToLine2D - name and output type", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;
    CHECK(decoder.name() == "TensorToLine2D");
    CHECK(decoder.outputTypeName() == "Line2D");
}

TEST_CASE("TensorToLine2D - horizontal line", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});
    // Draw a horizontal line at y=5, from x=2 to x=7
    for (int x = 2; x <= 7; ++x) {
        tensor[0][0][5][x] = 1.0f;
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);

    // Should have 6 points along the line
    REQUIRE(line.size() >= 4); // skeleton might be slightly shorter
    CHECK(line.size() <= 8);   // shouldn't be much longer

    // All points should be near y=5
    for (size_t i = 0; i < line.size(); ++i) {
        CHECK_THAT(line[i].y, WithinAbs(5.0f, 1.5f));
    }
}

TEST_CASE("TensorToLine2D - vertical line", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});
    // Draw a vertical line at x=4, from y=1 to y=8
    for (int y = 1; y <= 8; ++y) {
        tensor[0][0][y][4] = 1.0f;
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);

    REQUIRE(line.size() >= 4);

    // All points should be near x=4
    for (size_t i = 0; i < line.size(); ++i) {
        CHECK_THAT(line[i].x, WithinAbs(4.0f, 1.5f));
    }
}

TEST_CASE("TensorToLine2D - empty tensor produces empty line", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);
    CHECK(line.empty());
}

TEST_CASE("TensorToLine2D - all below threshold produces empty line", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::full({1, 1, 10, 10}, 0.3f);

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);
    CHECK(line.empty());
}

TEST_CASE("TensorToLine2D - thick line gets thinned", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({1, 1, 20, 20});
    // Draw a thick (3px) horizontal line at y=10
    for (int y = 9; y <= 11; ++y) {
        for (int x = 3; x <= 17; ++x) {
            tensor[0][0][y][x] = 1.0f;
        }
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 20;
    params.width = 20;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);

    // After thinning, the line should be roughly 1 pixel wide
    // and have a reasonable number of points
    REQUIRE(!line.empty());
    // The thinned skeleton of a 3px-wide, 15px-long horizontal band
    // should be roughly 15 pixels long (give or take)
    CHECK(line.size() >= 8);
    CHECK(line.size() <= 20);

    // Points should be approximately at y=10
    for (size_t i = 0; i < line.size(); ++i) {
        CHECK_THAT(line[i].y, WithinAbs(10.0f, 2.0f));
    }
}

TEST_CASE("TensorToLine2D - scaling to target image size", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    // Single-pixel-wide horizontal line at y=5, x=[2..7] in 10x10
    auto tensor = torch::zeros({1, 1, 10, 10});
    for (int x = 2; x <= 7; ++x) {
        tensor[0][0][5][x] = 1.0f;
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;
    params.target_image_size = ImageSize{100, 100};

    auto const line = decoder.decode(tensor, params);
    REQUIRE(!line.empty());

    // All y coords should be around 50.0 (5 * 10)
    for (size_t i = 0; i < line.size(); ++i) {
        CHECK_THAT(line[i].y, WithinAbs(50.0f, 15.0f));
    }

    // x coords should be in the range [20..70] approximately
    for (size_t i = 0; i < line.size(); ++i) {
        CHECK(line[i].x >= 15.0f);
        CHECK(line[i].x <= 75.0f);
    }
}

TEST_CASE("TensorToLine2D - batch index", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({2, 1, 10, 10});
    // Batch 0: horizontal line at y=3
    for (int x = 2; x <= 7; ++x) {
        tensor[0][0][3][x] = 1.0f;
    }
    // Batch 1: horizontal line at y=7
    for (int x = 2; x <= 7; ++x) {
        tensor[1][0][7][x] = 1.0f;
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    params.batch_index = 0;
    auto const line0 = decoder.decode(tensor, params);
    REQUIRE(!line0.empty());
    for (size_t i = 0; i < line0.size(); ++i) {
        CHECK_THAT(line0[i].y, WithinAbs(3.0f, 1.5f));
    }

    params.batch_index = 1;
    auto const line1 = decoder.decode(tensor, params);
    REQUIRE(!line1.empty());
    for (size_t i = 0; i < line1.size(); ++i) {
        CHECK_THAT(line1[i].y, WithinAbs(7.0f, 1.5f));
    }
}

TEST_CASE("TensorToLine2D - single pixel", "[channel_decoding][TensorToLine2D]")
{
    dl::TensorToLine2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});
    tensor[0][0][5][5] = 1.0f;

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const line = decoder.decode(tensor, params);

    // A single pixel produces a line with 1 point
    CHECK(line.size() == 1);
}
