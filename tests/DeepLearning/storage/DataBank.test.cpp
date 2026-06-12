/**
 * @file DataBank.test.cpp
 * @brief Unit tests for dl::DataBank named storage.
 */

#include "storage/DataBank.hpp"
#include "storage/DataBankEncode.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <ATen/Functions.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace {

dl::TensorSlotDescriptor makeMaskSlot() {
    dl::TensorSlotDescriptor slot;
    slot.name = "memory_masks";
    slot.shape = {1, 10, 10};
    slot.dtype = dl::TensorDType::Float32;
    slot.recommended_encoder = "Mask2DEncoder";
    return slot;
}

dl::TensorSlotDescriptor makeSequenceMaskSlot() {
    auto slot = makeMaskSlot();
    slot.shape = {2, 1, 10, 10};
    slot.sequence_dim = 0;
    return slot;
}

Mask2D makeSampleMask() {
    return Mask2D({Point2D<uint32_t>{2, 3},
                   Point2D<uint32_t>{5, 5},
                   Point2D<uint32_t>{8, 1}});
}

}// namespace

TEST_CASE("validateEntryId rejects invalid IDs", "[storage][DataBank]") {
    CHECK(dl::validateEntryId("").has_value());
    CHECK(dl::validateEntryId("   ").has_value());
    CHECK(dl::validateEntryId("bad id").has_value());
    CHECK(dl::validateEntryId("slot:0").has_value());
    CHECK_FALSE(dl::validateEntryId("ref_mask_1").has_value());
}

TEST_CASE("DataBank putSource and get round-trip", "[storage][DataBank]") {
    dl::DataBank bank;
    auto const mask = makeSampleMask();

    dl::DataBankEntryMetadata metadata;
    metadata.label = "Reference mask";
    metadata.data_key = "whisker_mask";
    metadata.source_image_size = ImageSize{10, 10};
    metadata.captured_frame = 42;

    REQUIRE(bank.putSource("ref_mask", mask, metadata));
    CHECK(bank.contains("ref_mask"));
    CHECK(bank.size() == 1);
    REQUIRE(bank.ids() == std::vector<std::string>{"ref_mask"});

    auto const source = bank.getSource("ref_mask");
    REQUIRE(source);
    REQUIRE(std::holds_alternative<Mask2D>(*source));
    CHECK(std::get<Mask2D>(*source).size() == 3);

    auto const entry = bank.get("ref_mask");
    REQUIRE(entry);
    CHECK(entry->metadata.label == "Reference mask");
    CHECK(entry->metadata.data_key == "whisker_mask");
    CHECK(entry->metadata.captured_frame == 42);
    CHECK_FALSE(entry->encoded.has_value());
}

