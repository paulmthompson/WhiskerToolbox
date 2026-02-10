#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_encoding/Line2DEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("Line2DEncoder - name and input type", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;
    CHECK(encoder.name() == "Line2DEncoder");
    CHECK(encoder.inputTypeName() == "Line2D");
}

TEST_CASE("Line2DEncoder - horizontal line binary", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Horizontal line at y=5 from x=0 to x=9
    Line2D line({
        Point2D<float>{0.0f, 5.0f},
        Point2D<float>{9.0f, 5.0f}
    });

    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // All pixels along row 5 should be 1.0
    for (int x = 0; x < 10; ++x) {
        CHECK_THAT(accessor[0][0][5][x], WithinAbs(1.0f, 1e-5f));
    }

    // Pixels on other rows should be 0.0
    CHECK_THAT(accessor[0][0][0][5], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][9][5], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Line2DEncoder - vertical line binary", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Vertical line at x=3 from y=0 to y=9
    Line2D line({
        Point2D<float>{3.0f, 0.0f},
        Point2D<float>{3.0f, 9.0f}
    });

    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // All pixels along column 3 should be 1.0
    for (int y = 0; y < 10; ++y) {
        CHECK_THAT(accessor[0][0][y][3], WithinAbs(1.0f, 1e-5f));
    }
}

TEST_CASE("Line2DEncoder - empty and single point lines", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Empty line
    Line2D empty_line;
    encoder.encode(empty_line, src_size, tensor, params);
    CHECK_THAT(tensor.sum().item<float>(), WithinAbs(0.0f, 1e-5f));

    // Single point line (not enough for a segment)
    Line2D single_point({Point2D<float>{5.0f, 5.0f}});
    encoder.encode(single_point, src_size, tensor, params);
    CHECK_THAT(tensor.sum().item<float>(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Line2DEncoder - multi-segment polyline binary", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // L-shaped line: horizontal then vertical
    Line2D line({
        Point2D<float>{0.0f, 0.0f},
        Point2D<float>{5.0f, 0.0f},
        Point2D<float>{5.0f, 5.0f}
    });

    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // Check corners of the L
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(1.0f, 1e-5f)); // start
    CHECK_THAT(accessor[0][0][0][5], WithinAbs(1.0f, 1e-5f)); // corner
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f)); // end

    // Total number of set pixels should be reasonable for an L shape
    auto const total = tensor.sum().item<float>();
    CHECK(total >= 10.0f); // at least 10 pixels for an L spanning 5+5
}

TEST_CASE("Line2DEncoder - heatmap mode", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{20, 20};
    auto tensor = torch::zeros({1, 1, 20, 20});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 20;
    params.width = 20;
    params.mode = dl::RasterMode::Heatmap;
    params.gaussian_sigma = 2.0f;

    // Horizontal line at y=10
    Line2D line({
        Point2D<float>{0.0f, 10.0f},
        Point2D<float>{19.0f, 10.0f}
    });

    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // Points on the line should have value 1.0
    CHECK_THAT(accessor[0][0][10][10], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][10][0], WithinAbs(1.0f, 1e-5f));

    // Points near the line should have positive values
    CHECK(accessor[0][0][11][10] > 0.5f);
    CHECK(accessor[0][0][12][10] > 0.0f);

    // Values should decrease with distance
    CHECK(accessor[0][0][10][10] > accessor[0][0][12][10]);
    CHECK(accessor[0][0][12][10] > accessor[0][0][15][10]);

    // All values should be <= 1.0
    auto const max_val = tensor.max().item<float>();
    CHECK(max_val <= 1.0f + 1e-5f);
}

TEST_CASE("Line2DEncoder - scaling from larger source", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    // Source 100x100, tensor 10x10
    ImageSize const src_size{100, 100};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Horizontal line at y=50 from x=0 to x=99 in source
    // Should map to y=5 across tensor
    Line2D line({
        Point2D<float>{0.0f, 50.0f},
        Point2D<float>{99.0f, 50.0f}
    });

    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Row 5 should have the line
    for (int x = 0; x < 10; ++x) {
        CHECK_THAT(accessor[0][0][5][x], WithinAbs(1.0f, 1e-5f));
    }
}

TEST_CASE("Line2DEncoder - invalid mode throws", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Raw;

    Line2D line({Point2D<float>{0.0f, 0.0f}, Point2D<float>{9.0f, 9.0f}});
    CHECK_THROWS_AS(encoder.encode(line, src_size, tensor, params), std::invalid_argument);
}

TEST_CASE("Line2DEncoder - batch index", "[channel_encoding][Line2DEncoder]")
{
    dl::Line2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({2, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 1;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    Line2D line({Point2D<float>{0.0f, 5.0f}, Point2D<float>{9.0f, 5.0f}});
    encoder.encode(line, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Batch 0 should remain all zeros
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(0.0f, 1e-5f));
    // Batch 1 should have the line
    CHECK_THAT(accessor[1][0][5][5], WithinAbs(1.0f, 1e-5f));
}
