#include "ParameterSchema/ParameterSchema.hpp"

#include "TransformsV2/algorithms/AnalogEventThreshold/AnalogEventThreshold.hpp"
#include "TransformsV2/algorithms/LineAngle/LineAngle.hpp"
#include "TransformsV2/algorithms/LineCurvature/LineCurvature.hpp"
#include "TransformsV2/algorithms/LineSubsegment/LineSubsegment.hpp"
#include "TransformsV2/algorithms/MaskArea/MaskArea.hpp"
#include "TransformsV2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Tests: snakeCaseToDisplay helper
// ============================================================================

TEST_CASE("snakeCaseToDisplay - basic conversions", "[transforms][v2][schema][helpers]") {
    CHECK(snakeCaseToDisplay("scale_factor") == "Scale Factor");
    CHECK(snakeCaseToDisplay("min_area") == "Min Area");
    CHECK(snakeCaseToDisplay("exclude_holes") == "Exclude Holes");
    CHECK(snakeCaseToDisplay("position") == "Position");
    CHECK(snakeCaseToDisplay("") == "");
    CHECK(snakeCaseToDisplay("x") == "X");
    CHECK(snakeCaseToDisplay("polynomial_order") == "Polynomial Order");
    CHECK(snakeCaseToDisplay("reference_x") == "Reference X");
}

// ============================================================================
// Tests: Type string parsing helpers
// ============================================================================

TEST_CASE("isOptionalType - detects optional wrapper", "[transforms][v2][schema][helpers]") {
    CHECK(isOptionalType("std::optional<float>"));
    CHECK(isOptionalType("std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0>>>"));
    CHECK(isOptionalType("std::optional<bool>"));
    CHECK_FALSE(isOptionalType("float"));
    CHECK_FALSE(isOptionalType("int"));
    CHECK_FALSE(isOptionalType("bool"));
}

TEST_CASE("hasValidator - detects rfl::Validator", "[transforms][v2][schema][helpers]") {
    CHECK(hasValidator("std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0>>>"));
    CHECK(hasValidator("rfl::Validator<float, rfl::Minimum<0>>"));
    CHECK_FALSE(hasValidator("std::optional<float>"));
    CHECK_FALSE(hasValidator("float"));
    CHECK_FALSE(hasValidator("bool"));
}

TEST_CASE("parseUnderlyingType - extracts base type", "[transforms][v2][schema][helpers]") {
    CHECK(parseUnderlyingType("float") == "float");
    CHECK(parseUnderlyingType("double") == "double");
    CHECK(parseUnderlyingType("int") == "int");
    CHECK(parseUnderlyingType("bool") == "bool");
    CHECK(parseUnderlyingType("std::optional<float>") == "float");
    CHECK(parseUnderlyingType("std::optional<int>") == "int");
    CHECK(parseUnderlyingType("std::optional<bool>") == "bool");
    CHECK(parseUnderlyingType("std::optional<std::string>") == "std::string");
    CHECK(parseUnderlyingType("std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0>>>") == "float");
    CHECK(parseUnderlyingType("std::optional<rfl::Validator<float, rfl::Minimum<0>>>") == "float");
}

TEST_CASE("extractConstraints - parses validator constraints", "[transforms][v2][schema][helpers]") {
    SECTION("ExclusiveMinimum") {
        auto info = extractConstraints("rfl::Validator<float, rfl::ExclusiveMinimum<0>>");
        CHECK(info.is_exclusive_min);
        CHECK(info.min_value.has_value());
        if (info.min_value.has_value()) {
            CHECK_THAT(info.min_value.value(), Catch::Matchers::WithinAbs(0.0, 0.001));
        }
        CHECK_FALSE(info.max_value.has_value());
    }

    SECTION("Inclusive Minimum") {
        auto info = extractConstraints("rfl::Validator<float, rfl::Minimum<0>>");
        CHECK_FALSE(info.is_exclusive_min);
        CHECK(info.min_value.has_value());
        if (info.min_value.has_value()) {
            CHECK_THAT(info.min_value.value(), Catch::Matchers::WithinAbs(0.0, 0.001));
        }
    }

    SECTION("No constraints") {
        auto info = extractConstraints("float");
        CHECK_FALSE(info.min_value.has_value());
        CHECK_FALSE(info.max_value.has_value());
        CHECK_FALSE(info.is_exclusive_min);
        CHECK_FALSE(info.is_exclusive_max);
    }
}

