// Fuzz test for V2 transform parameter JSON loading
// Tests both valid variations and invalid/malformed JSON to ensure robustness

#include "transforms/v2/examples/ParameterIO.hpp"

#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>

#include <cmath>
#include <string>

using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Fuzz Tests: MaskAreaParams
// ============================================================================

/**
 * @brief Fuzz test for MaskAreaParams JSON loading - should never crash
 * 
 * This test feeds arbitrary strings to the JSON parser to ensure it handles
 * malformed/invalid input gracefully without crashing.
 */
void FuzzMaskAreaParamsNoCrash(std::string const& json_str) {
    // Should not crash on any input, even malformed JSON
    auto result = loadParametersFromJson<MaskAreaParams>(json_str);
    // We don't care if it succeeds or fails, just that it doesn't crash
    (void)result;
}
FUZZ_TEST(MaskAreaParamsFuzz, FuzzMaskAreaParamsNoCrash);

/**
 * @brief Fuzz test with valid scale_factor values
 */
void FuzzMaskAreaParamsValidScaleFactor(float scale_factor) {
    // Skip special float values that don't serialize well to JSON
    // Also skip denormalized floats and values very close to zero
    if (!std::isfinite(scale_factor) || scale_factor <= 1e-6f) {
        return;
    }
    
    // Use reflect-cpp to serialize the struct properly instead of manual JSON construction
    MaskAreaParams params;
    params.scale_factor = scale_factor;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    // Should succeed for positive finite values
    EXPECT_TRUE(result);
    if (result) {
        auto recovered = result.value();
        // Use EXPECT_NEAR to account for JSON serialization precision loss
        EXPECT_NEAR(recovered.getScaleFactor(), scale_factor, 1e-5f);
    }
}
FUZZ_TEST(MaskAreaParamsFuzz, FuzzMaskAreaParamsValidScaleFactor)
    .WithDomains(fuzztest::Positive<float>());

/**
 * @brief Fuzz test with valid min_area values
 */
void FuzzMaskAreaParamsValidMinArea(float min_area) {
    // Skip special float values that don't serialize well to JSON
    if (!std::isfinite(min_area)) {
        return;
    }
    
    std::string json;
    if (min_area >= 0.0f) {
        // For valid values, use reflect-cpp to serialize properly
        MaskAreaParams params;
        params.min_area = min_area;
        json = saveParametersToJson(params);
        
        auto result = loadParametersFromJson<MaskAreaParams>(json);
        // Should succeed for non-negative values
        EXPECT_TRUE(result);
        if (result) {
            auto recovered = result.value();
            // Use EXPECT_NEAR to account for JSON serialization precision loss
            EXPECT_NEAR(recovered.getMinArea(), min_area, 1e-5f);
        }
    } else {
        // For invalid values, construct JSON manually to bypass serialization validation
        // This tests that deserialization properly rejects invalid values
        json = "{\"min_area\": " + std::to_string(min_area) + "}";
        
        auto result = loadParametersFromJson<MaskAreaParams>(json);
        // Should fail for negative values
        EXPECT_FALSE(result);
    }
}
FUZZ_TEST(MaskAreaParamsFuzz, FuzzMaskAreaParamsValidMinArea)
    .WithDomains(fuzztest::InRange<float>(-1000.0f, 1000.0f));

/**
 * @brief Fuzz test with boolean exclude_holes
 */
void FuzzMaskAreaParamsExcludeHoles(bool exclude_holes) {
    // Use reflect-cpp to serialize the struct properly
    MaskAreaParams params;
    params.exclude_holes = exclude_holes;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    EXPECT_TRUE(result);
    if (result) {
        auto recovered = result.value();
        EXPECT_EQ(recovered.getExcludeHoles(), exclude_holes);
    }
}
FUZZ_TEST(MaskAreaParamsFuzz, FuzzMaskAreaParamsExcludeHoles);

/**
 * @brief Fuzz test with complete valid parameter sets
 */
