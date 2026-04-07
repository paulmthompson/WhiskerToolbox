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
    CHECK(snakeCaseToDisplay("").empty());
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

    auto * method = schema.field("method");
    REQUIRE(method != nullptr);
    CHECK(method->display_name == "Method");


    auto * poly_order = schema.field("polynomial_order");
    REQUIRE(poly_order != nullptr);
    CHECK(poly_order->display_name == "Polynomial Order");
    CHECK(poly_order->type_name == "int");

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

// ============================================================================
// Tests: Phase 1 — Enum class auto-detection
// ============================================================================

// Enum and struct must be in a named namespace for reflect-cpp's compile-time
// enum name extraction (anonymous namespaces break __PRETTY_FUNCTION__ parsing).
namespace test_enum {

/// Test enum for schema extraction
enum class TestMode { fast,
                      balanced,
                      accurate };

/// Test struct with required and optional enum fields alongside a plain field
struct TestEnumParams {
    TestMode mode = TestMode::fast;
    std::optional<TestMode> alt_mode;
    float threshold = 0.5f;
};

}// namespace test_enum

using test_enum::TestEnumParams;
using test_enum::TestMode;

TEST_CASE("extractParameterSchema - enum class auto-detection", "[transforms][v2][schema][enum]") {
    auto schema = extractParameterSchema<TestEnumParams>();

    REQUIRE(schema.fields.size() == 3);

    SECTION("Required enum field") {
        auto * mode = schema.field("mode");
        REQUIRE(mode != nullptr);
        CHECK(mode->type_name == "enum");
        CHECK(mode->display_name == "Mode");
        CHECK_FALSE(mode->is_optional);
        REQUIRE(mode->allowed_values.size() == 3);
        CHECK(mode->allowed_values[0] == "fast");
        CHECK(mode->allowed_values[1] == "balanced");
        CHECK(mode->allowed_values[2] == "accurate");
    }

    SECTION("Optional enum field") {
        auto * alt = schema.field("alt_mode");
        REQUIRE(alt != nullptr);
        CHECK(alt->type_name == "enum");
        CHECK(alt->display_name == "Alt Mode");
        CHECK(alt->is_optional);
        REQUIRE(alt->allowed_values.size() == 3);
        CHECK(alt->allowed_values[0] == "fast");
        CHECK(alt->allowed_values[1] == "balanced");
        CHECK(alt->allowed_values[2] == "accurate");
    }

    SECTION("Non-enum fields are unaffected") {
        auto * thresh = schema.field("threshold");
        REQUIRE(thresh != nullptr);
        CHECK(thresh->type_name == "float");
        CHECK(thresh->allowed_values.empty());
    }
}

TEST_CASE("extractParameterSchema - enum default value is JSON string", "[transforms][v2][schema][enum]") {
    auto schema = extractParameterSchema<TestEnumParams>();

    auto * mode = schema.field("mode");
    REQUIRE(mode != nullptr);
    REQUIRE(mode->default_value_json.has_value());
    CHECK(mode->default_value_json.value() == "\"fast\"");
}

TEST_CASE("extractParameterSchema - enum JSON round-trip via reflect-cpp", "[transforms][v2][schema][enum]") {
    TestEnumParams params;
    params.mode = TestMode::accurate;
    params.alt_mode = TestMode::balanced;
    params.threshold = 0.75f;

    auto json = rfl::json::write(params);
    auto result = rfl::json::read<TestEnumParams>(json);

    REQUIRE(result);
    CHECK(result.value().mode == TestMode::accurate);
    REQUIRE(result.value().alt_mode.has_value());
    CHECK(result.value().alt_mode.value() == TestMode::balanced);
    CHECK_THAT(result.value().threshold, Catch::Matchers::WithinAbs(0.75, 0.001));
}

TEST_CASE("extractParameterSchema - enum ParameterUIHints still work", "[transforms][v2][schema][enum]") {
    // Verify that manual ParameterUIHints on string fields still work alongside enum auto-detection
    auto schema = extractParameterSchema<TestHintParams>();

    auto * mode = schema.field("mode");
    REQUIRE(mode != nullptr);
    // TestHintParams::mode is std::optional<std::string> with allowed_values set via UIHints
    CHECK(mode->allowed_values.size() == 3);
    CHECK(mode->allowed_values[0] == "fast");
    // type_name should remain "std::string" since it's a string, not an enum
    CHECK(mode->type_name == "std::string");
}

// ============================================================================
// Tests: Phase 2 — Tagged variant (rfl::TaggedUnion) auto-detection
// ============================================================================

namespace test_variant {

struct LinearMotionParams {
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;
};

struct SinusoidalMotionParams {
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 1.0f;
    float frequency_y = 1.0f;
    float phase_x = 0.0f;
    float phase_y = 0.0f;
};

struct BrownianMotionParams {
    float diffusion = 1.0f;
    int seed = 42;
};

using MotionVariant = rfl::TaggedUnion<
        "model",
        LinearMotionParams,
        SinusoidalMotionParams,
        BrownianMotionParams>;

struct MovingPointParams {
    float start_x = 100.0f;
    float start_y = 100.0f;
    int num_frames = 100;
    MotionVariant motion = LinearMotionParams{};
};

}// namespace test_variant

using test_variant::BrownianMotionParams;
using test_variant::LinearMotionParams;
using test_variant::MotionVariant;
using test_variant::MovingPointParams;
using test_variant::SinusoidalMotionParams;

