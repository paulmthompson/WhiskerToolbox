/// @file OutputSlotWidget.test.cpp
/// @brief Tests for the OutputSlotWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// toOutputBindingData conversion, and paramsFromBinding restore.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/OutputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QSignalSpy>

/// Helper: create a minimal output TensorSlotDescriptor for testing.
static dl::TensorSlotDescriptor makeOutputSlot(
        std::string name = "mask_out",
        std::string terminal_step = "TensorToMask2D") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {1, 256, 256};
    slot.description = "Test output slot";
    slot.recommended_pipeline = {dl::OutputPipelineStepSpec{
            .step_id = std::move(terminal_step)}};
    return slot;
}

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("OutputSlotWidget constructs with valid slot and DM",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    auto widget = std::make_unique<dl::widget::OutputSlotWidget>(slot, dm);
    CHECK(widget->slotName() == "mask_out");
}

TEST_CASE("OutputSlotWidget default params have empty data_key",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    dl::widget::OutputSlotWidget const widget(slot, dm);
    auto p = widget.params();
    auto is_empty = p.data_key.empty() || p.data_key == "(None)";
    CHECK(is_empty);
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("OutputSlotWidget setParams and params round-trip",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/result", TimeKey("time"));
    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    dl::widget::OutputSlotParams p;
    p.data_key = "masks/result";
    p.decoder = dl::MaskDecoderParams{.threshold = 0.7f};
    widget.setParams(p);

    auto result = widget.params();
    CHECK(result.data_key == "masks/result");
}

// ============================================================================
// Data source refresh
// ============================================================================

TEST_CASE("OutputSlotWidget refreshDataSources updates target combo",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/out", TimeKey("time"));

    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");
    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    auto p = widget.params();
    CHECK(p.decoder.visit([](auto const &) { return true; }));
}

// ============================================================================
// toOutputBindingData conversion
// ============================================================================

TEST_CASE("toOutputBindingData extracts correct fields from MaskDecoder",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/result", TimeKey("time"));
    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    dl::widget::OutputSlotParams p;
    p.data_key = "masks/result";
    p.decoder = dl::MaskDecoderParams{.threshold = 0.6f};
    widget.setParams(p);

    auto binding = widget.toOutputBindingData();
    CHECK(binding.slot_name == "mask_out");
    CHECK(binding.data_key == "masks/result");
    REQUIRE(binding.pipeline.size() == 1);
    CHECK(binding.pipeline[0].step_id == "TensorToMask2D");
    auto const mask_params = dl::maskDecoderParamsForStep(binding.pipeline[0]);
    CHECK(mask_params.threshold == 0.6f);
}

TEST_CASE("toOutputBindingData extracts subpixel from PointDecoder",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/out", TimeKey("time"));
    auto slot = makeOutputSlot("point_out", "TensorToPoint2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    dl::widget::OutputSlotParams p;
    p.data_key = "points/out";
    p.decoder = dl::PointDecoderParams{
            .subpixel = false,
            .threshold = 0.4f};
    widget.setParams(p);

    auto binding = widget.toOutputBindingData();
    CHECK(binding.slot_name == "point_out");
    REQUIRE(binding.pipeline.size() == 1);
    CHECK(binding.pipeline[0].step_id == "TensorToPoint2D");
    auto const point_params = dl::pointDecoderParamsForStep(binding.pipeline[0]);
    CHECK(point_params.subpixel == false);
    CHECK(point_params.threshold == 0.4f);
}

// ============================================================================
// paramsFromBinding / state restore
// ============================================================================

TEST_CASE("paramsFromBinding restores MaskDecoder binding",
          "[dl_widget][output_slot_widget]") {
    OutputBindingData binding;
    binding.slot_name = "mask_out";
    binding.data_key = "masks/saved";
    binding.pipeline = {dl::OutputPipelineStepSpec{
            .step_id = "TensorToMask2D",
            .parameters = dl::OutputPipelineStepParameters{
                    dl::MaskDecoderParams{.threshold = 0.55f}}}};

    auto params = dl::widget::OutputSlotWidget::paramsFromBinding(binding);
    CHECK(params.data_key == "masks/saved");
    params.decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::MaskDecoderParams>) {
            CHECK(dec.threshold == 0.55f);
        }
    });
}

TEST_CASE("setParams from paramsFromBinding round-trips toOutputBindingData",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/restored", TimeKey("time"));
    auto slot = makeOutputSlot("point_out", "TensorToPoint2D");

    OutputBindingData original;
    original.slot_name = "point_out";
    original.data_key = "points/restored";
    original.pipeline = {dl::OutputPipelineStepSpec{
            .step_id = "TensorToPoint2D",
            .parameters = dl::OutputPipelineStepParameters{
                    dl::PointDecoderParams{.subpixel = true, .threshold = 0.3f}}}};

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();
    widget.setParams(
            dl::widget::OutputSlotWidget::paramsFromBinding(original));

    auto binding = widget.toOutputBindingData();
    CHECK(binding.data_key == "points/restored");
    REQUIRE(binding.pipeline.size() == 1);
    CHECK(binding.pipeline[0].step_id == "TensorToPoint2D");
    auto const point_params = dl::pointDecoderParamsForStep(binding.pipeline[0]);
    CHECK(point_params.threshold == 0.3f);
    CHECK(point_params.subpixel == true);
}

// ============================================================================
// Signal emission
// ============================================================================

TEST_CASE("OutputSlotWidget emits bindingChanged on param changes",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    dl::widget::OutputSlotWidget widget(slot, dm);

    QSignalSpy const spy(&widget, &dl::widget::OutputSlotWidget::bindingChanged);

    dl::widget::OutputSlotParams p;
    p.decoder = dl::LineDecoderParams{.threshold = 0.5f};
    widget.setParams(p);

    CHECK(spy.isValid());
}
