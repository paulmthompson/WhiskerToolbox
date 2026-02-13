#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_encoding/ImageEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <numeric>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("ImageEncoder - name and input type", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;
    CHECK(encoder.name() == "ImageEncoder");
    CHECK(encoder.inputTypeName() == "Image");
}

TEST_CASE("ImageEncoder - grayscale uint8 basic encoding", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 4;
    int constexpr src_w = 4;
    ImageSize const src_size{src_w, src_h};

    // Create a simple 4x4 grayscale image: all 255 (white)
    std::vector<uint8_t> image(src_h * src_w, 255);

    // Target tensor: [1, 1, 4, 4]
    auto tensor = torch::zeros({1, 1, 4, 4});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 4;
    params.width = 4;
    params.normalize = true;

    encoder.encode(image, src_size, 1, tensor, params);

    // All pixels should be 1.0 after normalization
    auto accessor = tensor.accessor<float, 4>();
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            CHECK_THAT(accessor[0][0][y][x], WithinAbs(1.0f, 1e-5f));
        }
    }
}

TEST_CASE("ImageEncoder - grayscale replication to 3 channels", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 2;
    int constexpr src_w = 2;
    ImageSize const src_size{src_w, src_h};

    // Grayscale image with known values
    std::vector<uint8_t> image = {0, 128, 255, 64};

    // Target tensor: [1, 3, 2, 2] — 3 channels available
    auto tensor = torch::zeros({1, 3, 2, 2});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 2;
    params.width = 2;
    params.normalize = true;

    encoder.encode(image, src_size, 1, tensor, params);

    // All 3 channels should have the same values (replicated)
    auto accessor = tensor.accessor<float, 4>();
    for (int c = 0; c < 3; ++c) {
        CHECK_THAT(accessor[0][c][0][0], WithinAbs(0.0f, 1e-5f));          // 0/255
        CHECK_THAT(accessor[0][c][0][1], WithinAbs(128.0f / 255.0f, 1e-3f)); // 128/255
        CHECK_THAT(accessor[0][c][1][0], WithinAbs(1.0f, 1e-5f));          // 255/255
        CHECK_THAT(accessor[0][c][1][1], WithinAbs(64.0f / 255.0f, 1e-3f));  // 64/255
    }
}

TEST_CASE("ImageEncoder - RGB uint8 encoding", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 2;
    int constexpr src_w = 2;
    ImageSize const src_size{src_w, src_h};

    // RGB image: H*W*3 = 12 bytes, row-major with interleaved channels
    // Pixel (0,0): R=255, G=0, B=0
    // Pixel (0,1): R=0, G=255, B=0
    // Pixel (1,0): R=0, G=0, B=255
    // Pixel (1,1): R=128, G=128, B=128
    std::vector<uint8_t> image = {
        255, 0, 0,    0, 255, 0,
        0, 0, 255,    128, 128, 128
    };

    auto tensor = torch::zeros({1, 3, 2, 2});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 2;
    params.width = 2;
    params.normalize = true;

    encoder.encode(image, src_size, 3, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Channel 0 (R): pixel(0,0)=1.0, pixel(0,1)=0, pixel(1,0)=0, pixel(1,1)≈0.502
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][0][1], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][1][0][0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][1][0][1], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][2][1][0], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("ImageEncoder - resize from larger source", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 8;
    int constexpr src_w = 8;
    ImageSize const src_size{src_w, src_h};

    // Uniform gray image
    std::vector<uint8_t> image(src_h * src_w, 128);

    auto tensor = torch::zeros({1, 1, 4, 4});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 4;
    params.width = 4;
    params.normalize = true;

    encoder.encode(image, src_size, 1, tensor, params);

    // After bilinear resize of uniform image, values should stay ~128/255
    auto accessor = tensor.accessor<float, 4>();
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            CHECK_THAT(accessor[0][0][y][x], WithinAbs(128.0f / 255.0f, 0.01f));
        }
    }
}

TEST_CASE("ImageEncoder - float data encoding", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 2;
    int constexpr src_w = 2;
    ImageSize const src_size{src_w, src_h};

    std::vector<float> image = {0.0f, 0.5f, 1.0f, 0.25f};

    auto tensor = torch::zeros({1, 1, 2, 2});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 0;
    params.height = 2;
    params.width = 2;
    params.normalize = false; // already in [0,1]

    encoder.encode(image, src_size, 1, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][0][1], WithinAbs(0.5f, 1e-5f));
    CHECK_THAT(accessor[0][0][1][0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][1][1], WithinAbs(0.25f, 1e-5f));
}

TEST_CASE("ImageEncoder - batch index writing", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    int constexpr src_h = 2;
    int constexpr src_w = 2;
    ImageSize const src_size{src_w, src_h};

    std::vector<uint8_t> image(src_h * src_w, 255);

    // Batch size = 2
    auto tensor = torch::zeros({2, 1, 2, 2});

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = 1; // write to second batch element
    params.height = 2;
    params.width = 2;
    params.normalize = true;

    encoder.encode(image, src_size, 1, tensor, params);

    auto accessor = tensor.accessor<float, 4>();
    // Batch 0 should be all zeros
    CHECK_THAT(accessor[0][0][0][0], WithinAbs(0.0f, 1e-5f));
    // Batch 1 should be all ones
    CHECK_THAT(accessor[1][0][0][0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[1][0][1][1], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("ImageEncoder - invalid channel count throws", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    std::vector<uint8_t> image(16, 0);
    auto tensor = torch::zeros({1, 1, 4, 4});
    dl::EncoderParams params;
    params.height = 4;
    params.width = 4;

    CHECK_THROWS_AS(encoder.encode(image, {4, 4}, 2, tensor, params), std::invalid_argument);
}

TEST_CASE("ImageEncoder - size mismatch throws", "[channel_encoding][ImageEncoder]")
{
    dl::ImageEncoder encoder;

    // 4x4 image has 16 pixels, but we pass size for 2x2
    std::vector<uint8_t> image(16, 0);
    auto tensor = torch::zeros({1, 1, 4, 4});
    dl::EncoderParams params;
    params.height = 4;
    params.width = 4;

    CHECK_THROWS_AS(encoder.encode(image, {2, 2}, 1, tensor, params), std::invalid_argument);
}
