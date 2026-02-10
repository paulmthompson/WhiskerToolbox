#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_decoding/TensorToLine2D.hpp"
#include "channel_decoding/TensorToMask2D.hpp"
#include "channel_decoding/TensorToPoint2D.hpp"
#include "channel_encoding/Line2DEncoder.hpp"
#include "channel_encoding/Mask2DEncoder.hpp"
#include "channel_encoding/Point2DEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("Round-trip: Point2D encode→decode (binary)", "[channel_decoding][roundtrip]")
{
    // Encode a point, then decode it back
    dl::Point2DEncoder encoder;
    dl::TensorToPoint2D decoder;

    ImageSize const img_size{64, 64};
    Point2D<float> const original{32.0f, 20.0f};

    auto tensor = torch::zeros({1, 1, 64, 64});
    dl::EncoderParams enc_params;
    enc_params.target_channel = 0;
    enc_params.batch_index = 0;
    enc_params.height = 64;
    enc_params.width = 64;
    enc_params.mode = dl::RasterMode::Binary;

    encoder.encode(original, img_size, tensor, enc_params);

    dl::DecoderParams dec_params;
    dec_params.source_channel = 0;
    dec_params.batch_index = 0;
    dec_params.height = 64;
    dec_params.width = 64;
    dec_params.subpixel = false;

    auto const decoded = decoder.decode(tensor, dec_params);

    CHECK_THAT(decoded.x, WithinAbs(original.x, 1.0f));
    CHECK_THAT(decoded.y, WithinAbs(original.y, 1.0f));
}

TEST_CASE("Round-trip: Point2D encode→decode (heatmap, subpixel)", "[channel_decoding][roundtrip]")
{
    dl::Point2DEncoder encoder;
    dl::TensorToPoint2D decoder;

    ImageSize const img_size{64, 64};
    Point2D<float> const original{25.3f, 40.7f};

    auto tensor = torch::zeros({1, 1, 64, 64});
    dl::EncoderParams enc_params;
    enc_params.target_channel = 0;
    enc_params.batch_index = 0;
    enc_params.height = 64;
    enc_params.width = 64;
    enc_params.mode = dl::RasterMode::Heatmap;
    enc_params.gaussian_sigma = 2.0f;

    encoder.encode(original, img_size, tensor, enc_params);

    dl::DecoderParams dec_params;
    dec_params.source_channel = 0;
    dec_params.batch_index = 0;
    dec_params.height = 64;
    dec_params.width = 64;
    dec_params.subpixel = true;

    auto const decoded = decoder.decode(tensor, dec_params);

    // With heatmap + subpixel, should be closer than integer argmax
    CHECK_THAT(decoded.x, WithinAbs(original.x, 0.7f));
    CHECK_THAT(decoded.y, WithinAbs(original.y, 0.7f));
}

TEST_CASE("Round-trip: Mask2D encode→decode", "[channel_decoding][roundtrip]")
{
    dl::Mask2DEncoder encoder;
    dl::TensorToMask2D decoder;

    ImageSize const img_size{10, 10};

    Mask2D original({
        Point2D<uint32_t>{2, 3},
        Point2D<uint32_t>{5, 5},
        Point2D<uint32_t>{8, 1}
    });

    // Encode
    auto tensor = torch::zeros({1, 1, 10, 10});
    dl::EncoderParams enc_params;
    enc_params.target_channel = 0;
    enc_params.batch_index = 0;
    enc_params.height = 10;
    enc_params.width = 10;
    enc_params.mode = dl::RasterMode::Binary;

    encoder.encode(original, img_size, tensor, enc_params);

    // Decode
    dl::DecoderParams dec_params;
    dec_params.source_channel = 0;
    dec_params.batch_index = 0;
    dec_params.height = 10;
    dec_params.width = 10;
    dec_params.threshold = 0.5f;

    auto const decoded = decoder.decode(tensor, dec_params);

    // Should recover the same 3 pixels
    CHECK(decoded.size() == 3);

    // Check all original pixels are present
    auto contains = [&](uint32_t x, uint32_t y) {
        for (size_t i = 0; i < decoded.size(); ++i) {
            if (decoded[i].x == x && decoded[i].y == y) return true;
        }
        return false;
    };

    CHECK(contains(2, 3));
    CHECK(contains(5, 5));
    CHECK(contains(8, 1));
}

TEST_CASE("Round-trip: Line2D encode→decode (binary)", "[channel_decoding][roundtrip]")
{
    dl::Line2DEncoder encoder;
    dl::TensorToLine2D decoder;

    ImageSize const img_size{30, 30};
    Line2D original({
        Point2D<float>{5.0f, 15.0f},
        Point2D<float>{25.0f, 15.0f}
    });

    // Encode
    auto tensor = torch::zeros({1, 1, 30, 30});
    dl::EncoderParams enc_params;
    enc_params.target_channel = 0;
    enc_params.batch_index = 0;
    enc_params.height = 30;
    enc_params.width = 30;
    enc_params.mode = dl::RasterMode::Binary;

    encoder.encode(original, img_size, tensor, enc_params);

    // Decode
    dl::DecoderParams dec_params;
    dec_params.source_channel = 0;
    dec_params.batch_index = 0;
    dec_params.height = 30;
    dec_params.width = 30;
    dec_params.threshold = 0.5f;

    auto const decoded = decoder.decode(tensor, dec_params);

    // The decoded line should have multiple points along y≈15
    REQUIRE(!decoded.empty());
    REQUIRE(decoded.size() >= 10);

    for (size_t i = 0; i < decoded.size(); ++i) {
        CHECK_THAT(decoded[i].y, WithinAbs(15.0f, 2.0f));
    }

    // The line should span approximately x=[5, 25]
    float min_x = decoded[0].x;
    float max_x = decoded[0].x;
    for (size_t i = 1; i < decoded.size(); ++i) {
        min_x = std::min(min_x, decoded[i].x);
        max_x = std::max(max_x, decoded[i].x);
    }
    CHECK(min_x <= 7.0f);
    CHECK(max_x >= 23.0f);
}

TEST_CASE("Round-trip: Point2D with scaling", "[channel_decoding][roundtrip]")
{
    dl::Point2DEncoder encoder;
    dl::TensorToPoint2D decoder;

    // Original in 100x100 space, tensor is 32x32
    ImageSize const img_size{100, 100};
    Point2D<float> const original{60.0f, 40.0f};

    auto tensor = torch::zeros({1, 1, 32, 32});
    dl::EncoderParams enc_params;
    enc_params.target_channel = 0;
    enc_params.batch_index = 0;
    enc_params.height = 32;
    enc_params.width = 32;
    enc_params.mode = dl::RasterMode::Binary;

    encoder.encode(original, img_size, tensor, enc_params);

    // Decode back to 100x100 space
    dl::DecoderParams dec_params;
    dec_params.source_channel = 0;
    dec_params.batch_index = 0;
    dec_params.height = 32;
    dec_params.width = 32;
    dec_params.subpixel = false;
    dec_params.target_image_size = ImageSize{100, 100};

    auto const decoded = decoder.decode(tensor, dec_params);

    // Should be close to original (within quantization error of 32→100)
    CHECK_THAT(decoded.x, WithinAbs(original.x, 4.0f));
    CHECK_THAT(decoded.y, WithinAbs(original.y, 4.0f));
}
