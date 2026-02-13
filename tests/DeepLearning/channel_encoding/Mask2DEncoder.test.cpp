#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_encoding/Mask2DEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("Mask2DEncoder - name and input type", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;
    CHECK(encoder.name() == "Mask2DEncoder");
    CHECK(encoder.inputTypeName() == "Mask2D");
}

TEST_CASE("Mask2DEncoder - basic binary encoding", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    // Source image is 10x10, tensor is 10x10 (no scaling)
    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Create a mask with a few pixels
    Mask2D mask({
        Point2D<uint32_t>{2, 3},
        Point2D<uint32_t>{5, 5},
        Point2D<uint32_t>{8, 1}
    });

    encoder.encode(mask, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();

    // Check mask pixels are 1.0
    CHECK_THAT(accessor[0][0][3][2], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][1][8], WithinAbs(1.0f, 1e-5f));

    // Check non-mask pixels are 0.0
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][9][9], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Mask2DEncoder - empty mask", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    Mask2D mask;
    encoder.encode(mask, src_size, tensor, params);

    // Tensor should remain all zeros
    auto const sum = tensor.sum().item<float>();
    CHECK_THAT(sum, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Mask2DEncoder - scaling from larger source", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    // Source is 100x100, tensor is 10x10
    ImageSize const src_size{100, 100};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Point at (50, 50) in source → (5, 5) in tensor
    Mask2D mask({Point2D<uint32_t>{50, 50}});
    encoder.encode(mask, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Mask2DEncoder - large mask coverage", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    // Fill entire image with mask pixels
    Mask2D mask;
    for (uint32_t y = 0; y < 10; ++y) {
        for (uint32_t x = 0; x < 10; ++x) {
            mask.push_back(Point2D<uint32_t>{x, y});
        }
    }

    encoder.encode(mask, src_size, tensor, params);

    // All pixels should be 1.0
    auto accessor = tensor.accessor<float, 4>();
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            CHECK_THAT(accessor[0][0][y][x], WithinAbs(1.0f, 1e-5f));
        }
    }
}

TEST_CASE("Mask2DEncoder - invalid mode throws", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::EncoderParams params;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Heatmap;

    Mask2D mask({Point2D<uint32_t>{5, 5}});
    CHECK_THROWS_AS(encoder.encode(mask, src_size, tensor, params), std::invalid_argument);
}

TEST_CASE("Mask2DEncoder - batch index", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({2, 1, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 1;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    Mask2D mask({Point2D<uint32_t>{5, 5}});
    encoder.encode(mask, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Batch 0 undisturbed
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(0.0f, 1e-5f));
    // Batch 1 has the mask pixel
    CHECK_THAT(accessor[1][0][5][5], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Mask2DEncoder - target channel selection", "[channel_encoding][Mask2DEncoder]")
{
    dl::Mask2DEncoder encoder;

    ImageSize const src_size{10, 10};
    auto tensor = torch::zeros({1, 3, 10, 10});

    dl::EncoderParams params;
    params.target_channel = 2; // write to channel 2
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.mode = dl::RasterMode::Binary;

    Mask2D mask({Point2D<uint32_t>{5, 5}});
    encoder.encode(mask, src_size, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Channels 0 and 1 should be untouched
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][1][5][5], WithinAbs(0.0f, 1e-5f));
    // Channel 2 should have the mask
    CHECK_THAT(accessor[0][2][5][5], WithinAbs(1.0f, 1e-5f));
}