TEST_CASE("extractParameterSchema - TaggedUnion auto-detection", "[transforms][v2][schema][variant]") {
    auto schema = extractParameterSchema<MovingPointParams>();

    REQUIRE(schema.fields.size() == 4);

    SECTION("Non-variant fields are unaffected") {
        auto * sx = schema.field("start_x");
        REQUIRE(sx != nullptr);
        CHECK(sx->type_name == "float");
        CHECK_FALSE(sx->is_variant);
        CHECK(sx->variant_alternatives.empty());

        auto * nf = schema.field("num_frames");
        REQUIRE(nf != nullptr);
        CHECK(nf->type_name == "int");
    }

    SECTION("Variant field is detected") {
        auto * motion = schema.field("motion");
        REQUIRE(motion != nullptr);
        CHECK(motion->type_name == "variant");
        CHECK(motion->is_variant);
        CHECK(motion->variant_discriminator == "model");
        CHECK(motion->display_name == "Motion");
    }

    SECTION("Variant field has correct alternatives") {
        auto * motion = schema.field("motion");
        REQUIRE(motion != nullptr);
        REQUIRE(motion->variant_alternatives.size() == 3);

        // Check tags match struct names
        CHECK(motion->variant_alternatives[0].tag == "LinearMotionParams");
        CHECK(motion->variant_alternatives[1].tag == "SinusoidalMotionParams");
        CHECK(motion->variant_alternatives[2].tag == "BrownianMotionParams");

        // allowed_values should match tags
        REQUIRE(motion->allowed_values.size() == 3);
        CHECK(motion->allowed_values[0] == "LinearMotionParams");
        CHECK(motion->allowed_values[1] == "SinusoidalMotionParams");
        CHECK(motion->allowed_values[2] == "BrownianMotionParams");
    }

    SECTION("Alternative sub-schemas have correct fields") {
        auto * motion = schema.field("motion");
        REQUIRE(motion != nullptr);
        REQUIRE(motion->variant_alternatives.size() == 3);

        // Linear: velocity_x, velocity_y
        auto const & linear = *motion->variant_alternatives[0].schema;
        CHECK(linear.fields.size() == 2);
        CHECK(linear.field("velocity_x") != nullptr);
        CHECK(linear.field("velocity_y") != nullptr);
        CHECK(linear.field("velocity_x")->type_name == "float");

        // Sinusoidal: 6 float fields
        auto const & sinusoidal = *motion->variant_alternatives[1].schema;
        CHECK(sinusoidal.fields.size() == 6);
        CHECK(sinusoidal.field("amplitude_x") != nullptr);
        CHECK(sinusoidal.field("frequency_x") != nullptr);
        CHECK(sinusoidal.field("phase_y") != nullptr);

        // Brownian: diffusion (float), seed (int)
        auto const & brownian = *motion->variant_alternatives[2].schema;
        CHECK(brownian.fields.size() == 2);
        auto * diff = brownian.field("diffusion");
        REQUIRE(diff != nullptr);
        CHECK(diff->type_name == "float");
        auto * s = brownian.field("seed");
        REQUIRE(s != nullptr);
        CHECK(s->type_name == "int");
    }

    SECTION("Alternative sub-schemas have default values") {
        auto * motion = schema.field("motion");
        REQUIRE(motion != nullptr);

        auto const & linear = *motion->variant_alternatives[0].schema;
        auto * vx = linear.field("velocity_x");
        REQUIRE(vx != nullptr);
        REQUIRE(vx->default_value_json.has_value());
        // Default is 1.0f
        auto def_result = rfl::json::read<rfl::Generic>(vx->default_value_json.value());
        REQUIRE(def_result);
        auto const * d = std::get_if<double>(&def_result.value().get());
        REQUIRE(d != nullptr);
        CHECK_THAT(*d, Catch::Matchers::WithinAbs(1.0, 0.001));
    }
}

TEST_CASE("extractParameterSchema - TaggedUnion default value JSON", "[transforms][v2][schema][variant]") {
    auto schema = extractParameterSchema<MovingPointParams>();

    auto * motion = schema.field("motion");
    REQUIRE(motion != nullptr);
    REQUIRE(motion->default_value_json.has_value());

    // Default-constructed MotionVariant holds the first alternative (LinearMotionParams)
    auto def_result = rfl::json::read<rfl::Generic>(motion->default_value_json.value());
    REQUIRE(def_result);
    auto const * obj = std::get_if<rfl::Generic::Object>(&def_result.value().get());
    REQUIRE(obj != nullptr);

    // Should contain the discriminator field
    auto model_val = obj->get("model");
    REQUIRE(model_val);
    auto const * model_str = std::get_if<std::string>(&model_val.value().get());
    REQUIRE(model_str != nullptr);
    CHECK(*model_str == "LinearMotionParams");
}

TEST_CASE("TaggedUnion JSON round-trip via reflect-cpp", "[transforms][v2][schema][variant]") {
    SECTION("Default construction") {
        MovingPointParams const params;
        auto json = rfl::json::write(params);
        auto result = rfl::json::read<MovingPointParams>(json);
        REQUIRE(result);
        CHECK_THAT(result.value().start_x, Catch::Matchers::WithinAbs(100.0, 0.001));
        CHECK(result.value().num_frames == 100);
    }

    SECTION("With sinusoidal alternative") {
        MovingPointParams params;
        params.start_x = 50.0f;
        params.motion = SinusoidalMotionParams{.amplitude_x = 10.0f, .amplitude_y = 5.0f};

        auto json = rfl::json::write(params);
        auto result = rfl::json::read<MovingPointParams>(json);
        REQUIRE(result);
        CHECK_THAT(result.value().start_x, Catch::Matchers::WithinAbs(50.0, 0.001));
    }

    SECTION("With brownian alternative") {
        MovingPointParams params;
        params.motion = BrownianMotionParams{.diffusion = 2.5f, .seed = 99};

        auto json = rfl::json::write(params);
        auto result = rfl::json::read<MovingPointParams>(json);
        REQUIRE(result);
    }
}
