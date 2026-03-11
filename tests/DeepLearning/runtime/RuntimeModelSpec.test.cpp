#include "runtime/RuntimeModelSpec.hpp"
#include "registry/ModelRegistry.hpp"
#include "runtime/RuntimeModel.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace dl;
using Catch::Matchers::ContainsSubstring;

// Helper to create a SlotSpec with only name and shape
static SlotSpec makeSlot(std::string name, std::vector<int64_t> shape) {
    SlotSpec s;
    s.name = std::move(name);
    s.shape = std::move(shape);
    return s;
}

static SlotSpec makeSlotWithSeqDim(std::string name, std::vector<int64_t> shape, int seq_dim) {
    SlotSpec s;
    s.name = std::move(name);
    s.shape = std::move(shape);
    s.sequence_dim = seq_dim;
    return s;
}

/// Extract error message from an rfl::Result
template<typename T>
static std::string errorMsg(rfl::Result<T> const & result) {
    return std::string(result.error()->what());
}

// ──────────────────────────────────────────────────────────────
// Helper: canonical JSON for the NeuroSAM-like model spec
// ──────────────────────────────────────────────────────────────
static std::string const kFullJson = R"({
    "model_id": "test_model",
    "display_name": "Test Model",
    "description": "A test model for unit tests",
    "weights_path": "/tmp/nonexistent.pte",
    "preferred_batch_size": 1,
    "max_batch_size": 4,
    "inputs": [
        {
            "name": "image",
            "shape": [3, 256, 256],
            "description": "Current video frame",
            "recommended_encoder": "ImageEncoder",
            "is_static": false,
            "is_boolean_mask": false,
            "sequence_dim": -1
        },
        {
            "name": "memory_images",
            "shape": [4, 3, 256, 256],
            "description": "Memory encoder frames",
            "recommended_encoder": "ImageEncoder",
            "is_static": true,
            "is_boolean_mask": false,
            "sequence_dim": 0
        },
        {
            "name": "memory_mask",
            "shape": [4],
            "description": "Memory slot active flags",
            "is_static": true,
            "is_boolean_mask": true,
            "sequence_dim": -1
        }
    ],
    "outputs": [
        {
            "name": "heatmap",
            "shape": [1, 256, 256],
            "description": "Probability map",
            "recommended_decoder": "TensorToMask2D"
        }
    ]
})";

// Minimal JSON: only required fields
static std::string const kMinimalJson = R"({
    "model_id": "minimal_model",
    "display_name": "Minimal",
    "inputs": [
        { "name": "x", "shape": [3, 64, 64] }
    ],
    "outputs": [
        { "name": "y", "shape": [1, 64, 64] }
    ]
})";

// ═══════════════════════════════════════════════════════════════
// SlotSpec → TensorSlotDescriptor
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SlotSpec - toDescriptor with all fields", "[runtime]") {
    SlotSpec slot;
    slot.name = "image";
    slot.shape = {3, 256, 256};
    slot.description = "Current frame";
    slot.recommended_encoder = "ImageEncoder";
    slot.recommended_decoder = "TensorToMask2D";
    slot.is_static = true;
    slot.is_boolean_mask = true;
    slot.sequence_dim = 0;

    auto desc = slot.toDescriptor();

    CHECK(desc.name == "image");
    auto const expected_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(desc.shape == expected_shape);
    CHECK(desc.description == "Current frame");
    CHECK(desc.recommended_encoder == "ImageEncoder");
    CHECK(desc.recommended_decoder == "TensorToMask2D");
    CHECK(desc.is_static == true);
    CHECK(desc.is_boolean_mask == true);
    CHECK(desc.sequence_dim == 0);
}

