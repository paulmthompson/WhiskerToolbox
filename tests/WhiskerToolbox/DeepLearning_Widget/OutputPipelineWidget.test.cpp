/**
 * @file OutputPipelineWidget.test.cpp
 * @brief Tests for the DeepLearning output pipeline widget.
 */

#include "DeepLearning_Widget/UI/Helpers/OutputPipelineWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("OutputPipelineWidget defaults NeuroSAM output to mask decoder",
          "[dl_widget][output_pipeline_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/out", TimeKey("time"));

    dl::TensorSlotDescriptor slot;
    slot.name = "probability_map";
    slot.shape = {1, 256, 256};
    slot.recommended_pipeline = {dl::OutputPipelineStepSpec{
            .step_id = "TensorToMask2D"}};

    dl::widget::OutputPipelineWidget widget("neurosam", slot, dm);
    auto binding = widget.toOutputBindingData();

    REQUIRE(binding.pipeline.size() == 1);
    CHECK(binding.pipeline[0].step_id == "TensorToMask2D");
}

TEST_CASE("OutputPipelineWidget defaults General Encoder to feature pipeline",
          "[dl_widget][output_pipeline_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<TensorData>("features/out", TimeKey("time"));

    dl::TensorSlotDescriptor slot;
    slot.name = "features";
    slot.shape = {384, 7, 7};
    slot.recommended_pipeline = dl::defaultOutputPipeline(
            "general_encoder", "features", slot.shape);

    dl::widget::OutputPipelineWidget widget("general_encoder", slot, dm);
    auto binding = widget.toOutputBindingData();

    REQUIRE(binding.pipeline.size() == 2);
    CHECK(binding.pipeline[0].step_id == "global_avg_pool");
    CHECK(binding.pipeline[1].step_id == "TensorToFeatureVector");
}

TEST_CASE("OutputPipelineWidget preserves typed decoder parameters",
          "[dl_widget][output_pipeline_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<MaskData>("masks/out", TimeKey("time"));

    dl::TensorSlotDescriptor slot;
    slot.name = "probability_map";
    slot.shape = {1, 256, 256};

    dl::widget::OutputPipelineWidget widget("neurosam", slot, dm);

    OutputBindingData saved;
    saved.slot_name = "probability_map";
    saved.data_key = "masks/out";
    saved.pipeline = {dl::OutputPipelineStepSpec{
            .step_id = "TensorToMask2D",
            .parameters = dl::OutputPipelineStepParameters{
                    dl::MaskDecoderParams{.threshold = 0.7f}}}};
    widget.setBinding(saved);

    auto binding = widget.toOutputBindingData();
    REQUIRE(binding.pipeline.size() == 1);
    CHECK(binding.pipeline[0].step_id == "TensorToMask2D");
    auto const params = dl::maskDecoderParamsForStep(binding.pipeline[0]);
    CHECK(params.threshold == 0.7f);
}
