#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_encoding/Point2DEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("Point2DEncoder - name and input type", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;
    CHECK(encoder.name() == "Point2DEncoder");
    CHECK(encoder.inputTypeName() == "Point2D<float>");
}

TEST_CASE("Point2DEncoder - single point binary mode", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    // Source image is 100x100, tensor is 10x10
    ImageSize const src_size{100, 100};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Point at center of source image (50, 50) → should map to (5, 5) in tensor
    Point2D<float> point{50.0f, 50.0f};
    encoder.encode(point, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));

    // Other pixels should remain 0
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][9][9], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Point2DEncoder - multiple points binary mode", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    std::vector<Point2D<float>> points = {
        {0.0f, 0.0f},
        {5.0f, 5.0f},
        {9.0f, 9.0f}
    };

    encoder.encode(points, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][9][9], WithinAbs(1.0f, 1e-5f));

    // Non-point pixels should be 0
    CHECK_THAT(accessor[0][0][3][7], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Point2DEncoder - heatmap mode", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    ImageSize const src_size{20, 20};
    auto tensor = torch::zeros({1, 1, 20, 20});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 20;
    params.width = 20;
    params.mode = dl::RasterMode::Heatmap;
    params.gaussian_sigma = 2.0f;

    // Single point at center
    Point2D<float> point{10.0f, 10.0f};
    encoder.encode(point, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // Center should have maximum value (1.0)
    CHECK_THAT(accessor[0][0][10][10], WithinAbs(1.0f, 1e-5f));

    // Values should decrease away from center
    float const center_val = accessor[0][0][10][10];
    float const neighbor_val = accessor[0][0][10][11];
    float const far_val = accessor[0][0][10][15];

    CHECK(center_val > neighbor_val);
    CHECK(neighbor_val > far_val);

    // Far corners should be ~0
    CHECK(accessor[0][0][0][0] < 0.01f);
}

TEST_CASE("Point2DEncoder - heatmap overlapping points use max", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    ImageSize const src_size{20, 20};
    auto tensor = torch::zeros({1, 1, 20, 20});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 20;
    params.width = 20;
    params.mode = dl::RasterMode::Heatmap;
    params.gaussian_sigma = 2.0f;

    // Two nearby points
    std::vector<Point2D<float>> points = {
        {8.0f, 10.0f},
        {12.0f, 10.0f}
    };
    encoder.encode(points, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // Both point centers should be 1.0
    CHECK_THAT(accessor[0][0][10][8], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][10][12], WithinAbs(1.0f, 1e-5f));

    // Midpoint between them should have some positive value
    CHECK(accessor[0][0][10][10] > 0.0f);

    // All values should be <= 1.0
    auto const max_val = tensor.max().item<float>();
    CHECK(max_val <= 1.0f + 1e-5f);
}

TEST_CASE("Point2DEncoder - scaling from larger source", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    // Source 200x200, tensor 10x10
    ImageSize const src_size{200, 200};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Point at (100, 100) in source → (5, 5) in tensor
    Point2D<float> point{100.0f, 100.0f};
    encoder.encode(point, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Point2DEncoder - invalid mode throws", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Raw;

    Point2D<float> point{5.0f, 5.0f};
    CHECK_THROWS_AS(encoder.encode(point, src_size, tensor, params), std::invalid_argument);
}

TEST_CASE("Point2DEncoder - batch index", "[channel_encoding][Point2DEncoder]")
{
    dl::Point2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({2, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 1;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    Point2D<float> point{5.0f, 5.0f};
    encoder.encode(point, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Batch 0 should remain all zeros
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(0.0f, 1e-5f));
    // Batch 1 should have the point
    CHECK_THAT(accessor[1][0][5][5], WithinAbs(1.0f, 1e-5f));
}
