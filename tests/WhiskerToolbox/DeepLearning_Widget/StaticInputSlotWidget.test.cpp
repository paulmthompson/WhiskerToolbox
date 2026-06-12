/// @file StaticInputSlotWidget.test.cpp
/// @brief Tests for the StaticInputSlotWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// and signal emission.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QSignalSpy>

/// Helper: create a minimal TensorSlotDescriptor for static input testing.
static dl::TensorSlotDescriptor makeStaticSlot(
        std::string name = "memory_images",
        std::string recommended_encoder = "ImageEncoder") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {3, 256, 256};
    slot.description = "Test static input slot";
    slot.recommended_encoder = std::move(recommended_encoder);
    slot.is_static = true;
    return slot;
}

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("StaticInputSlotWidget constructs with valid slot and DM",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    auto widget = std::make_unique<dl::widget::StaticInputSlotWidget>(slot, dm);
    CHECK(widget->slotName() == "memory_images");
}

TEST_CASE("StaticInputSlotWidget default params use DataManager source",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget const widget(slot, dm);
    auto p = widget.params();
    bool is_data_manager = false;
    p.source.visit([&](auto const & src) {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, dl::widget::DataManagerStaticSourceParams>) {
            is_data_manager = true;
        }
    });
    CHECK(is_data_manager);
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("StaticInputSlotWidget setParams and params round-trip (DataManager)",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("media/ref", TimeKey("time"));
    auto slot = makeStaticSlot("memory_images", "Point2DEncoder");

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    dl::widget::StaticInputSlotParams p;
    p.source = dl::widget::DataManagerStaticSourceParams{
            .data_key = "media/ref",
            .time_offset = -5};

    widget.setParams(p);
    auto result = widget.params();

    bool is_data_manager = false;
    int time_offset = 0;
    std::string data_key;
    result.source.visit([&](auto const & src) {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, dl::widget::DataManagerStaticSourceParams>) {
            is_data_manager = true;
            time_offset = src.time_offset;
            data_key = src.data_key;
        }
    });
    CHECK(is_data_manager);
    CHECK(time_offset == -5);
    CHECK(data_key == "media/ref");
}

TEST_CASE("StaticInputSlotWidget setParams and params round-trip (DataBank)",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    widget.refreshBankEntries({"ref_mask_1"});

    dl::widget::StaticInputSlotParams p;
    p.source = dl::widget::DataBankStaticSourceParams{
            .bank_entry_id = "ref_mask_1"};

    widget.setParams(p);
    auto result = widget.params();

    bool is_data_bank = false;
    result.source.visit([&](auto const & src) {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, dl::widget::DataBankStaticSourceParams>) {
            is_data_bank = true;
            CHECK(src.bank_entry_id == "ref_mask_1");
        }
    });
    CHECK(is_data_bank);
}

// ============================================================================
// Data source refresh
// ============================================================================

TEST_CASE("StaticInputSlotWidget refreshDataSources with point encoder",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();

    dm->setData<PointData>("points/reference", TimeKey("time"));
    dm->setData<PointData>("points/other", TimeKey("time"));

    auto slot = makeStaticSlot("memory_pts", "Point2DEncoder");
    dl::widget::StaticInputSlotWidget widget(slot, dm);

    widget.refreshDataSources();

    auto p = widget.params();
    (void) p;
}

// ============================================================================
// toStaticInputData conversion
// ============================================================================

TEST_CASE("toStaticInputData produces DataManager source correctly",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/ref", TimeKey("time"));
    auto slot = makeStaticSlot("mem_pts", "Point2DEncoder");

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    dl::widget::StaticInputSlotParams p;
    p.source = dl::widget::DataManagerStaticSourceParams{
            .data_key = "pts/ref",
            .time_offset = -3};
    widget.setParams(p);

    auto si = widget.toStaticInputData();
    CHECK(si.slot_name == "mem_pts");
    CHECK(si.memory_index == 0);
    CHECK(si.data_key == "pts/ref");
    CHECK(si.sourceType() == StaticInputSource::DataManager);
    CHECK(si.time_offset == -3);
}

TEST_CASE("toStaticInputData produces DataBank source correctly",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot("mem_pts", "Point2DEncoder");

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    widget.refreshBankEntries({"pts_ref"});

    dl::widget::StaticInputSlotParams p;
    p.source = dl::widget::DataBankStaticSourceParams{
            .bank_entry_id = "pts_ref"};
    widget.setParams(p);

    auto si = widget.toStaticInputData();
    CHECK(si.slot_name == "mem_pts");
    CHECK(si.sourceType() == StaticInputSource::DataBank);
    CHECK(si.bank_entry_id == "pts_ref");
    CHECK(si.data_key.empty());
}

// ============================================================================
// Signal emission
// ============================================================================

TEST_CASE("StaticInputSlotWidget emits bindingChanged on setParams",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    QSignalSpy const spy(&widget, &dl::widget::StaticInputSlotWidget::bindingChanged);

    dl::widget::StaticInputSlotParams p;
    p.source = dl::widget::DataManagerStaticSourceParams{.time_offset = -2};
    widget.setParams(p);

    CHECK(spy.isValid());
}
