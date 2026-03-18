/// @file EncoderShapeWidget.test.cpp
/// @brief Tests for the EncoderShapeWidget.
///
/// Verifies construction, parameter get/set, paramsFromState restore,
/// applyEncoderShapeToAssembler, and signal emission.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/EncoderShapeWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

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

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("EncoderShapeWidget constructs with valid state and assembler",
          "[dl_widget][encoder_shape_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    auto widget = std::make_unique<dl::widget::EncoderShapeWidget>(
            state, assembler.get());
    auto p = widget->params();
    CHECK(p.input_height == 224);
    CHECK(p.input_width == 224);
}

TEST_CASE("EncoderShapeWidget constructs with null assembler",
          "[dl_widget][encoder_shape_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto widget = std::make_unique<dl::widget::EncoderShapeWidget>(
            state, nullptr);
    // Should not crash; applyFromState is safe with null assembler
    widget->applyFromState();
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("EncoderShapeWidget setParams and params round-trip",
          "[dl_widget][encoder_shape_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::EncoderShapeWidget widget(state, assembler.get());

    dl::widget::EncoderShapeParams p;
    p.input_height = 256;
    p.input_width = 384;
    p.output_shape = "768,16,16";
    widget.setParams(p);

    auto result = widget.params();
    CHECK(result.input_height == 256);
    CHECK(result.input_width == 384);
    CHECK(result.output_shape == "768,16,16");
}

// ============================================================================
// paramsFromState / state restore
// ============================================================================

TEST_CASE("paramsFromState restores default when zeros",
          "[dl_widget][encoder_shape_widget]") {
    auto params = dl::widget::EncoderShapeWidget::paramsFromState(0, 0, "");
    CHECK(params.input_height == 224);
    CHECK(params.input_width == 224);
    CHECK(params.output_shape.empty());
}

TEST_CASE("paramsFromState restores saved values",
          "[dl_widget][encoder_shape_widget]") {
    auto params = dl::widget::EncoderShapeWidget::paramsFromState(
            512, 512, "384,7,7");
    CHECK(params.input_height == 512);
    CHECK(params.input_width == 512);
    CHECK(params.output_shape == "384,7,7");
}

// ============================================================================
// applyEncoderShapeToAssembler (static)
// ============================================================================

TEST_CASE("applyEncoderShapeToAssembler with null assembler does not crash",
          "[dl_widget][encoder_shape_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    state->setEncoderInputHeight(256);
    state->setEncoderInputWidth(256);
    state->setEncoderOutputShape("192,16,16");

    dl::widget::EncoderShapeWidget::applyEncoderShapeToAssembler(
            state.get(), nullptr);
}

// ============================================================================
// Signal emission
// ============================================================================

TEST_CASE("EncoderShapeWidget emits bindingChanged on param changes",
          "[dl_widget][encoder_shape_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::EncoderShapeWidget widget(state, assembler.get());

    QSignalSpy const spy(&widget,
                         &dl::widget::EncoderShapeWidget::bindingChanged);

    dl::widget::EncoderShapeParams p;
    p.input_height = 128;
    p.input_width = 128;
    widget.setParams(p);

    CHECK(spy.isValid());
}
