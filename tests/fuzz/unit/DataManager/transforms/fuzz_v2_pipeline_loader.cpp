// Fuzz tests for V2 Pipeline Loader
// Tests robust JSON parsing and error handling for complete pipelines

#include "transforms/v2/examples/PipelineLoader.hpp"
#include "transforms/v2/examples/RegisteredTransforms.hpp"

#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <string>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Fuzz Tests: Pipeline Loading
// ============================================================================

/**
 * @brief Fuzz test for pipeline loading - should never crash
 */
void FuzzPipelineLoadingNoCrash(std::string const& json_str) {
    // Should not crash on any input, even malformed JSON
    auto result = loadPipelineFromJson(json_str);
    // We don't care if it succeeds or fails, just that it doesn't crash
    (void)result;
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzPipelineLoadingNoCrash);

/**
 * @brief Fuzz test for valid pipeline descriptors
 */
void FuzzValidPipelineDescriptor(std::string const& step_id, 
                                  std::string const& transform_name,
                                  float scale_factor,
                                  float min_area) {
    // Skip empty strings which can cause construction issues
    if (step_id.empty() || transform_name.empty()) {
        return;
    }
    
    // Skip invalid float values
    if (!std::isfinite(scale_factor) || !std::isfinite(min_area)) {
        return;
    }
    
    // Skip invalid parameter values
    if (scale_factor <= 1e-6f || min_area < 0.0f) {
        return;
    }
    
    // Build pipeline JSON programmatically
    PipelineDescriptor descriptor;
    descriptor.steps = {
        PipelineStepDescriptor{
            .step_id = step_id,
            .transform_name = transform_name
        }
    };
    
    // Add parameters if valid transform name
    if (transform_name == "CalculateMaskArea") {
        MaskAreaParams params;
        params.scale_factor = scale_factor;
        params.min_area = min_area;
        
        std::string param_json = saveParametersToJson(params);
        auto param_result = rfl::json::read<rfl::Generic>(param_json);
        if (!param_result) {
            return; // Skip if parameter serialization failed
        }
        descriptor.steps[0].parameters = param_result.value();
        
        // Should succeed for valid transform and parameters
        std::string json = savePipelineToJson(descriptor);
        auto result = loadPipelineFromJson(json);
        EXPECT_TRUE(result);
    }
    // For invalid transform names, loading should fail gracefully
    else {
        std::string json = savePipelineToJson(descriptor);
        auto result = loadPipelineFromJson(json);
        // Either succeeds (if transform exists) or fails gracefully
        (void)result;
    }
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzValidPipelineDescriptor)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMinSize(1),  // step_id - non-empty
        fuzztest::PrintableAsciiString().WithMinSize(1),  // transform_name - non-empty
        fuzztest::Positive<float>(),         // scale_factor > 0
        fuzztest::InRange(0.0f, 1000.0f)     // min_area >= 0
    );

/**
 * @brief Fuzz test corpus files - ensure all corpus files are handled correctly
 */
void FuzzCorpusFiles(std::string const& filepath) {
    // Skip excessively long paths or paths with problematic characters
    if (filepath.empty() || filepath.size() > 4096) {
        return;
    }
    
    // Only test paths that look reasonable
    // Check for filesystem operations without throwing
    try {
        if (!std::filesystem::exists(filepath)) {
            return;
        }
        
        auto result = loadPipelineFromFile(filepath);
        // Files may be invalid on purpose - just ensure no crash
        (void)result;
    } catch (std::filesystem::filesystem_error const&) {
        // Filesystem errors (path too long, invalid characters, etc.) are expected
        // from fuzzing - just skip these inputs
        return;
    }
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzCorpusFiles)
    .WithSeeds({
        {"tests/fuzz/corpus/v2_pipelines/simple_mask_area.json"},
        {"tests/fuzz/corpus/v2_pipelines/chained_transforms.json"},
        {"tests/fuzz/corpus/v2_pipelines/minimal_pipeline.json"},
        {"tests/fuzz/corpus/v2_pipelines/with_optional_fields.json"},
        {"tests/fuzz/corpus/v2_pipelines/invalid_transform.json"},
        {"tests/fuzz/corpus/v2_pipelines/invalid_parameters.json"},
        {"tests/fuzz/corpus/v2_pipelines/empty_pipeline.json"},
        {"tests/fuzz/corpus/v2_pipelines/disabled_step.json"}
    });

/**
 * @brief Fuzz test for multi-step pipelines
 */
