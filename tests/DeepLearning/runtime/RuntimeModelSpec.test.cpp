#include "runtime/RuntimeModel.hpp"
#include "runtime/RuntimeModelSpec.hpp"
#include "registry/ModelRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace dl;
using Catch::Matchers::ContainsSubstring;

// Helper to create a SlotSpec with only name and shape
static SlotSpec makeSlot(std::string name, std::vector<int64_t> shape)
{
    SlotSpec s;
    s.name = std::move(name);
    s.shape = std::move(shape);
    return s;
}

static SlotSpec makeSlotWithSeqDim(std::string name, std::vector<int64_t> shape, int seq_dim)
{
    SlotSpec s;
    s.name = std::move(name);
    s.shape = std::move(shape);
    s.sequence_dim = seq_dim;
    return s;
}

/// Extract error message from an rfl::Result
template<typename T>
static std::string errorMsg(rfl::Result<T> const & result)
{
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

TEST_CASE("SlotSpec - toDescriptor with all fields", "[runtime]")
{
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
    CHECK(desc.shape == std::vector<int64_t>{3, 256, 256});
    CHECK(desc.description == "Current frame");
    CHECK(desc.recommended_encoder == "ImageEncoder");
    CHECK(desc.recommended_decoder == "TensorToMask2D");
    CHECK(desc.is_static == true);
    CHECK(desc.is_boolean_mask == true);
    CHECK(desc.sequence_dim == 0);
}

TEST_CASE("SlotSpec - toDescriptor applies defaults for omitted fields", "[runtime]")
{
    SlotSpec slot;
    slot.name = "x";
    slot.shape = {1, 64, 64};
    // All optional fields left as std::nullopt

    auto desc = slot.toDescriptor();

    CHECK(desc.name == "x");
    CHECK(desc.shape == std::vector<int64_t>{1, 64, 64});
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

TEST_CASE("RuntimeModelSpec - parse full JSON", "[runtime]")
{
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
    CHECK(spec.inputs[0].shape == std::vector<int64_t>{3, 256, 256});
    CHECK(spec.inputs[0].recommended_encoder.value() == "ImageEncoder");
    CHECK(spec.inputs[0].is_static.value() == false);
    CHECK(spec.inputs[1].name == "memory_images");
    CHECK(spec.inputs[1].is_static.value() == true);
    CHECK(spec.inputs[1].sequence_dim.value() == 0);
    CHECK(spec.inputs[2].name == "memory_mask");
    CHECK(spec.inputs[2].is_boolean_mask.value() == true);

    REQUIRE(spec.outputs.size() == 1);
    CHECK(spec.outputs[0].name == "heatmap");
    CHECK(spec.outputs[0].shape == std::vector<int64_t>{1, 256, 256});
    CHECK(spec.outputs[0].recommended_decoder.value() == "TensorToMask2D");
}

TEST_CASE("RuntimeModelSpec - parse minimal JSON", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - parse invalid JSON returns error", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson("not valid json{{{");
    CHECK_FALSE(static_cast<bool>(result));
}

TEST_CASE("RuntimeModelSpec - parse JSON missing required field", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - toJson produces parseable JSON", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - inputDescriptors and outputDescriptors", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - validate passes on valid spec", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    auto errors = result.value().validate();
    CHECK(errors.empty());
}

TEST_CASE("RuntimeModelSpec - validate catches empty model_id", "[runtime]")
{
    RuntimeModelSpec spec;
    spec.model_id = "";
    spec.display_name = "Name";
    spec.inputs = {makeSlot("x", {1})};
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("model_id"));
}

TEST_CASE("RuntimeModelSpec - validate catches empty display_name", "[runtime]")
{
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "";
    spec.inputs = {makeSlot("x", {1})};
    spec.outputs = {makeSlot("y", {1})};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("display_name"));
}

TEST_CASE("RuntimeModelSpec - validate catches duplicate input names", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - validate catches sequence_dim out of bounds", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - validate catches empty shape", "[runtime]")
{
    RuntimeModelSpec spec;
    spec.model_id = "ok";
    spec.display_name = "OK";
    spec.inputs = {makeSlot("x", {})};
    spec.outputs = {};

    auto errors = spec.validate();
    REQUIRE_FALSE(errors.empty());
    CHECK_THAT(errors[0], ContainsSubstring("shape"));
}

TEST_CASE("RuntimeModelSpec - validate catches negative batch sizes", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - fromJsonFile with relative weights_path", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - fromJsonFile with absolute weights_path", "[runtime]")
{
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

TEST_CASE("RuntimeModelSpec - fromJsonFile nonexistent file", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJsonFile("/nonexistent/path/model.json");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_THAT(errorMsg(result), ContainsSubstring("Failed to open"));
}

// ═══════════════════════════════════════════════════════════════
// RuntimeModel - construction and metadata
// ═══════════════════════════════════════════════════════════════

TEST_CASE("RuntimeModel - metadata from spec", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    CHECK(model.modelId() == "test_model");
    CHECK(model.displayName() == "Test Model");
    CHECK(model.description() == "A test model for unit tests");
    CHECK(model.preferredBatchSize() == 1);
    CHECK(model.maxBatchSize() == 4);
}

TEST_CASE("RuntimeModel - minimal spec uses defaults", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    CHECK(model.modelId() == "minimal_model");
    CHECK(model.displayName() == "Minimal");
    CHECK(model.description().empty());
    CHECK(model.preferredBatchSize() == 0);
    CHECK(model.maxBatchSize() == 0);
}

TEST_CASE("RuntimeModel - input/output slots match spec", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kFullJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    auto inputs = model.inputSlots();
    REQUIRE(inputs.size() == 3);
    CHECK(inputs[0].name == "image");
    CHECK(inputs[0].shape == std::vector<int64_t>{3, 256, 256});
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

TEST_CASE("RuntimeModel - isReady before loadWeights", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));
    CHECK_FALSE(model.isReady());
}

TEST_CASE("RuntimeModel - forward throws when not ready", "[runtime]")
{
    auto result = RuntimeModelSpec::fromJson(kMinimalJson);
    REQUIRE(static_cast<bool>(result));

    RuntimeModel model(std::move(result.value()));

    std::unordered_map<std::string, torch::Tensor> inputs;
    CHECK_THROWS_AS(model.forward(inputs), std::runtime_error);
}

TEST_CASE("RuntimeModel - spec() accessor", "[runtime]")
{
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

TEST_CASE("ModelRegistry - registerFromJson success", "[runtime]")
{
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

TEST_CASE("ModelRegistry - registerFromJson with nonexistent file", "[runtime]")
{
    auto & registry = ModelRegistry::instance();
    std::string error;
    auto result = registry.registerFromJson("/nonexistent/model.json", &error);
    REQUIRE_FALSE(result.has_value());
    CHECK_THAT(error, ContainsSubstring("Failed to open"));
}

TEST_CASE("ModelRegistry - registerFromJson with invalid spec", "[runtime]")
{
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
