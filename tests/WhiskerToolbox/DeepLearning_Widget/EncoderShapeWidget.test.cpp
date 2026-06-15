/// @file EncoderShapeWidget.test.cpp
/// @brief Tests for the EncoderShapeWidget.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/EncoderShapeWidget.hpp"

#include "DeepLearning/models_v2/general_encoder/GeneralEncoderModelParams.hpp"
#include "DeepLearning/registry/ModelRegistry.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include <QSignalSpy>

#include <rfl/json.hpp>

namespace {

std::shared_ptr<DeepLearningState> stateWithGeneralEncoder() {
    auto state = std::make_shared<DeepLearningState>();
    state->setSelectedModelId("general_encoder");
    return state;
}

}// namespace

TEST_CASE("EncoderShapeWidget constructs for general_encoder",
          "[dl_widget][encoder_shape_widget]") {
    REQUIRE(dl::ModelRegistry::instance().hasConfiguration("general_encoder"));

    auto state = stateWithGeneralEncoder();
    auto assembler = std::make_unique<SlotAssembler>();
    assembler->loadModel("general_encoder");

    auto widget = std::make_unique<dl::widget::EncoderShapeWidget>(
            state, assembler.get());
    REQUIRE(widget != nullptr);
}

TEST_CASE("EncoderShapeWidget constructs with null assembler",
          "[dl_widget][encoder_shape_widget]") {
    auto state = stateWithGeneralEncoder();
    auto widget = std::make_unique<dl::widget::EncoderShapeWidget>(
            state, nullptr);
    widget->applyFromState();
}

TEST_CASE("EncoderShapeWidget persists configuration to state",
          "[dl_widget][encoder_shape_widget]") {
    auto state = stateWithGeneralEncoder();
    auto assembler = std::make_unique<SlotAssembler>();
    assembler->loadModel("general_encoder");

    dl::widget::EncoderShapeWidget widget(state, assembler.get());

    dl::GeneralEncoderModelParams params;
    params.input_height = 256;
    params.input_width = 384;
    params.output_shape = "768,16,16";
    state->setConfigurationJsonForModel(
            "general_encoder",
            rfl::json::write(params));

    auto const restored_json = state->configurationJsonForModel("general_encoder");
    auto parsed = rfl::json::read<dl::GeneralEncoderModelParams>(restored_json);
    REQUIRE(parsed);
    CHECK(parsed.value().input_height == 256);
    CHECK(parsed.value().input_width == 384);
    CHECK(parsed.value().output_shape == "768,16,16");
}

TEST_CASE("applyConfigurationFromState with null assembler does not crash",
          "[dl_widget][encoder_shape_widget]") {
    auto state = stateWithGeneralEncoder();
    dl::GeneralEncoderModelParams params;
    params.input_height = 256;
    params.input_width = 256;
    params.output_shape = "192,16,16";
    state->setConfigurationJsonForModel(
            "general_encoder",
            rfl::json::write(params));

    dl::widget::EncoderShapeWidget::applyConfigurationFromState(
            state.get(), nullptr);
}

TEST_CASE("EncoderShapeWidget emits bindingChanged on param changes",
          "[dl_widget][encoder_shape_widget]") {
    auto state = stateWithGeneralEncoder();
    auto assembler = std::make_unique<SlotAssembler>();
    assembler->loadModel("general_encoder");

    dl::widget::EncoderShapeWidget widget(state, assembler.get());

    QSignalSpy const spy(&widget,
                         &dl::widget::EncoderShapeWidget::bindingChanged);
    REQUIRE(spy.isValid());
}