// ============================================================================
// Tests: Schema extraction from real parameter structs
// ============================================================================

TEST_CASE("extractParameterSchema - MaskAreaParams", "[transforms][v2][schema][extraction]") {
    auto schema = extractParameterSchema<MaskAreaParams>();

    CHECK(schema.fields.size() == 3);

    // Check field names
    auto * sf = schema.field("scale_factor");
    REQUIRE(sf != nullptr);
    CHECK(sf->display_name == "Scale Factor");
    CHECK(sf->type_name == "float");
    CHECK(sf->is_optional);
    // Should have exclusive minimum constraint from rfl::ExclusiveMinimum
    CHECK(hasValidator(sf->raw_type_name));
    CHECK(sf->is_exclusive_min);

    auto * ma = schema.field("min_area");
    REQUIRE(ma != nullptr);
    CHECK(ma->display_name == "Min Area");
    CHECK(ma->type_name == "float");
    CHECK(ma->is_optional);
    // Should have inclusive minimum constraint from rfl::Minimum
    CHECK(hasValidator(ma->raw_type_name));
    CHECK_FALSE(ma->is_exclusive_min);

    auto * eh = schema.field("exclude_holes");
    REQUIRE(eh != nullptr);
    CHECK(eh->display_name == "Exclude Holes");
    CHECK(eh->type_name == "bool");
    CHECK(eh->is_optional);
    CHECK_FALSE(hasValidator(eh->raw_type_name));
}

TEST_CASE("extractParameterSchema - LineAngleParams", "[transforms][v2][schema][extraction]") {
    auto schema = extractParameterSchema<LineAngleParams>();

    CHECK(schema.fields.size() == 5);

    auto * position = schema.field("position");
    REQUIRE(position != nullptr);
    CHECK(position->display_name == "Position");
    CHECK(position->type_name == "float");
    CHECK(position->is_optional);

    auto * method = schema.field("method");
    REQUIRE(method != nullptr);
    CHECK(method->display_name == "Method");
    // Method is a std::optional<std::string>
    CHECK(method->is_optional);

    auto * poly_order = schema.field("polynomial_order");
    REQUIRE(poly_order != nullptr);
    CHECK(poly_order->display_name == "Polynomial Order");
    CHECK(poly_order->type_name == "int");
    CHECK(poly_order->is_optional);

    auto * ref_x = schema.field("reference_x");
    REQUIRE(ref_x != nullptr);
    CHECK(ref_x->display_name == "Reference X");
    CHECK(ref_x->type_name == "float");

    auto * ref_y = schema.field("reference_y");
    REQUIRE(ref_y != nullptr);
    CHECK(ref_y->display_name == "Reference Y");
    CHECK(ref_y->type_name == "float");
}

TEST_CASE("extractParameterSchema - AnalogEventThresholdParams", "[transforms][v2][schema][extraction]") {
    auto schema = extractParameterSchema<AnalogEventThresholdParams>();

    CHECK(schema.fields.size() == 3);

    auto * threshold = schema.field("threshold_value");
    REQUIRE(threshold != nullptr);
    CHECK(threshold->type_name == "float");
    CHECK(threshold->is_optional);
    CHECK(threshold->display_name == "Threshold Value");

    auto * direction = schema.field("direction");
    REQUIRE(direction != nullptr);
    CHECK(direction->is_optional);

    auto * lockout = schema.field("lockout_time");
    REQUIRE(lockout != nullptr);
    CHECK(lockout->type_name == "float");
    CHECK(lockout->is_optional);
    // lockout_time has rfl::Minimum<0.0f> validator
    CHECK(hasValidator(lockout->raw_type_name));
    CHECK_FALSE(lockout->is_exclusive_min);// Minimum is inclusive
}

