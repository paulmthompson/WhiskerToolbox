/// @file RecurrentBindingWidget.test.cpp
/// @brief Tests for the RecurrentBindingWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// toRecurrentBindingData conversion, and paramsFromBinding restore.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/RecurrentBindingWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QApplication>
#include <QSignalSpy>

#include <vector>

// Ensure a QApplication exists for QWidget-based tests
namespace {
struct QtAppGuard {
    QtAppGuard() {
        if (QApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            // Intentionally leaked to avoid destruction-order issues with Catch2
            new QApplication(argc, const_cast<char **>(argv));
        }
    }
};
QtAppGuard const s_guard;
}// namespace

/// Helper: create a minimal input TensorSlotDescriptor for testing.
static dl::TensorSlotDescriptor makeInputSlot(
        std::string name = "memory_hidden",
        std::string recommended_encoder = "ImageEncoder") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {1, 64};
    slot.is_static = true;
    slot.description = "Test recurrent input slot";
    slot.recommended_encoder = std::move(recommended_encoder);
    return slot;
}

/// Helper: create a minimal output TensorSlotDescriptor for testing.
static dl::TensorSlotDescriptor makeOutputSlot(std::string name = "hidden_out") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {1, 64};
    slot.description = "Test output slot";
    return slot;
}

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("RecurrentBindingWidget constructs with valid slot and DM",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto input_slot = makeInputSlot();
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("out1")};

    auto widget = std::make_unique<dl::widget::RecurrentBindingWidget>(
            input_slot, outputs, dm);
    CHECK(widget->slotName() == "memory_hidden");
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("RecurrentBindingWidget setParams and params round-trip",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto input_slot = makeInputSlot();
    std::vector<dl::TensorSlotDescriptor> outputs{
            makeOutputSlot("out1"),
            makeOutputSlot("out2"),
    };

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);
    widget.refreshDataSources();

    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = "out2";
    p.init = dl::widget::ZerosInitParams{};
    widget.setParams(p);

    auto result = widget.params();
    CHECK(result.output_slot_name == "out2");
}

// ============================================================================
// toRecurrentBindingData conversion
// ============================================================================

TEST_CASE("toRecurrentBindingData extracts Zeros init",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto input_slot = makeInputSlot();
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("hidden_out")};

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);

    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = "hidden_out";
    p.init = dl::widget::ZerosInitParams{};
    widget.setParams(p);

    auto binding = widget.toRecurrentBindingData();
    CHECK(binding.input_slot_name == "memory_hidden");
    CHECK(binding.output_slot_name == "hidden_out");
    CHECK(binding.init_mode_str == "Zeros");
}

TEST_CASE("toRecurrentBindingData extracts StaticCapture init",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/init", TimeKey("time"));
    auto input_slot = makeInputSlot("memory_hidden", "Mask2DEncoder");
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("mask_out")};

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);
    widget.refreshDataSources();

    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = "mask_out";
    p.init = dl::widget::StaticCaptureInitParams{
            .data_key = "masks/init",
            .frame = 42};
    widget.setParams(p);

    auto binding = widget.toRecurrentBindingData();
    CHECK(binding.input_slot_name == "memory_hidden");
    CHECK(binding.output_slot_name == "mask_out");
    CHECK(binding.init_mode_str == "StaticCapture");
    CHECK(binding.init_data_key == "masks/init");
    CHECK(binding.init_frame == 42);
}

TEST_CASE("toRecurrentBindingData extracts FirstOutput init",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto input_slot = makeInputSlot();
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("out1")};

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);

    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = "out1";
    p.init = dl::widget::FirstOutputInitParams{};
    widget.setParams(p);

    auto binding = widget.toRecurrentBindingData();
    CHECK(binding.init_mode_str == "FirstOutput");
}

// ============================================================================
// paramsFromBinding / state restore
// ============================================================================

TEST_CASE("paramsFromBinding restores Zeros binding",
          "[dl_widget][recurrent_binding_widget]") {
    RecurrentBindingData binding;
    binding.input_slot_name = "memory_hidden";
    binding.output_slot_name = "hidden_out";
    binding.init_mode_str = "Zeros";

    auto params =
            dl::widget::RecurrentBindingWidget::paramsFromBinding(binding);
    CHECK(params.output_slot_name == "hidden_out");
    bool is_zeros = false;
    params.init.visit([&](auto const & init) {
        using T = std::decay_t<decltype(init)>;
        is_zeros = std::is_same_v<T, dl::widget::ZerosInitParams>;
    });
    CHECK(is_zeros);
}

TEST_CASE("paramsFromBinding restores StaticCapture binding",
          "[dl_widget][recurrent_binding_widget]") {
    RecurrentBindingData binding;
    binding.input_slot_name = "memory_hidden";
    binding.output_slot_name = "mask_out";
    binding.init_mode_str = "StaticCapture";
    binding.init_data_key = "masks/init";
    binding.init_frame = 10;

    auto params =
            dl::widget::RecurrentBindingWidget::paramsFromBinding(binding);
    CHECK(params.output_slot_name == "mask_out");
    params.init.visit([&](auto const & init) {
        using T = std::decay_t<decltype(init)>;
        if constexpr (std::is_same_v<T, dl::widget::StaticCaptureInitParams>) {
            CHECK(init.data_key == "masks/init");
            CHECK(init.frame == 10);
        }
    });
}

TEST_CASE("setParams from paramsFromBinding round-trips toRecurrentBindingData",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/init", TimeKey("time"));
    auto input_slot = makeInputSlot("mem", "Point2DEncoder");
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("pt_out")};

    RecurrentBindingData original;
    original.input_slot_name = "mem";
    original.output_slot_name = "pt_out";
    original.init_mode_str = "StaticCapture";
    original.init_data_key = "points/init";
    original.init_frame = 5;

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);
    widget.refreshDataSources();
    widget.setParams(
            dl::widget::RecurrentBindingWidget::paramsFromBinding(original));

    auto binding = widget.toRecurrentBindingData();
    CHECK(binding.output_slot_name == "pt_out");
    CHECK(binding.init_mode_str == "StaticCapture");
    CHECK(binding.init_data_key == "points/init");
    CHECK(binding.init_frame == 5);
}

// ============================================================================
// Signal emission
// ============================================================================

TEST_CASE("RecurrentBindingWidget emits bindingChanged on param changes",
          "[dl_widget][recurrent_binding_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto input_slot = makeInputSlot();
    std::vector<dl::TensorSlotDescriptor> outputs{makeOutputSlot("out1")};

    dl::widget::RecurrentBindingWidget widget(input_slot, outputs, dm);

    QSignalSpy const spy(&widget,
                         &dl::widget::RecurrentBindingWidget::bindingChanged);

    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = "out1";
    p.init = dl::widget::FirstOutputInitParams{};
    widget.setParams(p);

    CHECK(spy.isValid());
}
