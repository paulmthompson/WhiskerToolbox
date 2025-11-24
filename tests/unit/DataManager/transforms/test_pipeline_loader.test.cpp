// Unit tests for V2 Pipeline Loader
// Tests JSON loading, validation, and error handling

#include "transforms/v2/examples/PipelineLoader.hpp"
#include "transforms/v2/examples/RegisteredTransforms.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Pipeline Descriptor Tests
// ============================================================================

TEST_CASE("PipelineDescriptor can be serialized to JSON", "[pipeline][json]") {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{
        .name = "Test Pipeline",
        .version = "1.0"
    };
    
    descriptor.steps = {
        PipelineStepDescriptor{
            .step_id = "step1",
            .transform_name = "CalculateMaskArea"
        }
    };
    
    auto json = savePipelineToJson(descriptor);
    REQUIRE(!json.empty());
    REQUIRE(json.find("Test Pipeline") != std::string::npos);
    REQUIRE(json.find("CalculateMaskArea") != std::string::npos);
}

TEST_CASE("PipelineDescriptor can be deserialized from JSON", "[pipeline][json]") {
    std::string json = R"({
        "metadata": {
            "name": "Test Pipeline",
            "version": "1.0"
        },
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea"
            }
        ]
    })";
    
    auto result = rfl::json::read<PipelineDescriptor>(json);
    REQUIRE(result);
    
    auto desc = result.value();
    REQUIRE(desc.metadata.has_value());
    REQUIRE(desc.metadata->name == "Test Pipeline");
    REQUIRE(desc.steps.size() == 1);
    REQUIRE(desc.steps[0].step_id == "step1");
    REQUIRE(desc.steps[0].transform_name == "CalculateMaskArea");
}

// ============================================================================
// Step Loading Tests
// ============================================================================

TEST_CASE("loadStepFromDescriptor loads valid step with no parameters", "[pipeline][step]") {
    PipelineStepDescriptor descriptor{
        .step_id = "test_step",
        .transform_name = "CalculateMaskArea"
    };
    
    auto result = loadStepFromDescriptor(descriptor);
    REQUIRE(result);
    
    auto step = result.value();
    REQUIRE(step.transform_name == "CalculateMaskArea");
}

TEST_CASE("loadStepFromDescriptor loads valid step with parameters", "[pipeline][step]") {
    // Create parameters as rfl::Generic (simulating JSON parse)
    std::string param_json = R"({"scale_factor": 2.5, "min_area": 10.0})";
    auto param_generic = rfl::json::read<rfl::Generic>(param_json).value();
    
    PipelineStepDescriptor descriptor{
        .step_id = "test_step",
        .transform_name = "CalculateMaskArea",
        .parameters = param_generic
    };
    
    auto result = loadStepFromDescriptor(descriptor);
    REQUIRE(result);
    
    auto step = result.value();
    REQUIRE(step.transform_name == "CalculateMaskArea");
    
    // Verify parameters were loaded correctly
    auto const& params = std::any_cast<MaskAreaParams const&>(step.params);
    REQUIRE(params.getScaleFactor() == 2.5f);
    REQUIRE(params.getMinArea() == 10.0f);
}

TEST_CASE("loadStepFromDescriptor rejects unknown transform", "[pipeline][step][error]") {
    PipelineStepDescriptor descriptor{
        .step_id = "test_step",
        .transform_name = "NonExistentTransform"
    };
    
    auto result = loadStepFromDescriptor(descriptor);
    REQUIRE(!result);
    REQUIRE(std::string(result.error()->what()).find("not found") != std::string::npos);
}

TEST_CASE("loadStepFromDescriptor rejects invalid parameters", "[pipeline][step][error]") {
    // Negative scale_factor should fail validation
    std::string param_json = R"({"scale_factor": -1.0})";
    auto param_generic = rfl::json::read<rfl::Generic>(param_json).value();
    
    PipelineStepDescriptor descriptor{
        .step_id = "test_step",
        .transform_name = "CalculateMaskArea",
        .parameters = param_generic
    };
    
    auto result = loadStepFromDescriptor(descriptor);
    REQUIRE(!result);
}

