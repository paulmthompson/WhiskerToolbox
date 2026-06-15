/**
 * @file EncoderDispatch.test.cpp
 * @brief Tests for type-safe encoder dispatch helpers.
 */

#include <catch2/catch_test_macros.hpp>

#include "channel_encoding/EncoderDispatch.hpp"

#include <ATen/Functions.h>

TEST_CASE("EncoderDispatch maps params to factory names",
          "[channel_encoding][encoder_dispatch]") {
    CHECK(dl::encoderFactoryName<dl::ImageEncoderParams>() == "ImageEncoder");
    CHECK(dl::encoderFactoryName<dl::Point2DEncoderParams>() == "Point2DEncoder");
    CHECK(dl::encoderFactoryName<dl::Mask2DEncoderParams>() == "Mask2DEncoder");
    CHECK(dl::encoderFactoryName<dl::Line2DEncoderParams>() == "Line2DEncoder");
}

TEST_CASE("EncoderDispatch maps params to DataManager types",
          "[channel_encoding][encoder_dispatch]") {
    CHECK(dl::dataTypeForEncoderParams<dl::ImageEncoderParams>() == "MediaData");
    CHECK(dl::dataTypeForEncoderParams<dl::Point2DEncoderParams>() == "PointData");
    CHECK(dl::dataTypeForEncoderParams<dl::Mask2DEncoderParams>() == "MaskData");
    CHECK(dl::dataTypeForEncoderParams<dl::Line2DEncoderParams>() == "LineData");
}

TEST_CASE("EncoderDispatch factory name round-trip",
          "[channel_encoding][encoder_dispatch]") {
    auto const image_params = dl::encoderParamsFromFactoryName("ImageEncoder");
    REQUIRE(image_params);
    CHECK(dl::encoderFactoryName(*image_params) == "ImageEncoder");
    CHECK(dl::dataTypeForEncoder(*image_params) == "MediaData");

    CHECK(!dl::encoderParamsFromFactoryName("UnknownEncoder"));
}

TEST_CASE("EncoderDispatch identifies image encoders",
          "[channel_encoding][encoder_dispatch]") {
    CHECK(dl::isImageEncoderParams<dl::ImageEncoderParams>());
    CHECK_FALSE(dl::isImageEncoderParams<dl::Point2DEncoderParams>());
    CHECK_FALSE(dl::isImageEncoderParams<dl::Mask2DEncoderParams>());
    CHECK_FALSE(dl::isImageEncoderParams<dl::Line2DEncoderParams>());

    CHECK(dl::isImageEncoder(dl::EncoderVariant{dl::ImageEncoderParams{}}));
    CHECK_FALSE(dl::isImageEncoder(
            dl::EncoderVariant{dl::Point2DEncoderParams{}}));
}

TEST_CASE("EncoderDispatch encodes point tensor via variant",
          "[channel_encoding][encoder_dispatch]") {
    auto tensor = at::zeros({1, 1, 8, 8});
    dl::EncoderContext ctx;
    ctx.batch_index = 0;
    ctx.height = 8;
    ctx.width = 8;

    std::vector<Point2D<float>> points{{4.0f, 4.0f}};
    dl::Point2DEncoderParams params{.mode = dl::RasterMode::Binary};

    dl::encodeToTensor(points, tensor, ctx, ImageSize{8, 8}, params);
    CHECK(tensor[0][0][4][4].item<float>() == 1.0f);
}
