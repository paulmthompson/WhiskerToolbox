#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_decoding/TensorToMask2D.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("TensorToMask2D - name and output type", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;
    CHECK(decoder.name() == "TensorToMask2D");
    CHECK(decoder.outputTypeName() == "Mask2D");
}

TEST_CASE("TensorToMask2D - basic thresholding", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});
    // Set a few pixels above threshold
    tensor[0][0][2][3] = 0.8f;
    tensor[0][0][5][5] = 0.9f;
    tensor[0][0][7][1] = 0.6f;
    // Below threshold
    tensor[0][0][0][0] = 0.3f;

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const mask = decoder.decode(tensor, params);

    // Should have exactly 3 pixels above threshold
    CHECK(mask.size() == 3);

    // Verify the pixels are present (order: row-major scan)
    bool found_2_3 = false;
    bool found_5_5 = false;
    bool found_7_1 = false;

    for (size_t i = 0; i < mask.size(); ++i) {
        auto const & p = mask[i];
        if (p.x == 3 && p.y == 2) found_2_3 = true;
        if (p.x == 5 && p.y == 5) found_5_5 = true;
        if (p.x == 1 && p.y == 7) found_7_1 = true;
    }

    CHECK(found_2_3);
    CHECK(found_5_5);
    CHECK(found_7_1);
}

TEST_CASE("TensorToMask2D - empty mask for zeros", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const mask = decoder.decode(tensor, params);
    CHECK(mask.empty());
}

TEST_CASE("TensorToMask2D - all above threshold", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::ones({1, 1, 5, 5}); // all 1.0

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 5;
    params.width = 5;
    params.threshold = 0.5f;

    auto const mask = decoder.decode(tensor, params);
    CHECK(mask.size() == 25);
}

TEST_CASE("TensorToMask2D - scaling to target image size", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    // Single pixel at (5, 5) in 10x10 tensor → (50, 50) in 100x100 target
    auto tensor = torch::zeros({1, 1, 10, 10});
    tensor[0][0][5][5] = 1.0f;

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;
    params.target_image_size = ImageSize{100, 100};

    auto const mask = decoder.decode(tensor, params);
    REQUIRE(mask.size() == 1);
    CHECK(mask[0].x == 50);
    CHECK(mask[0].y == 50);
}

TEST_CASE("TensorToMask2D - batch index", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::zeros({2, 1, 10, 10});
    tensor[0][0][2][3] = 1.0f; // batch 0: one pixel
    tensor[1][0][7][8] = 1.0f; // batch 1: different pixel

    dl::DecoderParams params;
    params.source_channel = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    params.batch_index = 0;
    auto const mask0 = decoder.decode(tensor, params);
    REQUIRE(mask0.size() == 1);
    CHECK(mask0[0].x == 3);
    CHECK(mask0[0].y == 2);

    params.batch_index = 1;
    auto const mask1 = decoder.decode(tensor, params);
    REQUIRE(mask1.size() == 1);
    CHECK(mask1[0].x == 8);
    CHECK(mask1[0].y == 7);
}

TEST_CASE("TensorToMask2D - threshold boundary", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::zeros({1, 1, 5, 5});
    tensor[0][0][0][0] = 0.5f;  // exactly at threshold (should NOT be included)
    tensor[0][0][1][1] = 0.51f; // above threshold (should be included)
    tensor[0][0][2][2] = 0.49f; // below threshold (should NOT be included)

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 5;
    params.width = 5;
    params.threshold = 0.5f;

    auto const mask = decoder.decode(tensor, params);
    REQUIRE(mask.size() == 1);
    CHECK(mask[0].x == 1);
    CHECK(mask[0].y == 1);
}

TEST_CASE("TensorToMask2D - channel selection", "[channel_decoding][TensorToMask2D]")
{
    dl::TensorToMask2D decoder;

    auto tensor = torch::zeros({1, 2, 10, 10});
    tensor[0][0][3][4] = 1.0f; // channel 0
    tensor[0][1][6][7] = 1.0f; // channel 1

    dl::DecoderParams params;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    params.source_channel = 0;
    auto const mask0 = decoder.decode(tensor, params);
    REQUIRE(mask0.size() == 1);
    CHECK(mask0[0].x == 4);
    CHECK(mask0[0].y == 3);

    params.source_channel = 1;
    auto const mask1 = decoder.decode(tensor, params);
    REQUIRE(mask1.size() == 1);
    CHECK(mask1[0].x == 7);
    CHECK(mask1[0].y == 6);
}
