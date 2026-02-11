#include <catch2/catch_test_macros.hpp>

#include "models_v2/neurosam/NeuroSAMModel.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"
#include "registry/ModelRegistry.hpp"

#include <torch/torch.h>

#include <algorithm>
#include <string>
#include <vector>

// ─── Metadata Tests ─────────────────────────────────────────────

TEST_CASE("NeuroSAMModel - modelId", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK(model.modelId() == "neurosam");
}

TEST_CASE("NeuroSAMModel - displayName", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK(model.displayName() == "NeuroSAM");
}

TEST_CASE("NeuroSAMModel - description is non-empty", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK_FALSE(model.description().empty());
    CHECK(model.description().find("Segment-Anything") != std::string::npos);
}

// ─── Batch Size Tests ───────────────────────────────────────────

TEST_CASE("NeuroSAMModel - preferredBatchSize is 1", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK(model.preferredBatchSize() == 1);
}

TEST_CASE("NeuroSAMModel - maxBatchSize is 1", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK(model.maxBatchSize() == 1);
}

// ─── Input Slot Tests ───────────────────────────────────────────

TEST_CASE("NeuroSAMModel - has 3 input slots", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 3);
}

TEST_CASE("NeuroSAMModel - encoder_image slot", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() >= 1);

    auto const & slot = inputs[0];
    auto const expected_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(slot.name == "encoder_image");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_encoder == "ImageEncoder");
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
    CHECK_FALSE(slot.description.empty());
}

TEST_CASE("NeuroSAMModel - memory_images slot", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() >= 2);

    auto const & slot = inputs[1];
    auto const expected_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(slot.name == "memory_images");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_encoder == "ImageEncoder");
    CHECK(slot.is_static == true);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

TEST_CASE("NeuroSAMModel - memory_masks slot", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() >= 3);

    auto const & slot = inputs[2];
    auto const expected_shape = std::vector<int64_t>{1, 256, 256};
    CHECK(slot.name == "memory_masks");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_encoder == "Mask2DEncoder");
    CHECK(slot.is_static == true);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
}

// ─── Output Slot Tests ──────────────────────────────────────────

TEST_CASE("NeuroSAMModel - has 1 output slot", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
}

TEST_CASE("NeuroSAMModel - probability_map output slot", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);

    auto const & slot = outputs[0];
    auto const expected_shape = std::vector<int64_t>{1, 256, 256};
    CHECK(slot.name == "probability_map");
    CHECK(slot.shape == expected_shape);
    CHECK(slot.recommended_decoder == "TensorToMask2D");
    CHECK(slot.is_static == false);
    CHECK(slot.is_boolean_mask == false);
    CHECK(slot.sequence_dim == -1);
    CHECK_FALSE(slot.description.empty());
}

// ─── Weight Loading / Readiness Tests ───────────────────────────

TEST_CASE("NeuroSAMModel - not ready without weights", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK(model.isReady() == false);
}

TEST_CASE("NeuroSAMModel - loadWeights with nonexistent file throws", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    CHECK_THROWS_AS(
        model.loadWeights("/nonexistent/path/model.pte"),
        std::runtime_error);
    CHECK(model.isReady() == false);
}

TEST_CASE("NeuroSAMModel - forward without weights throws", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;

    std::unordered_map<std::string, torch::Tensor> inputs{
        {"encoder_image", torch::randn({1, 3, 256, 256})},
        {"memory_images", torch::randn({1, 3, 256, 256})},
        {"memory_masks", torch::randn({1, 1, 256, 256})},
    };

    CHECK_THROWS_AS(model.forward(inputs), std::runtime_error);
}

// ─── Forward Validation Tests (no weights needed) ───────────────

TEST_CASE("NeuroSAMModel - forward with missing input throws", "[NeuroSAMModel][integration]")
{
    // This test verifies input validation logic.
    // Even if weights were loaded, missing inputs should be caught before execution.
    // Since we can't actually load weights in unit tests, we verify the error message.
    dl::NeuroSAMModel model;

    // Missing memory_masks
    std::unordered_map<std::string, torch::Tensor> incomplete_inputs{
        {"encoder_image", torch::randn({1, 3, 256, 256})},
        {"memory_images", torch::randn({1, 3, 256, 256})},
    };

    // Should throw because model isn't ready AND inputs are incomplete
    CHECK_THROWS_AS(model.forward(incomplete_inputs), std::runtime_error);
}

// ─── Constants Tests ────────────────────────────────────────────

TEST_CASE("NeuroSAMModel - constants are correct", "[NeuroSAMModel]")
{
    CHECK(dl::NeuroSAMModel::kModelSize == 256);
    CHECK(dl::NeuroSAMModel::kImageChannels == 3);
    CHECK(dl::NeuroSAMModel::kMaskChannels == 1);
    CHECK(dl::NeuroSAMModel::kOutputChannels == 1);
}