TEST_CASE("loadStepFromDescriptor skips disabled steps", "[pipeline][step]") {
    PipelineStepDescriptor descriptor{
        .step_id = "test_step",
        .transform_name = "CalculateMaskArea",
        .enabled = false
    };
    
    auto result = loadStepFromDescriptor(descriptor);
    REQUIRE(!result);
    REQUIRE(std::string(result.error()->what()).find("disabled") != std::string::npos);
}

// ============================================================================
// Pipeline Loading Tests
// ============================================================================

TEST_CASE("loadPipelineFromJson loads minimal valid pipeline", "[pipeline][json]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea"
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(result);
}

TEST_CASE("loadPipelineFromJson loads pipeline with metadata", "[pipeline][json]") {
    std::string json = R"({
        "metadata": {
            "name": "Test Pipeline",
            "description": "A test pipeline",
            "version": "1.0",
            "author": "Test Author"
        },
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea"
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(result);
}

TEST_CASE("loadPipelineFromJson loads pipeline with parameters", "[pipeline][json]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "calculate_area",
                "transform_name": "CalculateMaskArea",
                "parameters": {
                    "scale_factor": 1.5,
                    "min_area": 5.0,
                    "exclude_holes": true
                }
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(result);
}

TEST_CASE("loadPipelineFromJson loads multi-step pipeline", "[pipeline][json]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea",
                "parameters": {
                    "scale_factor": 1.5
                }
            },
            {
                "step_id": "step2",
                "transform_name": "SumReduction",
                "parameters": {
                    "ignore_nan": true
                }
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(result);
}

TEST_CASE("loadPipelineFromJson rejects empty pipeline", "[pipeline][json][error]") {
    std::string json = R"({
        "steps": []
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(!result);
    REQUIRE(std::string(result.error()->what()).find("at least one step") != std::string::npos);
}

TEST_CASE("loadPipelineFromJson rejects malformed JSON", "[pipeline][json][error]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1"
                // Missing transform_name - invalid JSON
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(!result);
}

TEST_CASE("loadPipelineFromJson rejects pipeline with invalid step", "[pipeline][json][error]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "NonExistentTransform"
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(!result);
    REQUIRE(std::string(result.error()->what()).find("not found") != std::string::npos);
}

TEST_CASE("loadPipelineFromJson rejects pipeline with invalid parameters", "[pipeline][json][error]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea",
                "parameters": {
                    "scale_factor": -1.0
                }
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(!result);
}

TEST_CASE("loadPipelineFromJson handles optional fields", "[pipeline][json]") {
    std::string json = R"({
        "steps": [
            {
                "step_id": "step1",
                "transform_name": "CalculateMaskArea",
                "description": "Calculate mask area",
                "enabled": true,
                "tags": ["analysis", "masks"]
            }
        ]
    })";
    
    auto result = loadPipelineFromJson(json);
    REQUIRE(result);
}

// ============================================================================
// File Loading Tests
// ============================================================================

TEST_CASE("loadPipelineFromFile handles missing file", "[pipeline][file][error]") {
    auto result = loadPipelineFromFile("/nonexistent/path/pipeline.json");
    REQUIRE(!result);
    REQUIRE(std::string(result.error()->what()).find("open") != std::string::npos);
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

TEST_CASE("Pipeline descriptor round-trips through JSON", "[pipeline][json][roundtrip]") {
    PipelineDescriptor original;
    original.metadata = PipelineMetadata{
        .name = "Test Pipeline",
        .version = "1.0"
    };
    
    std::string param_json = R"({"scale_factor": 2.5})";
    auto param_generic = rfl::json::read<rfl::Generic>(param_json).value();
    
    original.steps = {
        PipelineStepDescriptor{
            .step_id = "step1",
            .transform_name = "CalculateMaskArea",
            .parameters = param_generic
        }
    };
    
    // Serialize
    auto json = savePipelineToJson(original);
    
    // Deserialize
    auto result = rfl::json::read<PipelineDescriptor>(json);
    REQUIRE(result);
    
    auto recovered = result.value();
    REQUIRE(recovered.metadata.has_value());
    REQUIRE(recovered.metadata->name == "Test Pipeline");
    REQUIRE(recovered.steps.size() == 1);
    REQUIRE(recovered.steps[0].step_id == "step1");
    REQUIRE(recovered.steps[0].transform_name == "CalculateMaskArea");
    REQUIRE(recovered.steps[0].parameters.has_value());
}
