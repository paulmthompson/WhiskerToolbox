#include <catch2/catch_test_macros.hpp>

#include "channel_encoding/EncoderFactory.hpp"
#include "channel_encoding/ImageEncoder.hpp"
#include "channel_encoding/Line2DEncoder.hpp"
#include "channel_encoding/Mask2DEncoder.hpp"
#include "channel_encoding/Point2DEncoder.hpp"

#include <algorithm>

TEST_CASE("EncoderFactory - create known encoders", "[channel_encoding][EncoderFactory]")
{
    auto image_enc = dl::EncoderFactory::create("ImageEncoder");
    REQUIRE(image_enc != nullptr);
    CHECK(image_enc->name() == "ImageEncoder");

    auto point_enc = dl::EncoderFactory::create("Point2DEncoder");
    REQUIRE(point_enc != nullptr);
    CHECK(point_enc->name() == "Point2DEncoder");

    auto mask_enc = dl::EncoderFactory::create("Mask2DEncoder");
    REQUIRE(mask_enc != nullptr);
    CHECK(mask_enc->name() == "Mask2DEncoder");

    auto line_enc = dl::EncoderFactory::create("Line2DEncoder");
    REQUIRE(line_enc != nullptr);
    CHECK(line_enc->name() == "Line2DEncoder");
}

TEST_CASE("EncoderFactory - unknown name returns nullptr", "[channel_encoding][EncoderFactory]")
{
    auto enc = dl::EncoderFactory::create("NonExistentEncoder");
    CHECK(enc == nullptr);
}

TEST_CASE("EncoderFactory - availableEncoders lists all", "[channel_encoding][EncoderFactory]")
{
    auto const names = dl::EncoderFactory::availableEncoders();

    CHECK(names.size() == 4);
    CHECK(std::find(names.begin(), names.end(), "ImageEncoder") != names.end());
    CHECK(std::find(names.begin(), names.end(), "Point2DEncoder") != names.end());
    CHECK(std::find(names.begin(), names.end(), "Mask2DEncoder") != names.end());
    CHECK(std::find(names.begin(), names.end(), "Line2DEncoder") != names.end());
}

TEST_CASE("EncoderFactory - each encoder reports correct input type", "[channel_encoding][EncoderFactory]")
{
    auto image_enc = dl::EncoderFactory::create("ImageEncoder");
    CHECK(image_enc->inputTypeName() == "Image");

    auto point_enc = dl::EncoderFactory::create("Point2DEncoder");
    CHECK(point_enc->inputTypeName() == "Point2D<float>");

    auto mask_enc = dl::EncoderFactory::create("Mask2DEncoder");
    CHECK(mask_enc->inputTypeName() == "Mask2D");

    auto line_enc = dl::EncoderFactory::create("Line2DEncoder");
    CHECK(line_enc->inputTypeName() == "Line2D");
}
