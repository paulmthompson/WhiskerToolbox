#include <catch2/catch_test_macros.hpp>

#include "channel_decoding/DecoderFactory.hpp"
#include "channel_decoding/TensorToLine2D.hpp"
#include "channel_decoding/TensorToMask2D.hpp"
#include "channel_decoding/TensorToPoint2D.hpp"

#include <algorithm>

TEST_CASE("DecoderFactory - create known decoders", "[channel_decoding][DecoderFactory]")
{
    auto point_dec = dl::DecoderFactory::create("TensorToPoint2D");
    REQUIRE(point_dec != nullptr);
    CHECK(point_dec->name() == "TensorToPoint2D");

    auto mask_dec = dl::DecoderFactory::create("TensorToMask2D");
    REQUIRE(mask_dec != nullptr);
    CHECK(mask_dec->name() == "TensorToMask2D");

    auto line_dec = dl::DecoderFactory::create("TensorToLine2D");
    REQUIRE(line_dec != nullptr);
    CHECK(line_dec->name() == "TensorToLine2D");
}

TEST_CASE("DecoderFactory - unknown name returns nullptr", "[channel_decoding][DecoderFactory]")
{
    auto dec = dl::DecoderFactory::create("NonExistentDecoder");
    CHECK(dec == nullptr);
}

TEST_CASE("DecoderFactory - availableDecoders lists all", "[channel_decoding][DecoderFactory]")
{
    auto const names = dl::DecoderFactory::availableDecoders();

    CHECK(names.size() == 3);
    CHECK(std::find(names.begin(), names.end(), "TensorToPoint2D") != names.end());
    CHECK(std::find(names.begin(), names.end(), "TensorToMask2D") != names.end());
    CHECK(std::find(names.begin(), names.end(), "TensorToLine2D") != names.end());
}

TEST_CASE("DecoderFactory - each decoder reports correct output type", "[channel_decoding][DecoderFactory]")
{
    auto point_dec = dl::DecoderFactory::create("TensorToPoint2D");
    CHECK(point_dec->outputTypeName() == "Point2D<float>");

    auto mask_dec = dl::DecoderFactory::create("TensorToMask2D");
    CHECK(mask_dec->outputTypeName() == "Mask2D");

    auto line_dec = dl::DecoderFactory::create("TensorToLine2D");
    CHECK(line_dec->outputTypeName() == "Line2D");
}
