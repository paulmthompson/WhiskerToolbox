/// @file OutputSlotWidget.test.cpp
/// @brief Tests for the OutputSlotWidget.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/OutputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QSignalSpy>

#include <type_traits>

static dl::TensorSlotDescriptor makeOutputSlot(
        std::string name = "mask_out",
        std::string recommended_decoder = "TensorToMask2D") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {1, 256, 256};
    slot.description = "Test output slot";
    slot.recommended_decoder = std::move(recommended_decoder);
    return slot;
}

TEST_CASE("OutputSlotWidget constructs with valid slot and DM",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    auto widget = std::make_unique<dl::widget::OutputSlotWidget>(slot, dm);
    CHECK(widget->slotName() == "mask_out");
}

TEST_CASE("OutputSlotWidget default binding has empty data_key",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    dl::widget::OutputSlotWidget const widget(slot, dm);
    auto binding = widget.binding();
    CHECK(binding.data_key.empty());
}

TEST_CASE("OutputSlotWidget setBinding and binding round-trip",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/result", TimeKey("time"));
    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    OutputBindingData original;
    original.slot_name = "mask_out";
    original.data_key = "masks/result";
    original.decoder = dl::MaskDecoderParams{.threshold = 0.7f};
    widget.setBinding(original);

    auto result = widget.binding();
    CHECK(result.data_key == "masks/result");
}

TEST_CASE("OutputSlotWidget refreshDataSources updates target combo",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/out", TimeKey("time"));

    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");
    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    auto binding = widget.binding();
    CHECK(binding.decoder.visit([](auto const &) { return true; }));
}

TEST_CASE("binding extracts correct fields from MaskDecoder",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/result", TimeKey("time"));
    auto slot = makeOutputSlot("mask_out", "TensorToMask2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    OutputBindingData binding_in;
    binding_in.data_key = "masks/result";
    binding_in.decoder = dl::MaskDecoderParams{.threshold = 0.6f};
    widget.setBinding(binding_in);

    auto binding = widget.binding();
    CHECK(binding.slot_name == "mask_out");
    CHECK(binding.data_key == "masks/result");
    binding.decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::MaskDecoderParams>) {
            CHECK(dec.threshold == 0.6f);
        }
    });
}

TEST_CASE("binding extracts subpixel from PointDecoder",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/out", TimeKey("time"));
    auto slot = makeOutputSlot("point_out", "TensorToPoint2D");

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();

    OutputBindingData binding_in;
    binding_in.data_key = "points/out";
    binding_in.decoder = dl::PointDecoderParams{
            .subpixel = false,
            .threshold = 0.4f};
    widget.setBinding(binding_in);

    auto binding = widget.binding();
    CHECK(binding.slot_name == "point_out");
    binding.decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::PointDecoderParams>) {
            CHECK(dec.subpixel == false);
            CHECK(dec.threshold == 0.4f);
        }
    });
}

TEST_CASE("setBinding restores saved binding",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/restored", TimeKey("time"));
    auto slot = makeOutputSlot("point_out", "TensorToPoint2D");

    OutputBindingData original;
    original.slot_name = "point_out";
    original.data_key = "points/restored";
    original.decoder =
            dl::PointDecoderParams{.subpixel = true, .threshold = 0.3f};

    dl::widget::OutputSlotWidget widget(slot, dm);
    widget.refreshDataSources();
    widget.setBinding(original);

    auto binding = widget.binding();
    CHECK(binding.data_key == "points/restored");
    binding.decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::PointDecoderParams>) {
            CHECK(dec.threshold == 0.3f);
            CHECK(dec.subpixel == true);
        }
    });
}

TEST_CASE("OutputSlotWidget emits bindingChanged on param changes",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    dl::widget::OutputSlotWidget widget(slot, dm);

    QSignalSpy const spy(&widget, &dl::widget::OutputSlotWidget::bindingChanged);

    OutputBindingData updated;
    updated.decoder = dl::LineDecoderParams{.threshold = 0.5f};
    widget.setBinding(updated);

    CHECK(spy.isValid());
}

TEST_CASE("binding normalizes None sentinel for data_key",
          "[dl_widget][output_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeOutputSlot();

    dl::widget::OutputSlotWidget widget(slot, dm);

    dl::OutputBindingForm form;
    form.data_key = "(None)";
    widget.setBinding(dl::toOutputBindingData(slot.name, form));

    CHECK(widget.binding().data_key.empty());
}