TEST_CASE("SlotSpec - toDescriptor applies defaults for omitted fields", "[runtime]") {
    SlotSpec slot;
    slot.name = "x";
    slot.shape = {1, 64, 64};
    // All optional fields left as std::nullopt

    auto desc = slot.toDescriptor();

    CHECK(desc.name == "x");
    auto const expected_shape = std::vector<int64_t>{1, 64, 64};
    CHECK(desc.shape == expected_shape);
    CHECK(desc.description.empty());
    CHECK(desc.recommended_encoder.empty());
    CHECK(desc.recommended_decoder.empty());
    CHECK(desc.is_static == false);
    CHECK(desc.is_boolean_mask == false);
    CHECK(desc.sequence_dim == -1);
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModelSpec - JSON parsing
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - parse full JSON", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    auto const & spec = result.value();
    CHECK(spec.model_id == "test_model");
    CHECK(spec.display_name == "Test Model");
    CHECK(spec.description.value() == "A test model for unit tests");
    CHECK(spec.weights_path.value() == "/tmp/nonexistent.pte");
    CHECK(spec.preferred_batch_size.value() == 1);
    CHECK(spec.max_batch_size.value() == 4);

    REQUIRE(spec.inputs.size() == 3);
    CHECK(spec.inputs[0].name == "image");
    auto const expected_image_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(spec.inputs[0].shape == expected_image_shape);
    CHECK(spec.inputs[0].recommended_encoder.value() == "ImageEncoder");
    CHECK(spec.inputs[0].is_static.value() == false);
    CHECK(spec.inputs[1].name == "memory_images");
    CHECK(spec.inputs[1].is_static.value() == true);
    CHECK(spec.inputs[1].sequence_dim.value() == 0);
    CHECK(spec.inputs[2].name == "memory_mask");
    CHECK(spec.inputs[2].is_boolean_mask.value() == true);

    REQUIRE(spec.outputs.size() == 1);
    CHECK(spec.outputs[0].name == "heatmap");
    auto const expected_heatmap_shape = std::vector<int64_t>{1, 256, 256};
    CHECK(spec.outputs[0].shape == expected_heatmap_shape);
    CHECK(spec.outputs[0].recommended_decoder.value() == "TensorToMask2D");
}

TEST_CASE("RuntimeModelSpec - parse minimal JSON", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    auto const & spec = result.value();
    CHECK(spec.model_id == "minimal_model");
    CHECK(spec.display_name == "Minimal");
    CHECK_FALSE(spec.description.has_value());
    CHECK_FALSE(spec.weights_path.has_value());
    CHECK_FALSE(spec.preferred_batch_size.has_value());
    CHECK_FALSE(spec.max_batch_size.has_value());

    REQUIRE(spec.inputs.size() == 1);
    CHECK(spec.inputs[0].name == "x");
    CHECK_FALSE(spec.inputs[0].recommended_encoder.has_value());
    CHECK_FALSE(spec.inputs[0].is_static.has_value());

    REQUIRE(spec.outputs.size() == 1);
    CHECK(spec.outputs[0].name == "y");
}

TEST_CASE("RuntimeModelSpec - parse invalid JSON returns error", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson("not valid json{{{");
    CHECK_FALSE(static_cast<bool>(result));
}

TEST_CASE("RuntimeModelSpec - parse JSON missing required field", "[runtime]") {
    // Missing model_id
    std::string const json = R"({
        "display_name": "Oops",
        "inputs": [],
        "outputs": []
    })";
    auto result = RuntimeModelSpec::fromJson(json);
    CHECK_FALSE(static_cast<bool>(result));
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModelSpec - round-trip
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - toJson produces parseable JSON", "[runtime]") {
    auto result1 = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result1));

    auto json_str = result1.value().toJson();
    CHECK_FALSE(json_str.empty());

    // Parse back
    auto result2 = RuntimeModelSpec::fromJson(json_str);
    REQUIRE(static_cast<bool>(result2));

    auto const & spec1 = result1.value();
    auto const & spec2 = result2.value();
    CHECK(spec1.model_id == spec2.model_id);
    CHECK(spec1.display_name == spec2.display_name);
    CHECK(spec1.description == spec2.description);
    CHECK(spec1.preferred_batch_size == spec2.preferred_batch_size);
    CHECK(spec1.max_batch_size == spec2.max_batch_size);
    CHECK(spec1.inputs.size() == spec2.inputs.size());
    CHECK(spec1.outputs.size() == spec2.outputs.size());

    for (std::size_t i = 0; i < spec1.inputs.size(); ++i) {
        CHECK(spec1.inputs[i].name == spec2.inputs[i].name);
        CHECK(spec1.inputs[i].shape == spec2.inputs[i].shape);
    }
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModelSpec - descriptors
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - inputDescriptors and outputDescriptors", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    auto const & spec = result.value();

    auto in_descs = spec.inputDescriptors();
    REQUIRE(in_descs.size() == 3);
    CHECK(in_descs[0].name == "image");
    CHECK(in_descs[0].recommended_encoder == "ImageEncoder");
    CHECK(in_descs[0].is_static == false);
    CHECK(in_descs[1].name == "memory_images");
    CHECK(in_descs[1].is_static == true);
    CHECK(in_descs[1].hasSequenceDim());
    CHECK(in_descs[1].sequence_dim == 0);
    CHECK(in_descs[2].name == "memory_mask");
    CHECK(in_descs[2].is_boolean_mask == true);

    auto out_descs = spec.outputDescriptors();
    REQUIRE(out_descs.size() == 1);
    CHECK(out_descs[0].name == "heatmap");
    CHECK(out_descs[0].recommended_decoder == "TensorToMask2D");
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModelSpec - validate()
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - validate passes on valid spec", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    auto errors = result.value().validate();
    CHECK(errors.empty());
}

