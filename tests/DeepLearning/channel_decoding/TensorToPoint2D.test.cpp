#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_decoding/TensorToPoint2D.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("TensorToPoint2D - name and output type", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;
    CHECK(decoder.name() == "TensorToPoint2D");
    CHECK(decoder.outputTypeName() == "Point2D<float>");
}

TEST_CASE("TensorToPoint2D - single peak argmax", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});
    tensor[0][0][3][7] = 1.0f; // peak at (x=7, y=3)

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = false;

    auto const pt = decoder.decode(tensor, params);
    CHECK_THAT(pt.x, WithinAbs(7.0f, 1e-5f));
    CHECK_THAT(pt.y, WithinAbs(3.0f, 1e-5f));
}

TEST_CASE("TensorToPoint2D - subpixel refinement", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    // Create a Gaussian-like peak centered at (5.3, 4.7)
    auto tensor = torch::zeros({1, 1, 10, 10});
    float const cx = 5.3f;
    float const cy = 4.7f;
    float const sigma = 1.5f;

    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            float const dx = static_cast<float>(x) - cx;
            float const dy = static_cast<float>(y) - cy;
            tensor[0][0][y][x] = std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
        }
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = true;

    auto const pt = decoder.decode(tensor, params);
    // Subpixel should get closer to the true center than integer argmax
    CHECK_THAT(pt.x, WithinAbs(cx, 0.7f));
    CHECK_THAT(pt.y, WithinAbs(cy, 0.7f));
}

TEST_CASE("TensorToPoint2D - all zeros returns origin", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = false;

    auto const pt = decoder.decode(tensor, params);
    CHECK_THAT(pt.x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(pt.y, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("TensorToPoint2D - scaling to target image size", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    // Peak at (5, 5) in 10x10 tensor → should map to (50, 50) in 100x100 target
    auto tensor = torch::zeros({1, 1, 10, 10});
    tensor[0][0][5][5] = 1.0f;

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = false;
    params.target_image_size = ImageSize{100, 100};

    auto const pt = decoder.decode(tensor, params);
    CHECK_THAT(pt.x, WithinAbs(50.0f, 1e-3f));
    CHECK_THAT(pt.y, WithinAbs(50.0f, 1e-3f));
}

TEST_CASE("TensorToPoint2D - batch index", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({2, 1, 10, 10});
    tensor[0][0][2][3] = 1.0f; // batch 0: peak at (3, 2)
    tensor[1][0][7][8] = 1.0f; // batch 1: peak at (8, 7)

    dl::DecoderParams params;
    params.source_channel = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = false;

    params.batch_index = 0;
    auto const pt0 = decoder.decode(tensor, params);
    CHECK_THAT(pt0.x, WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(pt0.y, WithinAbs(2.0f, 1e-5f));

    params.batch_index = 1;
    auto const pt1 = decoder.decode(tensor, params);
    CHECK_THAT(pt1.x, WithinAbs(8.0f, 1e-5f));
    CHECK_THAT(pt1.y, WithinAbs(7.0f, 1e-5f));
}

TEST_CASE("TensorToPoint2D - channel selection", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({1, 2, 10, 10});
    tensor[0][0][1][2] = 1.0f; // channel 0: peak at (2, 1)
    tensor[0][1][8][9] = 1.0f; // channel 1: peak at (9, 8)

    dl::DecoderParams params;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.subpixel = false;

    params.source_channel = 0;
    auto const pt0 = decoder.decode(tensor, params);
    CHECK_THAT(pt0.x, WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(pt0.y, WithinAbs(1.0f, 1e-5f));

    params.source_channel = 1;
    auto const pt1 = decoder.decode(tensor, params);
    CHECK_THAT(pt1.x, WithinAbs(9.0f, 1e-5f));
    CHECK_THAT(pt1.y, WithinAbs(8.0f, 1e-5f));
}

TEST_CASE("TensorToPoint2D - decodeMultiple finds multiple peaks", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({1, 1, 20, 20});
    // Place two well-separated Gaussians
    float const sigma = 1.5f;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            float const dx1 = static_cast<float>(x) - 5.0f;
            float const dy1 = static_cast<float>(y) - 5.0f;
            float const dx2 = static_cast<float>(x) - 15.0f;
            float const dy2 = static_cast<float>(y) - 15.0f;
            float const v1 = std::exp(-(dx1 * dx1 + dy1 * dy1) / (2.0f * sigma * sigma));
            float const v2 = std::exp(-(dx2 * dx2 + dy2 * dy2) / (2.0f * sigma * sigma));
            tensor[0][0][y][x] = std::max(v1, v2);
        }
    }

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 20;
    params.width = 20;
    params.threshold = 0.5f;
    params.subpixel = false;

    auto const points = decoder.decodeMultiple(tensor, params);
    REQUIRE(points.size() == 2);

    // Sort by x to ensure stable ordering
    auto sorted = points;
    std::sort(sorted.begin(), sorted.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });

    CHECK_THAT(sorted[0].x, WithinAbs(5.0f, 1.0f));
    CHECK_THAT(sorted[0].y, WithinAbs(5.0f, 1.0f));
    CHECK_THAT(sorted[1].x, WithinAbs(15.0f, 1.0f));
    CHECK_THAT(sorted[1].y, WithinAbs(15.0f, 1.0f));
}

TEST_CASE("TensorToPoint2D - decodeMultiple empty for zeros", "[channel_decoding][TensorToPoint2D]")
{
    dl::TensorToPoint2D decoder;

    auto tensor = torch::zeros({1, 1, 10, 10});

    dl::DecoderParams params;
    params.source_channel = 0;
    params.batch_index = 0;
    params.height = 10;
    params.width = 10;
    params.threshold = 0.5f;

    auto const points = decoder.decodeMultiple(tensor, params);
    CHECK(points.empty());
}
