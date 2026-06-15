/**
 * @file DecoderDispatch.test.cpp
 * @brief Tests for type-safe decoder dispatch helpers.
 */

#include <catch2/catch_test_macros.hpp>

#include "channel_decoding/DecoderDispatch.hpp"

#include <ATen/Functions.h>

TEST_CASE("DecoderDispatch maps params to factory names",
          "[channel_decoding][decoder_dispatch]") {
    CHECK(dl::decoderFactoryName<dl::MaskDecoderParams>() == "TensorToMask2D");
    CHECK(dl::decoderFactoryName<dl::PointDecoderParams>() == "TensorToPoint2D");
    CHECK(dl::decoderFactoryName<dl::LineDecoderParams>() == "TensorToLine2D");
    CHECK(dl::decoderFactoryName<dl::FeatureVectorDecoderParams>() ==
          "TensorToFeatureVector");
}

TEST_CASE("DecoderDispatch maps params to DataManager types",
          "[channel_decoding][decoder_dispatch]") {
    CHECK(dl::dataTypeForDecoderParams<dl::MaskDecoderParams>() == "MaskData");
    CHECK(dl::dataTypeForDecoderParams<dl::PointDecoderParams>() == "PointData");
    CHECK(dl::dataTypeForDecoderParams<dl::LineDecoderParams>() == "LineData");
    CHECK(dl::dataTypeForDecoderParams<dl::FeatureVectorDecoderParams>() ==
          "TensorData");
}

TEST_CASE("DecoderDispatch factory name round-trip",
          "[channel_decoding][decoder_dispatch]") {
    auto const mask_params = dl::decoderParamsFromFactoryName("TensorToMask2D");
    REQUIRE(mask_params);
    CHECK(dl::decoderFactoryName(*mask_params) == "TensorToMask2D");
    CHECK(dl::dataTypeForDecoder(*mask_params) == "MaskData");

    CHECK(!dl::decoderParamsFromFactoryName("UnknownDecoder"));
}

TEST_CASE("DecoderDispatch identifies spatial decoders",
          "[channel_decoding][decoder_dispatch]") {
    CHECK(dl::isSpatialDecoderParams<dl::MaskDecoderParams>());
    CHECK(dl::isSpatialDecoderParams<dl::PointDecoderParams>());
    CHECK(dl::isSpatialDecoderParams<dl::LineDecoderParams>());
    CHECK_FALSE(dl::isSpatialDecoderParams<dl::FeatureVectorDecoderParams>());

    CHECK(dl::isSpatialDecoder(dl::DecoderVariant{dl::MaskDecoderParams{}}));
    CHECK_FALSE(dl::isSpatialDecoder(
            dl::DecoderVariant{dl::FeatureVectorDecoderParams{}}));
}

TEST_CASE("DecoderDispatch decodes mask tensor via variant",
          "[channel_decoding][decoder_dispatch]") {
    auto tensor = at::zeros({1, 1, 4, 4});
    tensor[0][0][1][2] = 0.9f;

    dl::DecoderContext ctx;
    ctx.batch_index = 0;
    ctx.height = 4;
    ctx.width = 4;

    dl::MaskDecoderParams params{.threshold = 0.5f};
    auto decoded = dl::decodeToGeometry(tensor, ctx, params);
    REQUIRE(std::holds_alternative<Mask2D>(decoded));
    CHECK(std::get<Mask2D>(decoded).size() == 1);
}
