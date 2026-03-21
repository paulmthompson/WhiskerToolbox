/// @file GeneralEncoderModel.test.cpp
/// @brief Unit tests for the GeneralEncoderModel wrapper.

#include <catch2/catch_test_macros.hpp>

#include "models_v2/TensorSlotDescriptor.hpp"
#include "models_v2/general_encoder/GeneralEncoderModel.hpp"
#include "registry/ModelRegistry.hpp"

#include <torch/torch.h>

#include <algorithm>
#include <string>
#include <vector>

// ─── Metadata Tests ─────────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - modelId", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK(model.modelId() == "general_encoder");
}

TEST_CASE("GeneralEncoderModel - displayName", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK(model.displayName() == "General Encoder");
}

TEST_CASE("GeneralEncoderModel - description is non-empty", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK_FALSE(model.description().empty());
}

// ─── Default Construction Tests ─────────────────────────────────

TEST_CASE("GeneralEncoderModel - default constructor uses 224x224 input", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK(model.inputChannels() == 3);
    CHECK(model.inputHeight() == 224);
    CHECK(model.inputWidth() == 224);
    auto const expected = std::vector<int64_t>{384, 7, 7};
    CHECK(model.outputShape() == expected);
}

// ─── Custom Construction Tests ──────────────────────────────────

TEST_CASE("GeneralEncoderModel - custom resolution constructor", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model(3, 512, 512, {768, 16, 16});
    CHECK(model.inputChannels() == 3);
    CHECK(model.inputHeight() == 512);
    CHECK(model.inputWidth() == 512);
    auto const expected = std::vector<int64_t>{768, 16, 16};
    CHECK(model.outputShape() == expected);
}

TEST_CASE("GeneralEncoderModel - single channel input", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model(1, 224, 224, {256});
    CHECK(model.inputChannels() == 1);

    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);
    auto const expected_shape = std::vector<int64_t>{1, 224, 224};
    CHECK(inputs[0].shape == expected_shape);
}

// ─── Batch Size Tests ───────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - preferredBatchSize is 0 (model decides)", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK(model.preferredBatchSize() == 0);
}

TEST_CASE("GeneralEncoderModel - maxBatchSize is 0 (unlimited)", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK(model.maxBatchSize() == 0);
}

TEST_CASE("GeneralEncoderModel - batchMode is DynamicBatch", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    auto mode = model.batchMode();
    REQUIRE(std::holds_alternative<dl::DynamicBatch>(mode));
    auto dynamic = std::get<dl::DynamicBatch>(mode);
    CHECK(dynamic.min_size == 1);
    CHECK(dynamic.max_size == 0);
}

// ─── Input Slot Tests ───────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - has 1 input slot", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);
}

TEST_CASE("GeneralEncoderModel - image input slot descriptor", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);

    auto const & slot = inputs[0];
    auto const expected_shape = std::vector<int64_t>{3, 224, 224};
    CHECK(slot.name == "image");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_encoder == "ImageEncoder");
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.dtype == dl::TensorDType::Float32);
    CHECK(slot.sequence_dim == -1);
    CHECK_FALSE(slot.description.empty());
}

TEST_CASE("GeneralEncoderModel - custom input slot reflects constructor args", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model(1, 128, 128, {512, 4, 4});
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);

    auto const expected_shape = std::vector<int64_t>{1, 128, 128};
    CHECK(inputs[0].shape == expected_shape);
}

// ─── Output Slot Tests ──────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - has 1 output slot", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
}

TEST_CASE("GeneralEncoderModel - features output slot descriptor", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);

    auto const & slot = outputs[0];
    auto const expected_shape = std::vector<int64_t>{384, 7, 7};
    CHECK(slot.name == "features");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_decoder.empty());
    CHECK(slot.is_static == false);
    CHECK(slot.dtype == dl::TensorDType::Float32);
    CHECK(slot.sequence_dim == -1);
    CHECK_FALSE(slot.description.empty());
}

TEST_CASE("GeneralEncoderModel - custom output slot reflects constructor args", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model(3, 224, 224, {1024, 14, 14});
    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);

    auto const expected_shape = std::vector<int64_t>{1024, 14, 14};
    CHECK(outputs[0].shape == expected_shape);
}

// ─── Readiness Tests ────────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - not ready before loading weights", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel const model;
    CHECK_FALSE(model.isReady());
}

