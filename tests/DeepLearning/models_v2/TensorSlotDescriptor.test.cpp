#include <catch2/catch_test_macros.hpp>

#include "models_v2/TensorSlotDescriptor.hpp"

TEST_CASE("TensorSlotDescriptor - default construction", "[TensorSlotDescriptor]") {
    dl::TensorSlotDescriptor const slot;

    CHECK(slot.name.empty());
    CHECK(slot.shape.empty());
    CHECK(slot.description.empty());
    CHECK(slot.recommended_encoder.empty());
    CHECK(slot.recommended_decoder.empty());
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

TEST_CASE("TensorSlotDescriptor - aggregate initialization", "[TensorSlotDescriptor]") {
    dl::TensorSlotDescriptor slot{
            .name = "encoder_image",
            .shape = {3, 256, 256},
            .description = "Current video frame",
            .recommended_encoder = "ImageEncoder",
            .recommended_decoder = "",
            .is_static = false,
            .is_boolean_mask = false,
            .sequence_dim = -1};

    CHECK(slot.name == "encoder_image");
    CHECK(slot.shape == std::vector<int64_t>{3, 256, 256});
    CHECK(slot.description == "Current video frame");
    CHECK(slot.recommended_encoder == "ImageEncoder");
    CHECK(slot.recommended_decoder.empty());
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

TEST_CASE("TensorSlotDescriptor - numElements", "[TensorSlotDescriptor]") {
    SECTION("3D shape") {
        dl::TensorSlotDescriptor const slot{.shape = {3, 256, 256}};
        CHECK(slot.numElements() == static_cast<int64_t>(3 * 256 * 256));
    }

    SECTION("1D shape") {
        dl::TensorSlotDescriptor const slot{.shape = {4}};
        CHECK(slot.numElements() == 4);
    }

    SECTION("empty shape (scalar)") {
        dl::TensorSlotDescriptor const slot{.shape = {}};
        CHECK(slot.numElements() == 1);
    }

    SECTION("4D shape with sequence dimension") {
        dl::TensorSlotDescriptor const slot{.shape = {4, 3, 256, 256}};
        CHECK(slot.numElements() == static_cast<int64_t>(4 * 3 * 256 * 256));
    }
}

TEST_CASE("TensorSlotDescriptor - hasSequenceDim", "[TensorSlotDescriptor]") {
    SECTION("no sequence dimension (default)") {
        dl::TensorSlotDescriptor const slot{.sequence_dim = -1};
        CHECK_FALSE(slot.hasSequenceDim());
    }

    SECTION("with sequence dimension") {
        dl::TensorSlotDescriptor const slot{.sequence_dim = 0};
        CHECK(slot.hasSequenceDim());
    }

    SECTION("negative is no sequence") {
        dl::TensorSlotDescriptor const slot{.sequence_dim = -2};
        CHECK_FALSE(slot.hasSequenceDim());
    }
}

TEST_CASE("TensorSlotDescriptor - static and boolean mask flags", "[TensorSlotDescriptor]") {
    dl::TensorSlotDescriptor const memory_mask{
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

TEST_CASE("SlotDirection enum", "[TensorSlotDescriptor]") {
    auto input = dl::SlotDirection::Input;
    auto output = dl::SlotDirection::Output;
    CHECK(input != output);
}

// ════════════════════════════════════════════════════════════════════════════
// Sequence dimension support (Phase 3)
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("TensorSlotDescriptor - sequence slot with dim 0",
          "[TensorSlotDescriptor][sequence]") {
    dl::TensorSlotDescriptor slot{
            .name = "memory_images",
            .shape = {4, 3, 256, 256},
            .description = "Memory frames stacked along dim 0",
            .is_static = true,
            .sequence_dim = 0};

    CHECK(slot.hasSequenceDim());
    CHECK(slot.shape[0] == 4);// sequence length
    CHECK(slot.numElements() == static_cast<int64_t>(4 * 3 * 256 * 256));
}

TEST_CASE("TensorSlotDescriptor - sequence slot with dim 1",
          "[TensorSlotDescriptor][sequence]") {
    dl::TensorSlotDescriptor slot{
            .name = "stacked_features",
            .shape = {3, 8, 64, 64},
            .description = "Features stacked along dim 1",
            .is_static = true,
            .sequence_dim = 1};

    CHECK(slot.hasSequenceDim());
    CHECK(slot.shape[1] == 8);// sequence length
}

TEST_CASE("TensorSlotDescriptor - boolean mask for sequence slots",
          "[TensorSlotDescriptor][sequence]") {
    dl::TensorSlotDescriptor mask_slot{
            .name = "memory_active",
            .shape = {4},
            .description = "Active flags for 4 memory positions",
            .is_static = true,
            .is_boolean_mask = true,
            .sequence_dim = 0};

    CHECK(mask_slot.hasSequenceDim());
    CHECK(mask_slot.is_boolean_mask);
    CHECK(mask_slot.is_static);
    CHECK(mask_slot.shape[0] == 4);
}

// ════════════════════════════════════════════════════════════════════════════
// BatchMode (Phase 6)
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("BatchMode - FixedBatch", "[BatchMode]") {
    dl::BatchMode mode = dl::FixedBatch{4};

    CHECK(dl::maxBatchSizeFromMode(mode) == 4);
    CHECK(dl::minBatchSizeFromMode(mode) == 4);
    CHECK_FALSE(dl::isBatchLocked(mode));
    CHECK(dl::batchModeDescription(mode) == "Fixed(4)");
}

TEST_CASE("BatchMode - FixedBatch size=1 is locked", "[BatchMode]") {
    dl::BatchMode mode = dl::FixedBatch{1};

    CHECK(dl::isBatchLocked(mode));
    CHECK(dl::maxBatchSizeFromMode(mode) == 1);
    CHECK(dl::minBatchSizeFromMode(mode) == 1);
}

TEST_CASE("BatchMode - DynamicBatch unlimited", "[BatchMode]") {
    dl::BatchMode mode = dl::DynamicBatch{1, 0};

    CHECK(dl::maxBatchSizeFromMode(mode) == 0);
    CHECK(dl::minBatchSizeFromMode(mode) == 1);
    CHECK_FALSE(dl::isBatchLocked(mode));
    CHECK(dl::batchModeDescription(mode) == "Dynamic(1, unlimited)");
}

TEST_CASE("BatchMode - DynamicBatch bounded", "[BatchMode]") {
    dl::BatchMode mode = dl::DynamicBatch{2, 16};

    CHECK(dl::maxBatchSizeFromMode(mode) == 16);
    CHECK(dl::minBatchSizeFromMode(mode) == 2);
    CHECK_FALSE(dl::isBatchLocked(mode));
    CHECK(dl::batchModeDescription(mode) == "Dynamic(2, 16)");
}

TEST_CASE("BatchMode - RecurrentOnlyBatch", "[BatchMode]") {
    dl::BatchMode mode = dl::RecurrentOnlyBatch{};

    CHECK(dl::isBatchLocked(mode));
    CHECK(dl::maxBatchSizeFromMode(mode) == 1);
    CHECK(dl::minBatchSizeFromMode(mode) == 1);
    CHECK(dl::batchModeDescription(mode) == "RecurrentOnly");
}
