/// @file DataBankPropertiesWidget.test.cpp
/// @brief Tests for the DataBankPropertiesWidget capture controls.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/DataBank/DataBankPropertiesWidget.hpp"

#include "DataManager/DataManager.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QComboBox>
#include <QLabel>
#include <QSpinBox>

namespace {

dl::TensorSlotDescriptor makeStaticSlot(
        std::string name,
        bool has_sequence = false,
        int64_t seq_len = 4) {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.is_static = true;
    slot.recommended_encoder = "Mask2DEncoder";
    if (has_sequence) {
        slot.shape = {seq_len, 1, 256, 256};
        slot.sequence_dim = 0;
    } else {
        slot.shape = {1, 256, 256};
    }
    return slot;
}

}// namespace

TEST_CASE("DataBankPropertiesWidget constructs with DataManager",
          "[dl_widget][data_bank_properties]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::DataBankPropertiesWidget widget;
    widget.setDataManager(dm);
    CHECK(widget.findChild<QComboBox *>(QStringLiteral("dataSourceCombo")) !=
          nullptr);
}

TEST_CASE("DataBankPropertiesWidget setStaticSlots populates target combo",
          "[dl_widget][data_bank_properties]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::DataBankPropertiesWidget widget;
    widget.setDataManager(dm);

    std::vector<dl::TensorSlotDescriptor> static_slot_list = {
            makeStaticSlot("memory_images"),
            makeStaticSlot("memory_masks")};
    widget.setStaticSlots(static_slot_list);

    auto * combo = widget.findChild<QComboBox *>(QStringLiteral("targetSlotCombo"));
    REQUIRE(combo != nullptr);
    CHECK(combo->count() == 2);
}

TEST_CASE("DataBankPropertiesWidget shows default entry ID for selected slot",
          "[dl_widget][data_bank_properties]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::DataBankPropertiesWidget widget;
    widget.setDataManager(dm);
    widget.setStaticSlots({makeStaticSlot("memory_masks")});

    auto * entry_label =
            widget.findChild<QLabel *>(QStringLiteral("entryIdLabel"));
    REQUIRE(entry_label != nullptr);
    CHECK(entry_label->text().toStdString() == "memory_masks_0");
}

TEST_CASE("DataBankPropertiesWidget memory index updates entry ID",
          "[dl_widget][data_bank_properties]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::DataBankPropertiesWidget widget;
    widget.setDataManager(dm);
    widget.setStaticSlots({makeStaticSlot("memory_images", /*has_sequence=*/true)});
    widget.show();

    auto * index_spin =
            widget.findChild<QSpinBox *>(QStringLiteral("memoryIndexSpin"));
    auto * entry_label =
            widget.findChild<QLabel *>(QStringLiteral("entryIdLabel"));
    REQUIRE(index_spin != nullptr);
    REQUIRE(entry_label != nullptr);
    REQUIRE(index_spin->isVisible());
    CHECK(index_spin->maximum() == 3);

    index_spin->setValue(2);
    CHECK(entry_label->text().toStdString() == "memory_images_2");
}