TEST_CASE("RuntimeModelSpec - validate catches empty model_id", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "";
    spec.display_name = "Name";
    spec.inputs = {makeSlot("x", {1})};
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("model_id"));
}

TEST_CASE("RuntimeModelSpec - validate catches empty display_name", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "";
    spec.inputs = {makeSlot("x", {1})};
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("display_name"));
}

TEST_CASE("RuntimeModelSpec - validate catches duplicate input names", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "OK";
    spec.inputs = {
            makeSlot("x", {1}),
            makeSlot("x", {2}),
    };
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("duplicate"));
}

TEST_CASE("RuntimeModelSpec - validate catches sequence_dim out of bounds", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "OK";
    spec.inputs = {
            makeSlotWithSeqDim("x", {3, 256, 256}, 5),
    };
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("sequence_dim"));
}

TEST_CASE("RuntimeModelSpec - validate catches empty shape", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "OK";
    spec.inputs = {makeSlot("x", {})};
    spec.outputs = {};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("shape"));
}

TEST_CASE("RuntimeModelSpec - validate catches negative batch sizes", "[runtime]") {
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "OK";
    spec.inputs = {makeSlot("x", {1})};
    spec.outputs = {makeSlot("y", {1})};
    spec.preferred_batch_size = -1;

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("preferred_batch_size"));
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModelSpec - file loading
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - fromJsonFile with relative weights_path", "[runtime]") {
    auto const tmp_dir = std::filesystem::temp_directory_path() / "dl_test_runtime";
    std::filesystem::create_directories(tmp_dir);
    auto const json_path = tmp_dir / "model_spec.json";

    std::string const json = R"({
        "model_id": "file_test",
        "display_name": "File Test",
        "weights_path": "weights/model.pte",
        "inputs": [{ "name": "x", "shape": [1] }],
        "outputs": [{ "name": "y", "shape": [1] }]
    })";

    {
        std::ofstream ofs(json_path);
        ofs << json;
    }

    auto result = RuntimeModelSpec::fromJsonFile(json_path);
    REQUIRE(static_cast<bool>(result));

    auto const & spec = result.value();
    CHECK(spec.model_id == "file_test");

    // weights_path should be resolved relative to the JSON file's directory
    auto expected_path = (tmp_dir / "weights" / "model.pte").string();
    CHECK(spec.weights_path.value() == expected_path);

    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("RuntimeModelSpec - fromJsonFile with absolute weights_path", "[runtime]") {
    auto const tmp_dir = std::filesystem::temp_directory_path() / "dl_test_runtime2";
    std::filesystem::create_directories(tmp_dir);
    auto const json_path = tmp_dir / "model_spec.json";

    std::string const json = R"({
        "model_id": "abs_test",
        "display_name": "Abs Test",
        "weights_path": "/absolute/path/model.pte",
        "inputs": [{ "name": "x", "shape": [1] }],
        "outputs": [{ "name": "y", "shape": [1] }]
    })";

    {
        std::ofstream ofs(json_path);
        ofs << json;
    }

    auto result = RuntimeModelSpec::fromJsonFile(json_path);
    REQUIRE(static_cast<bool>(result));

    CHECK(result.value().weights_path.value() == "/absolute/path/model.pte");

    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("RuntimeModelSpec - fromJsonFile nonexistent file", "[runtime]") {
    auto result = RuntimeModelSpec::fromJsonFile("/nonexistent/path/model.json");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_THAT(errorMsg(result), ContainsSubstring("Failed to open"));
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModel - construction and metadata
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModel - metadata from spec", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    CHECK(model.modelId() == "test_model");
    CHECK(model.displayName() == "Test Model");
    CHECK(model.description() == "A test model for unit tests");
    CHECK(model.preferredBatchSize() == 1);
    CHECK(model.maxBatchSize() == 4);
}

