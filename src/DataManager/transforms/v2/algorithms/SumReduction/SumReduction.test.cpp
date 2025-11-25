
#include "transforms/v2/core/ParameterIO.hpp"
#include "SumReduction.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::Transforms::V2::Examples;

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
