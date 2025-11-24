#include "transforms/v2/examples/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Tests: MaskAreaParams JSON Loading
// ============================================================================

TEST_CASE("MaskAreaParams - Load valid JSON with all fields", "[transforms][v2][params][json]") {
    std::string json = R"({
        "scale_factor": 2.5,
        "min_area": 10.0,
        "exclude_holes": true
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(2.5f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(10.0f, 0.001f));
    REQUIRE(params.getExcludeHoles() == true);
}

TEST_CASE("MaskAreaParams - Load JSON with partial fields (uses defaults)", "[transforms][v2][params][json]") {
    std::string json = R"({
        "scale_factor": 3.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(3.0f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(0.0f, 0.001f)); // default
    REQUIRE(params.getExcludeHoles() == false); // default
}

TEST_CASE("MaskAreaParams - Load empty JSON (uses all defaults)", "[transforms][v2][params][json]") {
    std::string json = "{}";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(1.0f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(0.0f, 0.001f));
    REQUIRE(params.getExcludeHoles() == false);
}

TEST_CASE("MaskAreaParams - Reject negative scale_factor", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": -1.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation
}

TEST_CASE("MaskAreaParams - Reject zero scale_factor", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": 0.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation (ExclusiveMinimum<true>)
}

TEST_CASE("MaskAreaParams - Reject negative min_area", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "min_area": -5.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation
}

TEST_CASE("MaskAreaParams - Accept zero min_area", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "min_area": 0.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result); // Zero should be valid for min_area
}

TEST_CASE("MaskAreaParams - Reject invalid JSON", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": "not_a_number"
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result);
}

TEST_CASE("MaskAreaParams - Reject malformed JSON", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": 1.0,
        "invalid
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result);
}

// ============================================================================
// Tests: SumReductionParams JSON Loading
// ============================================================================

TEST_CASE("SumReductionParams - Load valid JSON with all fields", "[transforms][v2][params][json]") {
    std::string json = R"({
        "ignore_nan": true,
        "default_value": 42.5
    })";
    
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE(params.getIgnoreNan() == true);
    REQUIRE_THAT(params.getDefaultValue(), Catch::Matchers::WithinRel(42.5f, 0.001f));
}

TEST_CASE("SumReductionParams - Load empty JSON (uses defaults)", "[transforms][v2][params][json]") {
    std::string json = "{}";
    
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE(params.getIgnoreNan() == false);
    REQUIRE_THAT(params.getDefaultValue(), Catch::Matchers::WithinRel(0.0f, 0.001f));
}

TEST_CASE("SumReductionParams - Load with only ignore_nan", "[transforms][v2][params][json]") {
    std::string json = R"({
        "ignore_nan": true
    })";
    
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE(params.getIgnoreNan() == true);
    REQUIRE_THAT(params.getDefaultValue(), Catch::Matchers::WithinRel(0.0f, 0.001f));
}

TEST_CASE("SumReductionParams - Accept negative default_value", "[transforms][v2][params][json]") {
    std::string json = R"({
        "default_value": -100.0
    })";
    
    auto result = loadParametersFromJson<SumReductionParams>(json);
    
    REQUIRE(result); // Negative default_value is valid
    auto params = result.value();
    REQUIRE_THAT(params.getDefaultValue(), Catch::Matchers::WithinRel(-100.0f, 0.001f));
}

// ============================================================================
// Tests: JSON Round-Trip (Serialize/Deserialize)
// ============================================================================

TEST_CASE("MaskAreaParams - JSON round-trip preserves values", "[transforms][v2][params][json]") {
    MaskAreaParams original;
    original.scale_factor = 2.5f;
    original.min_area = 15.0f;
    original.exclude_holes = true;
    
    // Serialize
    std::string json = saveParametersToJson(original);
    
    // Deserialize
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    REQUIRE(result);
    auto recovered = result.value();
    
    // Verify values match
    REQUIRE_THAT(recovered.getScaleFactor(), Catch::Matchers::WithinRel(2.5f, 0.001f));
    REQUIRE_THAT(recovered.getMinArea(), Catch::Matchers::WithinRel(15.0f, 0.001f));
    REQUIRE(recovered.getExcludeHoles() == true);
}

TEST_CASE("SumReductionParams - JSON round-trip preserves values", "[transforms][v2][params][json]") {
    SumReductionParams original;
    original.ignore_nan = true;
    original.default_value = -50.0f;
    
    // Serialize
    std::string json = saveParametersToJson(original);
    
    // Deserialize
    auto result = loadParametersFromJson<SumReductionParams>(json);
    REQUIRE(result);
    auto recovered = result.value();
    
    // Verify values match
    REQUIRE(recovered.getIgnoreNan() == true);
    REQUIRE_THAT(recovered.getDefaultValue(), Catch::Matchers::WithinRel(-50.0f, 0.001f));
}

// ============================================================================
// Tests: Parameter Variant Loading
// ============================================================================

TEST_CASE("loadParameterVariant - Load MaskAreaParams by name", "[transforms][v2][params][json][variant]") {
    std::string json = R"({"scale_factor": 3.0})";
    
    auto variant = loadParameterVariant("CalculateMaskArea", json);
    
    REQUIRE(variant.has_value());
    REQUIRE(std::holds_alternative<MaskAreaParams>(variant.value()));
    
    auto params = std::get<MaskAreaParams>(variant.value());
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(3.0f, 0.001f));
}

TEST_CASE("loadParameterVariant - Load SumReductionParams by name", "[transforms][v2][params][json][variant]") {
    std::string json = R"({"ignore_nan": true})";
    
    auto variant = loadParameterVariant("SumReduction", json);
    
    REQUIRE(variant.has_value());
    REQUIRE(std::holds_alternative<SumReductionParams>(variant.value()));
    
    auto params = std::get<SumReductionParams>(variant.value());
    REQUIRE(params.getIgnoreNan() == true);
}

TEST_CASE("loadParameterVariant - Return nullopt for unknown transform", "[transforms][v2][params][json][variant]") {
    std::string json = "{}";
    
    auto variant = loadParameterVariant("UnknownTransform", json);
    
    REQUIRE(!variant.has_value());
}

TEST_CASE("loadParameterVariant - Return nullopt for invalid JSON", "[transforms][v2][params][json][variant]") {
    std::string json = R"({"scale_factor": -1.0})"; // Invalid: negative
    
    auto variant = loadParameterVariant("CalculateMaskArea", json);
    
    REQUIRE(!variant.has_value()); // Validation should fail
}