void FuzzMaskAreaParamsComplete(float scale_factor, float min_area, bool exclude_holes) {
    // Skip special float values that don't serialize well to JSON
    // Also skip denormalized floats that might round to 0 in string conversion
    if (!std::isfinite(scale_factor) || !std::isfinite(min_area)) {
        return;
    }
    
    // Skip scale_factor values too close to zero (would violate ExclusiveMinimum)
    if (scale_factor <= 1e-6f) {
        return;
    }
    
    // Use reflect-cpp to serialize the struct properly
    MaskAreaParams params;
    params.scale_factor = scale_factor;
    params.min_area = min_area;
    params.exclude_holes = exclude_holes;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    // Validation rules
    bool should_succeed = (scale_factor > 0.0f) && (min_area >= 0.0f);
    
    EXPECT_EQ(static_cast<bool>(result), should_succeed);
    
    if (result) {
        auto recovered = result.value();
        // Use EXPECT_NEAR to account for JSON serialization precision loss
        EXPECT_NEAR(recovered.getScaleFactor(), scale_factor, 1e-5f);
        EXPECT_NEAR(recovered.getMinArea(), min_area, 1e-5f);
        EXPECT_EQ(recovered.getExcludeHoles(), exclude_holes);
    }
}
FUZZ_TEST(MaskAreaParamsFuzz, FuzzMaskAreaParamsComplete)
    .WithDomains(
        fuzztest::Positive<float>(),  // scale_factor > 0
        fuzztest::NonNegative<float>(),  // min_area >= 0
        fuzztest::Arbitrary<bool>()
    );

// ============================================================================
// Fuzz Tests: SumReductionParams
// ============================================================================

/**
 * @brief Fuzz test for SumReductionParams JSON loading - should never crash
 */
void FuzzSumReductionParamsNoCrash(std::string const& json_str) {
    auto result = loadParametersFromJson<SumReductionParams>(json_str);
    (void)result;
}
FUZZ_TEST(SumReductionParamsFuzz, FuzzSumReductionParamsNoCrash);

/**
 * @brief Fuzz test with arbitrary default_value (any float is valid)
 */
void FuzzSumReductionParamsDefaultValue(float default_value) {
    // Skip special float values that don't serialize well to JSON
    if (!std::isfinite(default_value)) {
        return;
    }
    
    // Use reflect-cpp to serialize the struct properly
    SumReductionParams params;
    params.default_value = default_value;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    // Any finite float value should be valid
    EXPECT_TRUE(result);
    if (result) {
        auto recovered = result.value();
        // Use EXPECT_NEAR to account for JSON serialization precision loss
        EXPECT_NEAR(recovered.getDefaultValue(), default_value, 1e-5f);
    }
}
FUZZ_TEST(SumReductionParamsFuzz, FuzzSumReductionParamsDefaultValue)
    .WithDomains(fuzztest::Arbitrary<float>());

/**
 * @brief Fuzz test with boolean ignore_nan
 */
void FuzzSumReductionParamsIgnoreNan(bool ignore_nan) {
    // Use reflect-cpp to serialize the struct properly
    SumReductionParams params;
    params.ignore_nan = ignore_nan;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    EXPECT_TRUE(result);
    if (result) {
        auto recovered = result.value();
        EXPECT_EQ(recovered.getIgnoreNan(), ignore_nan);
    }
}
FUZZ_TEST(SumReductionParamsFuzz, FuzzSumReductionParamsIgnoreNan);

/**
 * @brief Fuzz test with complete valid parameter sets
 */
void FuzzSumReductionParamsComplete(bool ignore_nan, float default_value) {
    // Skip special float values that don't serialize well to JSON
    if (!std::isfinite(default_value)) {
        return;
    }
    
    // Use reflect-cpp to serialize the struct properly
    SumReductionParams params;
    params.ignore_nan = ignore_nan;
    params.default_value = default_value;
    
    std::string json = saveParametersToJson(params);
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    // All combinations with finite floats should be valid
    EXPECT_TRUE(result);
    if (result) {
        auto recovered = result.value();
        EXPECT_EQ(recovered.getIgnoreNan(), ignore_nan);
        // Use EXPECT_NEAR to account for JSON serialization precision loss
        EXPECT_NEAR(recovered.getDefaultValue(), default_value, 1e-5f);
    }
}
FUZZ_TEST(SumReductionParamsFuzz, FuzzSumReductionParamsComplete)
    .WithDomains(
        fuzztest::Arbitrary<bool>(),
        fuzztest::Arbitrary<float>()
    );

// ============================================================================
// Fuzz Tests: Parameter Variant Loading
// ============================================================================

/**
 * @brief Fuzz test variant loading with arbitrary transform names and JSON
 */
void FuzzParameterVariantLoading(std::string const& transform_name, std::string const& json_str) {
    auto variant = loadParameterVariant(transform_name, json_str);
    // Should not crash, may or may not succeed
    (void)variant;
}
FUZZ_TEST(ParameterVariantFuzz, FuzzParameterVariantLoading)
    .WithDomains(
        fuzztest::InRegexp("[A-Za-z][A-Za-z0-9 ]*"),  // Valid-looking transform names
        fuzztest::Arbitrary<std::string>()  // Any JSON
    );