void FuzzMultiStepPipeline(float scale_factor1, float scale_factor2, bool ignore_nan) {
    // Skip invalid float values
    if (!std::isfinite(scale_factor1) || !std::isfinite(scale_factor2)) {
        return;
    }
    
    // Skip values that violate validators
    if (scale_factor1 <= 1e-6f || scale_factor2 <= 1e-6f) {
        return;
    }
    
    // Build two-step pipeline
    MaskAreaParams params1;
    params1.scale_factor = scale_factor1;
    
    SumReductionParams params2;
    params2.ignore_nan = ignore_nan;
    
    PipelineDescriptor descriptor;
    
    std::string param1_json = saveParametersToJson(params1);
    std::string param2_json = saveParametersToJson(params2);
    
    auto param1_result = rfl::json::read<rfl::Generic>(param1_json);
    auto param2_result = rfl::json::read<rfl::Generic>(param2_json);
    
    if (!param1_result || !param2_result) {
        return; // Skip if parameter serialization failed
    }
    
    descriptor.steps = {
        PipelineStepDescriptor{
            .step_id = "step1",
            .transform_name = "CalculateMaskArea",
            .parameters = param1_result.value()
        },
        PipelineStepDescriptor{
            .step_id = "step2",
            .transform_name = "SumReduction",
            .parameters = param2_result.value()
        }
    };
    
    std::string json = savePipelineToJson(descriptor);
    auto result = loadPipelineFromJson(json);
    
    // Should succeed with valid parameters
    EXPECT_TRUE(result);
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzMultiStepPipeline)
    .WithDomains(
        fuzztest::Positive<float>(),  // scale_factor1
        fuzztest::Positive<float>(),  // scale_factor2
        fuzztest::Arbitrary<bool>()   // ignore_nan
    );

/**
 * @brief Fuzz test for pipeline metadata
 */
void FuzzPipelineMetadata(std::string const& name,
                           std::string const& description,
                           std::string const& version) {
    // Skip empty strings that might cause issues
    // Metadata fields are optional, so we test with non-empty values
    if (name.empty() && description.empty() && version.empty()) {
        return;
    }
    
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{
        .name = name.empty() ? std::nullopt : std::optional<std::string>(name),
        .description = description.empty() ? std::nullopt : std::optional<std::string>(description),
        .version = version.empty() ? std::nullopt : std::optional<std::string>(version)
    };
    
    descriptor.steps = {
        PipelineStepDescriptor{
            .step_id = "step1",
            .transform_name = "CalculateMaskArea"
        }
    };
    
    std::string json = savePipelineToJson(descriptor);
    auto result = loadPipelineFromJson(json);
    
    // Should succeed regardless of metadata content (metadata is optional)
    EXPECT_TRUE(result);
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzPipelineMetadata)
    .WithDomains(
        fuzztest::PrintableAsciiString(),  // name
        fuzztest::PrintableAsciiString(),  // description
        fuzztest::PrintableAsciiString()   // version
    );

/**
 * @brief Fuzz test for optional step fields
 */
void FuzzOptionalStepFields(std::string const& description,
                             bool enabled,
                             std::vector<std::string> const& tags) {
    // Skip disabled steps (they intentionally fail loading)
    if (!enabled) {
        return;
    }
    
    // Filter out empty/problematic strings from tags
    std::vector<std::string> valid_tags;
    for (auto const& tag : tags) {
        if (!tag.empty()) {
            valid_tags.push_back(tag);
        }
    }
    
    PipelineDescriptor descriptor;
    descriptor.steps = {
        PipelineStepDescriptor{
            .step_id = "step1",
            .transform_name = "CalculateMaskArea",
            .description = description.empty() ? std::nullopt : std::optional<std::string>(description),
            .enabled = enabled,
            .tags = valid_tags.empty() ? std::nullopt : std::optional<std::vector<std::string>>(valid_tags)
        }
    };
    
    std::string json = savePipelineToJson(descriptor);
    auto result = loadPipelineFromJson(json);
    
    // Should succeed for enabled steps
    EXPECT_TRUE(result);
}
FUZZ_TEST(PipelineLoaderFuzz, FuzzOptionalStepFields)
    .WithDomains(
        fuzztest::PrintableAsciiString(),  // description
        fuzztest::Just(true),  // Only test enabled=true (disabled is tested elsewhere)
        fuzztest::VectorOf(fuzztest::PrintableAsciiString())  // tags
    );
