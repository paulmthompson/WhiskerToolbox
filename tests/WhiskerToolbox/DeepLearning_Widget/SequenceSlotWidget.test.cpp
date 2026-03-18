/// @file SequenceSlotWidget.test.cpp
/// @brief Tests for the SequenceSlotWidget.
///
/// Verifies construction, entry add/remove, getStaticInputs/getRecurrentBindings,
/// data source refresh, and signal validity.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/SequenceSlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
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
            new QApplication(argc, const_cast<char **>(argv));
        }
    }
};
QtAppGuard const s_guard;
}// namespace

/// Helper: create a minimal TensorSlotDescriptor for sequence input testing.
static dl::TensorSlotDescriptor makeSequenceSlot(
        std::string name = "memory_sequence",
        std::string recommended_encoder = "Point2DEncoder") {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    slot.shape = {4, 64, 64};
    slot.sequence_dim = 0;
    slot.description = "Test sequence input slot";
    slot.recommended_encoder = std::move(recommended_encoder);
    slot.is_static = true;
    return slot;
}

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("SequenceSlotWidget constructs with valid slot and DM",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs = {"mask_out", "points_out"};

    auto widget =
            std::make_unique<dl::widget::SequenceSlotWidget>(slot, dm, outputs);
    CHECK(widget->slotName() == "memory_sequence");
}

TEST_CASE("SequenceSlotWidget starts with one entry row",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    auto static_inputs = widget.getStaticInputs();
    auto recurrent = widget.getRecurrentBindings();
    CHECK(static_inputs.size() <= 1);
    CHECK(recurrent.size() <= 1);
}

// ============================================================================
// getStaticInputs / getRecurrentBindings
// ============================================================================

TEST_CASE("getStaticInputs returns empty when no static entries configured",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    auto inputs = widget.getStaticInputs();
    CHECK(inputs.empty());
}

TEST_CASE("getRecurrentBindings returns empty when no recurrent entries",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs = {"mask_out"};

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    auto bindings = widget.getRecurrentBindings();
    CHECK(bindings.empty());
}

// ============================================================================
// setEntriesFromState
// ============================================================================

TEST_CASE("setEntriesFromState restores static input",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/ref", TimeKey("time"));
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);

    std::vector<StaticInputData> static_inputs;
    StaticInputData si;
    si.slot_name = "memory_sequence";
    si.memory_index = 0;
    si.data_key = "pts/ref";
    si.capture_mode_str = "Relative";
    si.time_offset = -2;
    si.active = true;
    static_inputs.push_back(si);

    widget.setEntriesFromState(static_inputs, {});

    auto result = widget.getStaticInputs();
    REQUIRE(result.size() == 1);
    CHECK(result[0].slot_name == "memory_sequence");
    CHECK(result[0].memory_index == 0);
    CHECK(result[0].data_key == "pts/ref");
    CHECK(result[0].capture_mode_str == "Relative");
    CHECK(result[0].time_offset == -2);
}

TEST_CASE("setEntriesFromState restores recurrent binding",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs = {"mask_out", "points_out"};

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);

    std::vector<RecurrentBindingData> recurrent;
    RecurrentBindingData rb;
    rb.input_slot_name = "memory_sequence";
    rb.output_slot_name = "mask_out";
    rb.target_memory_index = 0;
    rb.init_mode_str = "Zeros";
    recurrent.push_back(rb);

    widget.setEntriesFromState({}, recurrent);

    auto result = widget.getRecurrentBindings();
    REQUIRE(result.size() == 1);
    CHECK(result[0].input_slot_name == "memory_sequence");
    CHECK(result[0].output_slot_name == "mask_out");
    CHECK(result[0].target_memory_index == 0);
    CHECK(result[0].init_mode_str == "Zeros");
}

// ============================================================================
// Capture status
// ============================================================================

TEST_CASE("setCapturedStatus and clearCapturedStatus do not crash",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    widget.setCapturedStatus(0, 42, {0.0f, 1.0f});
    widget.clearCapturedStatus(0);
    CHECK(true);
}

// ============================================================================
// refreshDataSources / setModelReady
// ============================================================================

TEST_CASE("refreshDataSources with point encoder does not crash",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/a", TimeKey("time"));
    auto slot = makeSequenceSlot("seq_pts", "Point2DEncoder");
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    widget.refreshDataSources();
    CHECK(true);
}

TEST_CASE("setModelReady does not crash",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);
    widget.setModelReady(true);
    widget.setModelReady(false);
    CHECK(true);
}

// ============================================================================
// Signal validity
// ============================================================================

TEST_CASE("SequenceSlotWidget bindingChanged signal is valid",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);

    QSignalSpy const spy(&widget,
                         &dl::widget::SequenceSlotWidget::bindingChanged);
    CHECK(spy.isValid());
}

TEST_CASE("SequenceSlotWidget captureRequested and captureInvalidated valid",
          "[dl_widget][sequence_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto slot = makeSequenceSlot();
    std::vector<std::string> outputs;

    dl::widget::SequenceSlotWidget widget(slot, dm, outputs);

    QSignalSpy const capture_spy(
            &widget,
            &dl::widget::SequenceSlotWidget::captureRequested);
    QSignalSpy const invalidate_spy(
            &widget,
            &dl::widget::SequenceSlotWidget::captureInvalidated);

    CHECK(capture_spy.isValid());
    CHECK(invalidate_spy.isValid());
}