TEST_CASE("GeneralEncoderModel - forward throws when not ready", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    std::unordered_map<std::string, torch::Tensor> inputs;
    inputs["image"] = torch::randn({1, 3, 224, 224});
    CHECK_THROWS_AS(model.forward(inputs), std::runtime_error);
}

TEST_CASE("GeneralEncoderModel - loadWeights throws on invalid path", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    CHECK_THROWS_AS(model.loadWeights("/nonexistent/model.pt"), std::runtime_error);
}

// ─── Move Semantics Tests ───────────────────────────────────────

TEST_CASE("GeneralEncoderModel - move constructor", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel original(3, 512, 512, {768, 16, 16});
    dl::GeneralEncoderModel const moved(std::move(original));

    CHECK(moved.modelId() == "general_encoder");
    CHECK(moved.inputChannels() == 3);
    CHECK(moved.inputHeight() == 512);
    CHECK(moved.inputWidth() == 512);
}

TEST_CASE("GeneralEncoderModel - move assignment", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel original(3, 512, 512, {768, 16, 16});
    dl::GeneralEncoderModel target;

    target = std::move(original);

    CHECK(target.inputChannels() == 3);
    CHECK(target.inputHeight() == 512);
}

// ─── Registry Tests ─────────────────────────────────────────────

TEST_CASE("GeneralEncoderModel - registered in ModelRegistry", "[GeneralEncoderModel]") {
    auto & registry = dl::ModelRegistry::instance();
    CHECK(registry.hasModel("general_encoder"));
}

TEST_CASE("GeneralEncoderModel - can be created from registry", "[GeneralEncoderModel]") {
    auto & registry = dl::ModelRegistry::instance();
    auto model = registry.create("general_encoder");
    REQUIRE(model != nullptr);
    CHECK(model->modelId() == "general_encoder");
}

TEST_CASE("GeneralEncoderModel - registry metadata matches", "[GeneralEncoderModel]") {
    auto & registry = dl::ModelRegistry::instance();
    auto info = registry.getModelInfo("general_encoder");
    REQUIRE(info.has_value());

    CHECK(info->model_id == "general_encoder");
    CHECK(info->display_name == "General Encoder");
    CHECK_FALSE(info->description.empty());
    CHECK(info->inputs.size() == 1);
    CHECK(info->outputs.size() == 1);
    CHECK(info->inputs[0].name == "image");
    CHECK(info->outputs[0].name == "features");
}

// ─── Runtime Shape Configuration Tests ──────────────────────────

TEST_CASE("GeneralEncoderModel - setInputResolution updates height and width", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    CHECK(model.inputHeight() == 224);
    CHECK(model.inputWidth() == 224);

    model.setInputResolution(256, 256);
    CHECK(model.inputHeight() == 256);
    CHECK(model.inputWidth() == 256);

    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);
    auto const expected_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(inputs[0].shape == expected_shape);
}

TEST_CASE("GeneralEncoderModel - setInputResolution with non-square", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    model.setInputResolution(480, 640);
    CHECK(model.inputHeight() == 480);
    CHECK(model.inputWidth() == 640);

    auto inputs = model.inputSlots();
    auto const expected_shape = std::vector<int64_t>{3, 480, 640};
    CHECK(inputs[0].shape == expected_shape);
}

TEST_CASE("GeneralEncoderModel - setOutputShape updates raw output", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    auto const original = std::vector<int64_t>{384, 7, 7};
    CHECK(model.outputShape() == original);

    auto const new_shape = std::vector<int64_t>{192, 16, 16};
    model.setOutputShape(new_shape);
    CHECK(model.outputShape() == new_shape);

    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
    CHECK(outputs[0].shape == new_shape);
}

TEST_CASE("GeneralEncoderModel - setOutputShape affects effectiveOutputShape with post-encoder", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model;
    model.setOutputShape({192, 16, 16});

    // Without post-encoder, effective = raw
    auto const expected_raw = std::vector<int64_t>{192, 16, 16};
    CHECK(model.effectiveOutputShape() == expected_raw);
}

TEST_CASE("GeneralEncoderModel - setInputResolution preserves channels", "[GeneralEncoderModel]") {
    dl::GeneralEncoderModel model(1, 224, 224, {256});
    CHECK(model.inputChannels() == 1);

    model.setInputResolution(512, 512);
    CHECK(model.inputChannels() == 1);
    CHECK(model.inputHeight() == 512);
    CHECK(model.inputWidth() == 512);
}