TEST_CASE("RuntimeModel - minimal spec uses defaults", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    CHECK(model.modelId() == "minimal_model");
    CHECK(model.displayName() == "Minimal");
    CHECK(model.description().empty());
    CHECK(model.preferredBatchSize() == 0);
    CHECK(model.maxBatchSize() == 0);
}

TEST_CASE("RuntimeModel - input/output slots match spec", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 3);
    CHECK(inputs[0].name == "image");
    auto const expected_shape = std::vector<int64_t>{3, 256, 256};
    CHECK(inputs[0].shape == expected_shape);
    CHECK(inputs[0].recommended_encoder == "ImageEncoder");
    CHECK(inputs[0].is_static == false);
    CHECK(inputs[1].name == "memory_images");
    CHECK(inputs[1].is_static == true);
    CHECK(inputs[1].sequence_dim == 0);
    CHECK(inputs[2].name == "memory_mask");
    CHECK(inputs[2].is_boolean_mask == true);

    auto outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
    CHECK(outputs[0].name == "heatmap");
    CHECK(outputs[0].recommended_decoder == "TensorToMask2D");
}

TEST_CASE("RuntimeModel - isReady before loadWeights", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));
    CHECK_FALSE(model.isReady());
}

TEST_CASE("RuntimeModel - forward throws when not ready", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    std::unordered_map<std::string, torch::Tensor> inputs;
    CHECK_THROWS_AS(model.forward(inputs), std::runtime_error);
}

TEST_CASE("RuntimeModel - spec() accessor", "[runtime]") {
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    auto const & spec = model.spec();
    CHECK(spec.model_id == "test_model");
    CHECK(spec.inputs.size() == 3);
    CHECK(spec.outputs.size() == 1);
}

// ═══════════════════════════════════════════════════════════════
// ModelRegistry - registerFromJson
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ModelRegistry - registerFromJson success", "[runtime]") {
    auto const tmp_dir = std::filesystem::temp_directory_path() / "dl_test_registry_json";
    std::filesystem::create_directories(tmp_dir);
    auto const json_path = tmp_dir / "model.json";

    std::string const json = R"({
        "model_id": "json_registered_model",
        "display_name": "JSON Model",
        "description": "Registered from JSON",
        "inputs": [{ "name": "x", "shape": [3, 64, 64] }],
        "outputs": [{ "name": "y", "shape": [1, 64, 64] }]
    })";

    {
        std::ofstream ofs(json_path);
        ofs << json;
    }

    auto & registry = ModelRegistry::instance();
    registry.unregisterModel("json_registered_model");

    auto result = registry.registerFromJson(json_path);
    REQUIRE(result.has_value());
    CHECK(result.value() == "json_registered_model");

    CHECK(registry.hasModel("json_registered_model"));

    auto model = registry.create("json_registered_model");
    REQUIRE(model != nullptr);
    CHECK(model->modelId() == "json_registered_model");
    CHECK(model->displayName() == "JSON Model");
    CHECK(model->inputSlots().size() == 1);
    CHECK(model->outputSlots().size() == 1);

    auto info = registry.getModelInfo("json_registered_model");
    REQUIRE(info.has_value());
    CHECK(info->display_name == "JSON Model");
    CHECK(info->description == "Registered from JSON");
    CHECK(info->inputs.size() == 1);
    CHECK(info->outputs.size() == 1);

    registry.unregisterModel("json_registered_model");
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("ModelRegistry - registerFromJson with nonexistent file", "[runtime]") {
    auto & registry = ModelRegistry::instance();
    std::string error;
    auto result = registry.registerFromJson("/nonexistent/model.json", &error);
    REQUIRE_FALSE(result.has_value());
    CHECK_THAT(error, ContainsSubstring("Failed to open"));
}