TEST_CASE("DataBank encodeEntry encodes Mask2D source", "[storage][DataBank]") {
    dl::DataBank bank;
    auto const mask = makeSampleMask();

    dl::DataBankEntryMetadata metadata;
    metadata.source_image_size = ImageSize{10, 10};

    REQUIRE(bank.putSource("ref_mask", mask, metadata));

    auto const slot = makeMaskSlot();
    dl::Mask2DEncoderParams params;
    REQUIRE(bank.encodeEntry("ref_mask", slot, params));

    auto const encoded = bank.getEncoded("ref_mask");
    REQUIRE(encoded);
    CHECK(encoded->dim() == 4);
    CHECK(encoded->size(0) == 1);
    CHECK(encoded->size(1) == 1);
    CHECK(encoded->size(2) == 10);
    CHECK(encoded->size(3) == 10);

    auto accessor = encoded->accessor<float, 4>();
    CHECK_THAT(accessor[0][0][3][2], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(accessor[0][0][5][5], WithinAbs(1.0f, 1e-5f));

    REQUIRE(bank.encodedShape("ref_mask") == std::vector<int64_t>({1, 1, 10, 10}));
    auto const [vmin, vmax] = bank.encodedRange("ref_mask");
    CHECK_THAT(static_cast<double>(vmin), WithinAbs(0.0, 1e-5));
    CHECK_THAT(static_cast<double>(vmax), WithinAbs(1.0, 1e-5));

    auto const entry = bank.get("ref_mask");
    REQUIRE(entry);
    CHECK(entry->metadata.encoder_factory_name == "Mask2DEncoder");
}

TEST_CASE("DataBank encodeEntry supports sequence slots", "[storage][DataBank]") {
    dl::DataBank bank;
    auto const mask = makeSampleMask();

    dl::DataBankEntryMetadata metadata;
    metadata.source_image_size = ImageSize{10, 10};
    REQUIRE(bank.putSource("seq_mask", mask, metadata));

    auto const slot = makeSequenceMaskSlot();
    dl::Mask2DEncoderParams params;
    REQUIRE(bank.encodeEntry("seq_mask", slot, params));

    REQUIRE(bank.encodedShape("seq_mask") == std::vector<int64_t>({1, 1, 10, 10}));
}

TEST_CASE("encodeSourceToTensor encodes Point2D source", "[storage][DataBankEncode]") {
    std::vector<Point2D<float>> points{{4.0f, 4.0f}};

    dl::TensorSlotDescriptor slot;
    slot.shape = {1, 8, 8};
    slot.dtype = dl::TensorDType::Float32;

    dl::Point2DEncoderParams params{.mode = dl::RasterMode::Binary};
    auto const encoded = dl::encodeSourceToTensor(
            points, slot, params, ImageSize{8, 8});
    REQUIRE(encoded);
    CHECK_THAT((*encoded)[0][0][4][4].item<float>(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("DataBank putEncoded stores tensor directly", "[storage][DataBank]") {
    dl::DataBank bank;
    auto tensor = at::ones({1, 1, 4, 4});

    dl::DataBankEntryMetadata metadata;
    metadata.encoder_factory_name = "Mask2DEncoder";

    REQUIRE(bank.putEncoded("cached", std::move(tensor), metadata));
    REQUIRE(bank.encodedRange("cached") == std::pair<float, float>({1.0f, 1.0f}));
    CHECK_FALSE(bank.getSource("cached").has_value());
}

TEST_CASE("DataBank remove and clear", "[storage][DataBank]") {
    dl::DataBank bank;
    dl::DataBankEntryMetadata metadata;
    REQUIRE(bank.putSource("a", makeSampleMask(), metadata));
    REQUIRE(bank.putSource("b", makeSampleMask(), metadata));
    CHECK(bank.size() == 2);

    bank.remove("a");
    CHECK(bank.size() == 1);
    CHECK_FALSE(bank.contains("a"));

    bank.clear();
    CHECK(bank.size() == 0);
}

TEST_CASE("DataBank enforces max entry limit", "[storage][DataBank]") {
    dl::DataBank bank{2};
    dl::DataBankEntryMetadata metadata;

    REQUIRE(bank.putSource("one", makeSampleMask(), metadata));
    REQUIRE(bank.putSource("two", makeSampleMask(), metadata));
    CHECK_FALSE(bank.putSource("three", makeSampleMask(), metadata));
    CHECK(bank.size() == 2);
}

TEST_CASE("DataBank rejects invalid put IDs", "[storage][DataBank]") {
    dl::DataBank bank;
    dl::DataBankEntryMetadata metadata;
    CHECK_FALSE(bank.putSource("", makeSampleMask(), metadata));
    CHECK_FALSE(bank.putEncoded("bad:id", at::zeros({1, 1, 2, 2}), metadata));
}

TEST_CASE("DataBank rejects putEncoded without batch dim 1", "[storage][DataBank]") {
    dl::DataBank bank;
    dl::DataBankEntryMetadata metadata;
    CHECK_FALSE(bank.putEncoded("bad_batch", at::zeros({2, 1, 2, 2}), metadata));
}

TEST_CASE("encodingShapeForSlot excludes sequence dimension", "[storage][DataBankEncode]") {
    auto const slot = makeSequenceMaskSlot();
    REQUIRE(dl::encodingShapeForSlot(slot) == std::vector<int64_t>({1, 10, 10}));
}
