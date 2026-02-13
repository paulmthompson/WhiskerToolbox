#include <catch2/catch_test_macros.hpp>

#include "models_v2/TensorSlotDescriptor.hpp"

TEST_CASE("TensorSlotDescriptor - default construction", "[TensorSlotDescriptor]")
{
    dl::TensorSlotDescriptor slot;

    CHECK(slot.name.empty());
    CHECK(slot.shape.empty());
    CHECK(slot.description.empty());
    CHECK(slot.recommended_encoder.empty());
    CHECK(slot.recommended_decoder.empty());
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

TEST_CASE("TensorSlotDescriptor - aggregate initialization", "[TensorSlotDescriptor]")
{
    dl::TensorSlotDescriptor slot{
        .name = "encoder_image",
        .shape = {3, 256, 256},
        .description = "Current video frame",
        .recommended_encoder = "ImageEncoder",
        .recommended_decoder = "",
        .is_static = false,
        .is_boolean_mask = false,
        .sequence_dim = -1
    };

    CHECK(slot.name == "encoder_image");
    CHECK(slot.shape == std::vector<int64_t>{3, 256, 256});
    CHECK(slot.description == "Current video frame");
    CHECK(slot.recommended_encoder == "ImageEncoder");
    CHECK(slot.recommended_decoder.empty());
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

TEST_CASE("TensorSlotDescriptor - numElements", "[TensorSlotDescriptor]")
{
    SECTION("3D shape") {
        dl::TensorSlotDescriptor slot{.shape = {3, 256, 256}};
        CHECK(slot.numElements() == 3 * 256 * 256);
    }

    SECTION("1D shape") {
        dl::TensorSlotDescriptor slot{.shape = {4}};
        CHECK(slot.numElements() == 4);
    }

    SECTION("empty shape (scalar)") {
        dl::TensorSlotDescriptor slot{.shape = {}};
        CHECK(slot.numElements() == 1);
    }

    SECTION("4D shape with sequence dimension") {
        dl::TensorSlotDescriptor slot{.shape = {4, 3, 256, 256}};
        CHECK(slot.numElements() == 4 * 3 * 256 * 256);
    }
}

TEST_CASE("TensorSlotDescriptor - hasSequenceDim", "[TensorSlotDescriptor]")
{
    SECTION("no sequence dimension (default)") {
        dl::TensorSlotDescriptor slot{.sequence_dim = -1};
        CHECK_FALSE(slot.hasSequenceDim());
    }

    SECTION("with sequence dimension") {
        dl::TensorSlotDescriptor slot{.sequence_dim = 0};
        CHECK(slot.hasSequenceDim());
    }

    SECTION("negative is no sequence") {
        dl::TensorSlotDescriptor slot{.sequence_dim = -2};
        CHECK_FALSE(slot.hasSequenceDim());
    }
}

TEST_CASE("TensorSlotDescriptor - static and boolean mask flags", "[TensorSlotDescriptor]")
{
    dl::TensorSlotDescriptor memory_mask{
        .name = "memory_mask",
        .shape = {1},
        .description = "Boolean active flags",
        .is_static = true,
        .is_boolean_mask = true,
    };

    CHECK(memory_mask.is_static == true);
    CHECK(memory_mask.is_boolean_mask == true);
    CHECK(memory_mask.numElements() == 1);
}

TEST_CASE("SlotDirection enum", "[TensorSlotDescriptor]")
{
    auto input = dl::SlotDirection::Input;
    auto output = dl::SlotDirection::Output;
    CHECK(input != output);
}