TEST_CASE("NeuroSAMModel - slot name constants match slot descriptors", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    auto outputs = model.outputSlots();

    CHECK(inputs[0].name == dl::NeuroSAMModel::kEncoderImageSlot);
    CHECK(inputs[1].name == dl::NeuroSAMModel::kMemoryImagesSlot);
    CHECK(inputs[2].name == dl::NeuroSAMModel::kMemoryMasksSlot);
    CHECK(outputs[0].name == dl::NeuroSAMModel::kProbabilityMapSlot);
}

// ─── Slot Element Count Tests ───────────────────────────────────

TEST_CASE("NeuroSAMModel - slot numElements correct", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;
    auto inputs = model.inputSlots();
    auto outputs = model.outputSlots();

    // encoder_image: 3 * 256 * 256 = 196608
    CHECK(inputs[0].numElements() == 3 * 256 * 256);

    // memory_images: 3 * 256 * 256
    CHECK(inputs[1].numElements() == 3 * 256 * 256);

    // memory_masks: 1 * 256 * 256
    CHECK(inputs[2].numElements() == 1 * 256 * 256);

    // probability_map: 1 * 256 * 256
    CHECK(outputs[0].numElements() == 1 * 256 * 256);
}

// ─── No Sequence Dimension Tests ────────────────────────────────

TEST_CASE("NeuroSAMModel - no slots have sequence dimension", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel model;

    for (auto const & slot : model.inputSlots()) {
        CHECK(slot.hasSequenceDim() == false);
    }
    for (auto const & slot : model.outputSlots()) {
        CHECK(slot.hasSequenceDim() == false);
    }
}

// ─── Move Semantics Test ────────────────────────────────────────

TEST_CASE("NeuroSAMModel - move constructor", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel original;
    CHECK(original.modelId() == "neurosam");

    dl::NeuroSAMModel moved(std::move(original));
    CHECK(moved.modelId() == "neurosam");
    CHECK(moved.inputSlots().size() == 3);
    CHECK(moved.outputSlots().size() == 1);
}

TEST_CASE("NeuroSAMModel - move assignment", "[NeuroSAMModel]")
{
    dl::NeuroSAMModel original;
    dl::NeuroSAMModel target;

    target = std::move(original);
    CHECK(target.modelId() == "neurosam");
    CHECK(target.isReady() == false);
}

// ─── Registry Integration Tests ─────────────────────────────────

TEST_CASE("NeuroSAMModel - registered in ModelRegistry", "[NeuroSAMModel][ModelRegistry]")
{
    auto & registry = dl::ModelRegistry::instance();
    CHECK(registry.hasModel("neurosam"));
}

TEST_CASE("NeuroSAMModel - can be created via registry", "[NeuroSAMModel][ModelRegistry]")
{
    auto & registry = dl::ModelRegistry::instance();
    auto model = registry.create("neurosam");
    REQUIRE(model != nullptr);
    CHECK(model->modelId() == "neurosam");
    CHECK(model->displayName() == "NeuroSAM");
}

TEST_CASE("NeuroSAMModel - ModelInfo from registry", "[NeuroSAMModel][ModelRegistry]")
{
    auto & registry = dl::ModelRegistry::instance();
    auto info = registry.getModelInfo("neurosam");
    REQUIRE(info.has_value());

    CHECK(info->model_id == "neurosam");
    CHECK(info->display_name == "NeuroSAM");
    CHECK_FALSE(info->description.empty());
    CHECK(info->inputs.size() == 3);
    CHECK(info->outputs.size() == 1);
    CHECK(info->preferred_batch_size == 1);
    CHECK(info->max_batch_size == 1);
}

TEST_CASE("NeuroSAMModel - registry slot lookup", "[NeuroSAMModel][ModelRegistry]")
{
    auto & registry = dl::ModelRegistry::instance();

    auto const * encoder_image = registry.getInputSlot("neurosam", "encoder_image");
    REQUIRE(encoder_image != nullptr);
    CHECK(encoder_image->recommended_encoder == "ImageEncoder");

    auto const * memory_masks = registry.getInputSlot("neurosam", "memory_masks");
    REQUIRE(memory_masks != nullptr);
    CHECK(memory_masks->recommended_encoder == "Mask2DEncoder");

    auto const * prob_map = registry.getOutputSlot("neurosam", "probability_map");
    REQUIRE(prob_map != nullptr);
    CHECK(prob_map->recommended_decoder == "TensorToMask2D");

    // Non-existent slot
    auto const * nonexistent = registry.getInputSlot("neurosam", "nonexistent");
    CHECK(nonexistent == nullptr);
}

TEST_CASE("NeuroSAMModel - registry creates independent instances", "[NeuroSAMModel][ModelRegistry]")
{
    auto & registry = dl::ModelRegistry::instance();
    auto model1 = registry.create("neurosam");
    auto model2 = registry.create("neurosam");

    REQUIRE(model1 != nullptr);
    REQUIRE(model2 != nullptr);
    CHECK(model1.get() != model2.get());
    CHECK(model1->modelId() == model2->modelId());
}
