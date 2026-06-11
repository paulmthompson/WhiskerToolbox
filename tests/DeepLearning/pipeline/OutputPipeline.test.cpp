#include "pipeline/OutputPipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <ranges>

TEST_CASE("OutputPipeline validates NeuroSAM mask default", "[OutputPipeline]") {
    auto pipeline = dl::defaultOutputPipeline(
            "neurosam", "probability_map", {1, 256, 256});

    REQUIRE(pipeline.size() == 1);
    CHECK(pipeline[0].step_id == "TensorToMask2D");

    auto validation = dl::validateOutputPipeline({1, 256, 256}, pipeline);
    CHECK(validation.valid);
    CHECK(validation.output_data_type == "MaskData");
}

TEST_CASE("OutputPipeline defaults General Encoder to terminated feature chain",
          "[OutputPipeline]") {
    auto pipeline = dl::defaultOutputPipeline(
            "general_encoder", "features", {384, 7, 7});

    REQUIRE(pipeline.size() == 2);
    CHECK(pipeline[0].step_id == "global_avg_pool");
    CHECK(pipeline[1].step_id == "TensorToFeatureVector");

    auto validation = dl::validateOutputPipeline({384, 7, 7}, pipeline);
    CHECK(validation.valid);
    CHECK(validation.final_tensor_shape == std::vector<int64_t>{384});
    CHECK(validation.output_data_type == "TensorData");
}

TEST_CASE("OutputPipeline rejects unterminated chain", "[OutputPipeline]") {
    std::vector<dl::OutputPipelineStepSpec> pipeline{
            dl::OutputPipelineStepSpec{.step_id = "global_avg_pool"}};

    auto validation = dl::validateOutputPipeline({384, 7, 7}, pipeline);
    CHECK_FALSE(validation.valid);
    CHECK_FALSE(validation.message.empty());
}

TEST_CASE("OutputPipeline filters next steps by propagated rank",
          "[OutputPipeline]") {
    auto spatial_steps = dl::validNextPipelineStepIds({1, 256, 256});
    CHECK(std::ranges::find(spatial_steps, "TensorToMask2D") != spatial_steps.end());
    CHECK(std::ranges::find(spatial_steps, "global_avg_pool") != spatial_steps.end());

    auto vector_steps = dl::validNextPipelineStepIds({384});
    CHECK(std::ranges::find(vector_steps, "TensorToFeatureVector") !=
          vector_steps.end());
    CHECK(std::ranges::find(vector_steps, "TensorToMask2D") == vector_steps.end());
}

TEST_CASE("OutputPipeline rejects mismatched step parameters", "[OutputPipeline]") {
    std::vector<dl::OutputPipelineStepSpec> pipeline{
            dl::OutputPipelineStepSpec{
                    .step_id = "TensorToMask2D",
                    .parameters = dl::OutputPipelineStepParameters{
                            dl::PointDecoderParams{}}}};

    auto validation = dl::validateOutputPipeline({1, 256, 256}, pipeline);
    CHECK_FALSE(validation.valid);
    CHECK_FALSE(validation.message.empty());
}