TEST_CASE("ModelRegistry - registerFromJson with invalid spec", "[runtime]") {
    auto const tmp_dir = std::filesystem::temp_directory_path() / "dl_test_registry_invalid";
    std::filesystem::create_directories(tmp_dir);
    auto const json_path = tmp_dir / "bad_model.json";

    std::string const json = R"({
        "model_id": "",
        "display_name": "Bad",
        "inputs": [{ "name": "x", "shape": [1] }],
        "outputs": [{ "name": "y", "shape": [1] }]
    })";

    {
        std::ofstream ofs(json_path);
        ofs << json;
    }

    auto & registry = ModelRegistry::instance();
    std::string error;
    auto result = registry.registerFromJson(json_path, &error);
    REQUIRE_FALSE(result.has_value());
    CHECK_THAT(error, ContainsSubstring("model_id"));

    std::filesystem::remove_all(tmp_dir);
}

// ──────────────────────────────────────────────────────────────
// Phase 3A: backend field
// ──────────────────────────────────────────────────────────────

TEST_CASE("RuntimeModelSpec - parse with backend field", "[runtime]") {
    auto json = R"({
        "model_id": "backend_test",
        "display_name": "Backend Test",
        "weights_path": "/tmp/model.pt2",
        "backend": "aotinductor",
        "inputs": [
            { "name": "x", "shape": [3, 64, 64] }
        ],
        "outputs": [
            { "name": "y", "shape": [1, 64, 64] }
        ]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    auto const & spec = result.value();

    REQUIRE(spec.backend.has_value());
    CHECK(spec.backend.value() == "aotinductor");
}

TEST_CASE("RuntimeModelSpec - parse without backend field defaults to empty", "[runtime]") {
    auto json = R"({
        "model_id": "no_backend",
        "display_name": "No Backend",
        "inputs": [
            { "name": "x", "shape": [1] }
        ],
        "outputs": [
            { "name": "y", "shape": [1] }
        ]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    CHECK_FALSE(result.value().backend.has_value());
}

TEST_CASE("RuntimeModelSpec - backend field roundtrips through toJson", "[runtime]") {
    auto json = R"({
        "model_id": "roundtrip_backend",
        "display_name": "Roundtrip",
        "backend": "torchscript",
        "inputs": [
            { "name": "x", "shape": [1] }
        ],
        "outputs": [
            { "name": "y", "shape": [1] }
        ]
    })";

    auto result1 = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result1);
    auto serialized = result1.value().toJson();

    auto result2 = dl::RuntimeModelSpec::fromJson(serialized);
    REQUIRE(result2);
    REQUIRE(result2.value().backend.has_value());
    CHECK(result2.value().backend.value() == "torchscript");
}

// ════════════════════════════════════════════════════════════════════════════
// Phase 6: BatchModeSpec
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("BatchModeSpec - default produces DynamicBatch(1,0)",
          "[RuntimeModelSpec][BatchMode]") {
    dl::BatchModeSpec spec;
    auto mode = spec.toBatchMode();
    REQUIRE(std::holds_alternative<dl::DynamicBatch>(mode));
    auto const & d = std::get<dl::DynamicBatch>(mode);
    CHECK(d.min_size == 1);
    CHECK(d.max_size == 0);
}

TEST_CASE("BatchModeSpec - fixed", "[RuntimeModelSpec][BatchMode]") {
    dl::BatchModeSpec spec;
    spec.fixed = 4;
    auto mode = spec.toBatchMode();
    REQUIRE(std::holds_alternative<dl::FixedBatch>(mode));
    CHECK(std::get<dl::FixedBatch>(mode).size == 4);
}