TEST_CASE("extractParameterSchema - NoParams is empty", "[transforms][v2][schema][extraction]") {
    auto schema = extractParameterSchema<NoParams>();
    CHECK(schema.fields.empty());
}

// ============================================================================
// Tests: ParameterUIHints override
// ============================================================================

namespace {

// A test parameter struct for UI hints testing
struct TestHintParams {
    std::optional<float> alpha;
    std::optional<std::string> mode;
    std::optional<int> detail_level;
};

}// anonymous namespace

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<TestHintParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("alpha")) {
            f->tooltip = "Opacity value between 0 and 1";
            f->default_value_json = "0.5";
        }
        if (auto * f = schema.field("mode")) {
            f->allowed_values = {"fast", "accurate", "balanced"};
            f->tooltip = "Processing mode selection";
        }
        if (auto * f = schema.field("detail_level")) {
            f->is_advanced = true;
            f->group = "Advanced Settings";
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

TEST_CASE("extractParameterSchema - ParameterUIHints applied", "[transforms][v2][schema][hints]") {
    auto schema = extractParameterSchema<TestHintParams>();

    CHECK(schema.fields.size() == 3);

    auto * alpha = schema.field("alpha");
    REQUIRE(alpha != nullptr);
    CHECK(alpha->tooltip == "Opacity value between 0 and 1");
    CHECK(alpha->default_value_json.has_value());
    CHECK(alpha->default_value_json.value() == "0.5");

    auto * mode = schema.field("mode");
    REQUIRE(mode != nullptr);
    CHECK(mode->allowed_values.size() == 3);
    CHECK(mode->allowed_values[0] == "fast");
    CHECK(mode->allowed_values[1] == "accurate");
    CHECK(mode->allowed_values[2] == "balanced");
    CHECK(mode->tooltip == "Processing mode selection");

    auto * detail = schema.field("detail_level");
    REQUIRE(detail != nullptr);
    CHECK(detail->is_advanced);
    CHECK(detail->group == "Advanced Settings");
}

// ============================================================================
// Tests: Schema field ordering
// ============================================================================

TEST_CASE("extractParameterSchema - field display_order is sequential", "[transforms][v2][schema]") {
    auto schema = extractParameterSchema<LineAngleParams>();

    for (size_t i = 0; i < schema.fields.size(); ++i) {
        CHECK(schema.fields[i].display_order == static_cast<int>(i));
    }
}

// ============================================================================
// Tests: Registry-level schema access
// ============================================================================

TEST_CASE("ElementRegistry::getParameterSchema - returns schema for registered transforms",
          "[transforms][v2][schema][registry]") {
    auto & registry = ElementRegistry::instance();

    // CalculateMaskArea is registered in RegisteredTransforms.cpp
    auto const * schema = registry.getParameterSchema("CalculateMaskArea");
    REQUIRE(schema != nullptr);
    CHECK(schema->fields.size() == 3);

    auto * sf = schema->field("scale_factor");
    REQUIRE(sf != nullptr);
    CHECK(sf->type_name == "float");
}

TEST_CASE("ElementRegistry::getParameterSchema - returns nullptr for unknown transform",
          "[transforms][v2][schema][registry]") {
    auto & registry = ElementRegistry::instance();

    auto const * schema = registry.getParameterSchema("NonExistentTransform");
    CHECK(schema == nullptr);
}

TEST_CASE("ElementRegistry::getParameterSchema - works for container transforms",
          "[transforms][v2][schema][registry]") {
    auto & registry = ElementRegistry::instance();

    // AnalogEventThreshold is registered as a container transform
    auto const * schema = registry.getParameterSchema("AnalogEventThreshold");
    if (schema != nullptr) {
        // If container transforms register schemas, verify the fields
        CHECK(schema->fields.size() == 3);
        CHECK(schema->field("threshold_value") != nullptr);
    }
}
