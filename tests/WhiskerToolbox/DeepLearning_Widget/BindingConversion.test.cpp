/**
 * @file BindingConversion.test.cpp
 * @brief Tests for widget params ↔ binding data conversion.
 */

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/Core/BindingConversion.hpp"

TEST_CASE("fromStaticInputParams round-trips DataBank source",
          "[dl_widget][binding_conversion]") {
    dl::widget::StaticInputSlotParams params;
    params.source = dl::widget::DataBankStaticSourceParams{
            .bank_entry_id = "ref_mask_1"};

    auto const si = dl::conversion::fromStaticInputParams(
            "memory_masks", params);

    CHECK(si.slot_name == "memory_masks");
    CHECK(si.sourceType() == StaticInputSource::DataBank);
    CHECK(si.bank_entry_id == "ref_mask_1");
    CHECK(si.data_key.empty());

    auto const restored = dl::conversion::toStaticInputParams(si);
    bool is_bank = false;
    std::string bank_id;
    restored.source.visit([&](auto const & src) {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, dl::widget::DataBankStaticSourceParams>) {
            is_bank = true;
            bank_id = src.bank_entry_id;
        }
    });
    CHECK(is_bank);
    CHECK(bank_id == "ref_mask_1");
}

TEST_CASE("fromStaticInputParams round-trips DataManager source",
          "[dl_widget][binding_conversion]") {
    dl::widget::StaticInputSlotParams params;
    params.source = dl::widget::DataManagerStaticSourceParams{
            .data_key = "masks/ref",
            .time_offset = -2};

    auto const si = dl::conversion::fromStaticInputParams(
            "memory_masks", params);

    CHECK(si.sourceType() == StaticInputSource::DataManager);
    CHECK(si.data_key == "masks/ref");
    CHECK(si.time_offset == -2);
}

TEST_CASE("fromStaticSequenceEntryParams round-trips DataBank source",
          "[dl_widget][binding_conversion]") {
    dl::widget::StaticSequenceEntryParams params{
            .static_source_kind = "DataBank",
            .bank_entry_id = "seq_mask_0"};

    auto const si = dl::conversion::fromStaticSequenceEntryParams(
            "memory_masks", /*memory_index=*/1, params);

    CHECK(si.memory_index == 1);
    CHECK(si.bank_entry_id == "seq_mask_0");
    CHECK(si.sourceType() == StaticInputSource::DataBank);
}