TEST_CASE("BatchModeSpec - dynamic", "[RuntimeModelSpec][BatchMode]") {
    dl::BatchModeSpec spec;
    spec.dynamic = dl::BatchModeSpec::DynamicSpec{2, 16};
    auto mode = spec.toBatchMode();
    REQUIRE(std::holds_alternative<dl::DynamicBatch>(mode));
    auto const & d = std::get<dl::DynamicBatch>(mode);
    CHECK(d.min_size == 2);
    CHECK(d.max_size == 16);
}

TEST_CASE("BatchModeSpec - recurrent_only", "[RuntimeModelSpec][BatchMode]") {
    dl::BatchModeSpec spec;
    spec.recurrent_only = true;
    auto mode = spec.toBatchMode();
    CHECK(std::holds_alternative<dl::RecurrentOnlyBatch>(mode));
}

TEST_CASE("RuntimeModelSpec - JSON round-trip with batch_mode",
          "[RuntimeModelSpec][BatchMode]") {
    std::string const json = R"({
        "model_id": "bm_test",
        "display_name": "Batch Mode Test",
        "batch_mode": { "fixed": 8 },
        "inputs": [
            { "name": "x", "shape": [3, 64, 64] }
        ],
        "outputs": [
            { "name": "y", "shape": [1, 64, 64] }
        ]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    auto const & spec = result.value();
    REQUIRE(spec.batch_mode.has_value());
    REQUIRE(spec.batch_mode->fixed.has_value());
    CHECK(spec.batch_mode->fixed.value() == 8);

    // Round-trip
    auto serialized = spec.toJson();
    auto result2 = dl::RuntimeModelSpec::fromJson(serialized);
    REQUIRE(result2);
    REQUIRE(result2.value().batch_mode.has_value());
    REQUIRE(result2.value().batch_mode->fixed.has_value());
    CHECK(result2.value().batch_mode->fixed.value() == 8);
}

TEST_CASE("RuntimeModelSpec - JSON with dynamic batch_mode",
          "[RuntimeModelSpec][BatchMode]") {
    std::string const json = R"({
        "model_id": "dyn_test",
        "display_name": "Dynamic Test",
        "batch_mode": { "dynamic": { "min": 1, "max": 32 } },
        "inputs": [{ "name": "x", "shape": [1] }],
        "outputs": [{ "name": "y", "shape": [1] }]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    auto mode = result.value().batch_mode->toBatchMode();
    REQUIRE(std::holds_alternative<dl::DynamicBatch>(mode));
    CHECK(std::get<dl::DynamicBatch>(mode).min_size == 1);
    CHECK(std::get<dl::DynamicBatch>(mode).max_size == 32);
}

TEST_CASE("RuntimeModelSpec - JSON with recurrent_only batch_mode",
          "[RuntimeModelSpec][BatchMode]") {
    std::string const json = R"({
        "model_id": "rec_test",
        "display_name": "Recurrent Test",
        "batch_mode": { "recurrent_only": true },
        "inputs": [{ "name": "x", "shape": [1] }],
        "outputs": [{ "name": "y", "shape": [1] }]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    auto mode = result.value().batch_mode->toBatchMode();
    CHECK(std::holds_alternative<dl::RecurrentOnlyBatch>(mode));
}

