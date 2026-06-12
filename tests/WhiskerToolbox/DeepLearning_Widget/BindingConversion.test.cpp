/**
 * @file BindingConversion.test.cpp
 * @brief Tests for widget params ↔ binding data conversion.
 */

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/Core/BindingConversion.hpp"

TEST_CASE("fromStaticInputParams round-trips bank_entry_id",
          "[dl_widget][binding_conversion]") {
    dl::widget::StaticInputSlotParams params;
    params.source = "masks/ref";
    params.bank_entry_id = "ref_mask_1";
    params.capture_mode = dl::widget::AbsoluteCaptureParams{};

    auto const si = dl::conversion::fromStaticInputParams(
            "memory_masks", params, /*captured_frame=*/42);

    CHECK(si.slot_name == "memory_masks");
    CHECK(si.data_key == "masks/ref");
    CHECK(si.bank_entry_id == "ref_mask_1");
    CHECK(si.capture_mode_str == "Absolute");
    CHECK(si.captured_frame == 42);

    auto const restored = dl::conversion::toStaticInputParams(si);
    CHECK(restored.source == "masks/ref");
    CHECK(restored.bank_entry_id == "ref_mask_1");
}

TEST_CASE("fromStaticSequenceEntryParams round-trips bank_entry_id",
          "[dl_widget][binding_conversion]") {
    dl::widget::StaticSequenceEntryParams params{
            .data_key = "masks/ref",
            .bank_entry_id = "seq_mask_0",
            .capture_mode_str = "Relative",
            .time_offset = -1};

    auto const si = dl::conversion::fromStaticSequenceEntryParams(
            "memory_masks", /*memory_index=*/1, params, /*captured_frame=*/-1);

    CHECK(si.memory_index == 1);
    CHECK(si.bank_entry_id == "seq_mask_0");
    CHECK(si.data_key == "masks/ref");
}
