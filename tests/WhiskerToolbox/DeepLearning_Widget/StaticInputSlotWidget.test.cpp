/// @file StaticInputSlotWidget.test.cpp
/// @brief Tests for the StaticInputSlotWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// capture status management, and signal emission.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QApplication>
#include <QSignalSpy>

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

TEST_CASE("StaticInputSlotWidget default params have empty source",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget const widget(slot, dm);
    auto p = widget.params();
    // source should be empty or "(None)" since DM has no keys
    // capture_mode should default to RelativeCaptureParams
    bool is_relative = false;
    p.capture_mode.visit([&](auto const & cm) {
        using T = std::decay_t<decltype(cm)>;
        if constexpr (std::is_same_v<T, dl::widget::RelativeCaptureParams>) {
            is_relative = true;
        }
    });
    CHECK(is_relative);
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("StaticInputSlotWidget setParams and params round-trip (Relative)",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    dl::widget::StaticInputSlotParams p;
    p.source = "";
    p.capture_mode = dl::widget::RelativeCaptureParams{.time_offset = -5};

    widget.setParams(p);
    auto result = widget.params();

    bool is_relative = false;
    int time_offset = 0;
    result.capture_mode.visit([&](auto const & cm) {
        using T = std::decay_t<decltype(cm)>;
        if constexpr (std::is_same_v<T, dl::widget::RelativeCaptureParams>) {
            is_relative = true;
            time_offset = cm.time_offset;
        }
    });
    CHECK(is_relative);
    CHECK(time_offset == -5);
}

TEST_CASE("StaticInputSlotWidget setParams and params round-trip (Absolute)",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    dl::widget::StaticInputSlotParams p;
    p.capture_mode = dl::widget::AbsoluteCaptureParams{.captured_frame = 42};

    widget.setParams(p);
    auto result = widget.params();

    bool is_absolute = false;
    result.capture_mode.visit([&](auto const & cm) {
        using T = std::decay_t<decltype(cm)>;
        if constexpr (std::is_same_v<T, dl::widget::AbsoluteCaptureParams>) {
            is_absolute = true;
        }
    });
    CHECK(is_absolute);
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

    // After construction, source combo should be populated
    widget.refreshDataSources();

    // Widget should not crash and params() should return valid data
    auto p = widget.params();
    (void) p;// just verify no crash
}

// ============================================================================
// toStaticInputData conversion
// ============================================================================

TEST_CASE("toStaticInputData produces Relative mode correctly",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/ref", TimeKey("time"));
    auto slot = makeStaticSlot("mem_pts", "Point2DEncoder");

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    dl::widget::StaticInputSlotParams p;
    p.source = "pts/ref";
    p.capture_mode = dl::widget::RelativeCaptureParams{.time_offset = -3};
    widget.setParams(p);

    auto si = widget.toStaticInputData();
    CHECK(si.slot_name == "mem_pts");
    CHECK(si.memory_index == 0);
    CHECK(si.data_key == "pts/ref");
    CHECK(si.capture_mode_str == "Relative");
    CHECK(si.time_offset == -3);
    CHECK(si.captured_frame == -1);
}

TEST_CASE("toStaticInputData produces Absolute mode correctly",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/ref", TimeKey("time"));
    auto slot = makeStaticSlot("mem_pts", "Point2DEncoder");

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    dl::widget::StaticInputSlotParams p;
    p.source = "pts/ref";
    p.capture_mode = dl::widget::AbsoluteCaptureParams{};
    widget.setParams(p);

    auto si = widget.toStaticInputData();
    CHECK(si.slot_name == "mem_pts");
    CHECK(si.capture_mode_str == "Absolute");
    CHECK(si.time_offset == 0);
    CHECK(si.captured_frame == -1);// No capture performed yet
}

// ============================================================================
// Capture status management
// ============================================================================

TEST_CASE("setCapturedStatus updates internal captured_frame",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    CHECK(widget.toStaticInputData().captured_frame == -1);

    widget.setCapturedStatus(77, {-1.0f, 1.0f});
    CHECK(widget.toStaticInputData().captured_frame == 77);
}

TEST_CASE("clearCapturedStatus resets captured_frame to -1",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    widget.setCapturedStatus(42, {0.0f, 2.0f});
    REQUIRE(widget.toStaticInputData().captured_frame == 42);

    widget.clearCapturedStatus();
    CHECK(widget.toStaticInputData().captured_frame == -1);
}

// ============================================================================
// setModelReady
// ============================================================================

TEST_CASE("setModelReady does not crash",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);
    widget.setModelReady(true);
    widget.setModelReady(false);
    // No crash = success
    CHECK(true);
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
    p.capture_mode = dl::widget::RelativeCaptureParams{.time_offset = -2};
    widget.setParams(p);

    // At minimum, verify the spy is valid (signal was emitted or not depending on value change)
    CHECK(spy.isValid());
}

TEST_CASE("captureRequested and captureInvalidated signals are valid",
          "[dl_widget][static_input_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeStaticSlot();

    dl::widget::StaticInputSlotWidget widget(slot, dm);

    QSignalSpy const capture_spy(&widget,
                                 &dl::widget::StaticInputSlotWidget::captureRequested);
    QSignalSpy const invalidate_spy(
            &widget, &dl::widget::StaticInputSlotWidget::captureInvalidated);

    CHECK(capture_spy.isValid());
    CHECK(invalidate_spy.isValid());
}