// ════════════════════════════════════════════════════════════════════════════
// Phase 6: WeightsVariant
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModelSpec - weights_variants JSON round-trip",
          "[RuntimeModelSpec][WeightsVariant]") {
    std::string const json = R"({
        "model_id": "mv_test",
        "display_name": "Multi-Variant Test",
        "weights_variants": [
            { "path": "model_b1.pt2", "batch_size": 1, "label": "recurrent" },
            { "path": "model_b8.pt2", "batch_size": 8, "label": "batched" }
        ],
        "inputs": [{ "name": "x", "shape": [3, 64, 64] }],
        "outputs": [{ "name": "y", "shape": [1, 64, 64] }]
    })";

    auto result = dl::RuntimeModelSpec::fromJson(json);
    REQUIRE(result);
    auto const & spec = result.value();
    REQUIRE(spec.weights_variants.has_value());
    REQUIRE(spec.weights_variants->size() == 2);
    CHECK((*spec.weights_variants)[0].path == "model_b1.pt2");
    CHECK((*spec.weights_variants)[0].batch_size == 1);
    CHECK((*spec.weights_variants)[0].label.value() == "recurrent");
    CHECK((*spec.weights_variants)[1].path == "model_b8.pt2");
    CHECK((*spec.weights_variants)[1].batch_size == 8);

    // Round-trip
    auto serialized = spec.toJson();
    auto result2 = dl::RuntimeModelSpec::fromJson(serialized);
    REQUIRE(result2);
    REQUIRE(result2.value().weights_variants.has_value());
    CHECK(result2.value().weights_variants->size() == 2);
}

TEST_CASE("RuntimeModelSpec - validate batch_mode errors",
          "[RuntimeModelSpec][BatchMode]") {
    SECTION("multiple modes set") {
        dl::RuntimeModelSpec spec;
        spec.model_id = "err_test";
        spec.display_name = "Error Test";
        spec.inputs = {makeSlot("x", {1})};
        spec.outputs = {makeSlot("y", {1})};
        spec.batch_mode = dl::BatchModeSpec{};
        spec.batch_mode->fixed = 4;
        spec.batch_mode->recurrent_only = true;

        auto errors = spec.validate();
        CHECK(!errors.empty());
        bool found = false;
        for (auto const & e: errors) {
            if (e.find("only one") != std::string::npos) found = true;
        }
        CHECK(found);
    }

    SECTION("fixed < 1") {
        dl::RuntimeModelSpec spec;
        spec.model_id = "err_test2";
        spec.display_name = "Error Test 2";
        spec.inputs = {makeSlot("x", {1})};
        spec.outputs = {makeSlot("y", {1})};
        spec.batch_mode = dl::BatchModeSpec{};
        spec.batch_mode->fixed = 0;

        auto errors = spec.validate();
        bool found = false;
        for (auto const & e: errors) {
            if (e.find("fixed") != std::string::npos) found = true;
        }
        CHECK(found);
    }

    SECTION("dynamic max < min") {
        dl::RuntimeModelSpec spec;
        spec.model_id = "err_test3";
        spec.display_name = "Error Test 3";
        spec.inputs = {makeSlot("x", {1})};
        spec.outputs = {makeSlot("y", {1})};
        spec.batch_mode = dl::BatchModeSpec{};
        spec.batch_mode->dynamic = dl::BatchModeSpec::DynamicSpec{4, 2};

        auto errors = spec.validate();
        bool found = false;
        for (auto const & e: errors) {
            if (e.find("max") != std::string::npos) found = true;
        }
        CHECK(found);
    }
}

TEST_CASE("RuntimeModelSpec - validate weights_variants errors",
          "[RuntimeModelSpec][WeightsVariant]") {
    SECTION("empty path") {
        dl::RuntimeModelSpec spec;
        spec.model_id = "wv_err";
        spec.display_name = "WV Error";
        spec.inputs = {makeSlot("x", {1})};
        spec.outputs = {makeSlot("y", {1})};
        spec.weights_variants = std::vector<dl::WeightsVariant>{
                {.path = "", .batch_size = 1}};

        auto errors = spec.validate();
        bool found = false;
        for (auto const & e: errors) {
            if (e.find("path") != std::string::npos) found = true;
        }
        CHECK(found);
    }

    SECTION("duplicate batch_size") {
        dl::RuntimeModelSpec spec;
        spec.model_id = "wv_dup";
        spec.display_name = "WV Dup";
        spec.inputs = {makeSlot("x", {1})};
        spec.outputs = {makeSlot("y", {1})};
        spec.weights_variants = std::vector<dl::WeightsVariant>{
                {.path = "a.pt2", .batch_size = 4},
                {.path = "b.pt2", .batch_size = 4}};

        auto errors = spec.validate();
        bool found = false;
        for (auto const & e: errors) {
            if (e.find("duplicate") != std::string::npos) found = true;
        }
        CHECK(found);
    }
}
