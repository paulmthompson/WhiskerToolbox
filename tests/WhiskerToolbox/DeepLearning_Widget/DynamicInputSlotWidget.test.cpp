/// @file DynamicInputSlotWidget.test.cpp
/// @brief Tests for the DynamicInputSlotWidget.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/DynamicInputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QSignalSpy>

#include <type_traits>

static dl::TensorSlotDescriptor makeTestSlot(
        std::string name = "encoder_image",
        std::string recommended_encoder = "ImageEncoder") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {3, 256, 256};
    slot.description = "Test dynamic input slot";
    slot.recommended_encoder = std::move(recommended_encoder);
    return slot;
}

TEST_CASE("DynamicInputSlotWidget constructs with valid slot and DM",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeTestSlot();

    auto widget = std::make_unique<dl::widget::DynamicInputSlotWidget>(slot, dm);
    CHECK(widget->slotName() == "encoder_image");
}

TEST_CASE("DynamicInputSlotWidget default binding has empty data_key",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeTestSlot();

    dl::widget::DynamicInputSlotWidget const widget(slot, dm);
    auto binding = widget.binding();
    CHECK(binding.data_key.empty());
    CHECK(binding.time_offset == 0);
}

TEST_CASE("DynamicInputSlotWidget setBinding and binding round-trip",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeTestSlot("enc_pts", "Point2DEncoder");

    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    SlotBindingData original;
    original.slot_name = "enc_pts";
    original.encoder = dl::Point2DEncoderParams{
            .mode = dl::RasterMode::Heatmap,
            .gaussian_sigma = 4.0f};
    original.time_offset = -3;

    widget.setBinding(original);
    auto result = widget.binding();

    CHECK(result.time_offset == -3);
    CHECK(result.slot_name == "enc_pts");
}

TEST_CASE("DynamicInputSlotWidget refreshDataSources updates data_key combo",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/whisker", TimeKey("time"));
    dm->setData<PointData>("points/other", TimeKey("time"));

    auto slot = makeTestSlot("enc_pts", "Point2DEncoder");
    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    widget.refreshDataSources();

    auto binding = widget.binding();
    CHECK(binding.time_offset == 0);
}

TEST_CASE("DynamicInputSlotWidget emits bindingChanged on param changes",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeTestSlot();

    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    QSignalSpy const spy(&widget, &dl::widget::DynamicInputSlotWidget::bindingChanged);

    SlotBindingData updated;
    updated.encoder = dl::ImageEncoderParams{};
    updated.time_offset = 5;
    widget.setBinding(updated);

    CHECK(spy.isValid());
}

TEST_CASE("binding extracts correct fields from ImageEncoder",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("test_source", TimeKey("time"));
    auto slot = makeTestSlot("enc_img", "ImageEncoder");

    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    SlotBindingData binding_in;
    binding_in.data_key = "test_source";
    binding_in.encoder = dl::ImageEncoderParams{.normalize = true};
    binding_in.time_offset = 2;
    widget.setBinding(binding_in);

    auto binding = widget.binding();
    CHECK(binding.slot_name == "enc_img");
    CHECK(binding.data_key == "test_source");
    CHECK(binding.time_offset == 2);
    binding.encoder.visit([&](auto const & enc) {
        using T = std::decay_t<decltype(enc)>;
        if constexpr (std::is_same_v<T, dl::ImageEncoderParams>) {
            CHECK(enc.normalize);
        }
    });
}

TEST_CASE("binding extracts mode and sigma from Point2DEncoder",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/whisker", TimeKey("time"));
    auto slot = makeTestSlot("enc_pts", "Point2DEncoder");

    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    SlotBindingData binding_in;
    binding_in.data_key = "points/whisker";
    binding_in.encoder = dl::Point2DEncoderParams{
            .mode = dl::RasterMode::Heatmap,
            .gaussian_sigma = 5.0f};
    binding_in.time_offset = -1;
    widget.setBinding(binding_in);

    auto binding = widget.binding();
    CHECK(binding.slot_name == "enc_pts");
    CHECK(binding.time_offset == -1);
    binding.encoder.visit([&](auto const & enc) {
        using T = std::decay_t<decltype(enc)>;
        if constexpr (std::is_same_v<T, dl::Point2DEncoderParams>) {
            CHECK(enc.mode == dl::RasterMode::Heatmap);
            CHECK(enc.gaussian_sigma == 5.0f);
        }
    });
}

TEST_CASE("binding normalizes None sentinel for data_key",
          "[dl_widget][dynamic_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeTestSlot();

    dl::widget::DynamicInputSlotWidget widget(slot, dm);

    dl::DynamicInputBindingForm form;
    form.data_key = "(None)";
    widget.setBinding(dl::toSlotBindingData(slot.name, form));

    CHECK(widget.binding().data_key.empty());
}
