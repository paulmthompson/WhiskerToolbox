/**
 * @file AutoParamWidget.test.cpp
 * @brief Tests for AutoParamWidget dynamic field support (Phase 0.2).
 *
 * Tests the dynamic_combo, include_none_sentinel, updateAllowedValues(),
 * and updateVariantAlternatives() extensions.
 */

#include "AutoParamWidget/AutoParamWidget.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QStackedWidget>

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>

// ============================================================================
// QApplication guard — ensures a QApplication exists for widget construction
// ============================================================================

namespace {

struct QtAppGuard {
    QtAppGuard() {
        if (QApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            // Intentionally leaked to avoid destruction-order issues with Catch2
            new QApplication(argc, const_cast<char **>(argv));
        }
    }
};

QtAppGuard const s_guard;

}// anonymous namespace

// ============================================================================
// Test parameter structs with dynamic_combo support
// ============================================================================

namespace test_dynamic {

/// A struct with a dynamic combo field (populated at runtime).
struct DynamicSourceParams {
    std::string source;
    int offset = 0;
};

/// A struct with a variant for testing updateVariantAlternatives.
struct OptionA {
    float alpha = 1.0f;
};

struct OptionB {
    int count = 5;
};

struct OptionC {
    bool flag = true;
};

using TestVariant = rfl::TaggedUnion<"kind", OptionA, OptionB, OptionC>;

struct VariantTestParams {
    TestVariant selection = OptionA{};
    std::string label;
};

/// A struct with a dynamic combo inside a variant alternative.
struct InnerDynamic {
    std::string data_key;
    int frame = 0;
};

struct InnerStatic {
    float weight = 0.5f;
};

using InnerVariant = rfl::TaggedUnion<"mode", InnerDynamic, InnerStatic>;

struct NestedDynamicParams {
    InnerVariant inner = InnerDynamic{};
};

}// namespace test_dynamic

// ============================================================================
// ParameterUIHints specializations
// ============================================================================

template<>
struct ParameterUIHints<test_dynamic::DynamicSourceParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("source")) {
            f->display_name = "Data Source";
            f->dynamic_combo = true;
            f->include_none_sentinel = true;
            f->tooltip = "Select a data source";
        }
    }
};

template<>
struct ParameterUIHints<test_dynamic::InnerDynamic> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("data_key")) {
            f->dynamic_combo = true;
            f->include_none_sentinel = true;
        }
    }
};

// ============================================================================
// Tests: dynamic_combo creates QComboBox instead of QLineEdit
// ============================================================================

TEST_CASE("dynamic_combo creates QComboBox for string field",
          "[auto_param_widget][dynamic_combo]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    auto * source_field = schema.field("source");
    REQUIRE(source_field != nullptr);
    CHECK(source_field->dynamic_combo);
    CHECK(source_field->include_none_sentinel);

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Verify the source field is a QComboBox (via toJson)
    auto json = widget.toJson();
    // With "(None)" sentinel and empty → toJson maps "(None)" to ""
    CHECK(json.find("\"source\":\"\"") != std::string::npos);
}

TEST_CASE("dynamic_combo without sentinel creates empty combo",
          "[auto_param_widget][dynamic_combo]") {
    // Build a schema manually with dynamic_combo but no sentinel
    ParameterSchema schema;
    ParameterFieldDescriptor desc;
    desc.name = "key";
    desc.type_name = "std::string";
    desc.display_name = "Key";
    desc.dynamic_combo = true;
    desc.include_none_sentinel = false;
    desc.default_value_json = "\"\"";
    schema.fields.push_back(std::move(desc));

    AutoParamWidget widget;
    widget.setSchema(schema);

    auto json = widget.toJson();
    // Empty combo with no items — current text is empty
    CHECK(json.find("\"key\":\"\"") != std::string::npos);
}

// ============================================================================
// Tests: include_none_sentinel prepends "(None)" and maps to empty in JSON
// ============================================================================

TEST_CASE("include_none_sentinel maps (None) to empty string in toJson",
          "[auto_param_widget][sentinel]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Default: (None) selected → toJson should produce empty source
    auto json = widget.toJson();
    CHECK(json.find("\"source\":\"\"") != std::string::npos);
}

TEST_CASE("fromJson with empty string selects (None) sentinel",
          "[auto_param_widget][sentinel]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Populate some values first
    widget.updateAllowedValues("source", {"key1", "key2"});

    // Load JSON with empty source → should select "(None)"
    widget.fromJson(R"({"source":"","offset":3})");

    auto json = widget.toJson();
    CHECK(json.find("\"source\":\"\"") != std::string::npos);
    CHECK(json.find("\"offset\":3") != std::string::npos);
}

// ============================================================================
// Tests: updateAllowedValues
// ============================================================================

TEST_CASE("updateAllowedValues populates combo items",
          "[auto_param_widget][update_allowed]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    widget.updateAllowedValues("source", {"alpha", "beta", "gamma"});

    auto json = widget.toJson();
    // "(None)" is selected by default since previous was "(None)"
    CHECK(json.find("\"source\":\"\"") != std::string::npos);
}

TEST_CASE("updateAllowedValues preserves existing selection",
          "[auto_param_widget][update_allowed]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Set initial values and select "beta"
    widget.updateAllowedValues("source", {"alpha", "beta", "gamma"});
    widget.fromJson(R"({"source":"beta","offset":0})");

    // Re-populate with a superset — "beta" should still be selected
    widget.updateAllowedValues("source", {"alpha", "beta", "gamma", "delta"});

    auto json = widget.toJson();
    CHECK(json.find("\"source\":\"beta\"") != std::string::npos);
}

TEST_CASE("updateAllowedValues switches to first when previous removed",
          "[auto_param_widget][update_allowed]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Select "beta"
    widget.updateAllowedValues("source", {"alpha", "beta"});
    widget.fromJson(R"({"source":"beta","offset":0})");

    // Remove "beta" from allowed values
    widget.updateAllowedValues("source", {"alpha", "gamma"});

    auto json = widget.toJson();
    // Should fall back to "(None)" (index 0 with sentinel)
    CHECK(json.find("\"source\":\"\"") != std::string::npos);
}

TEST_CASE("updateAllowedValues is no-op for non-combo field",
          "[auto_param_widget][update_allowed]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // "offset" is an int field — updateAllowedValues should be a no-op
    widget.updateAllowedValues("offset", {"1", "2", "3"});

    auto json = widget.toJson();
    CHECK(json.find("\"offset\":0") != std::string::npos);
}

// ============================================================================
// Tests: updateVariantAlternatives
// ============================================================================

TEST_CASE("updateVariantAlternatives filters visible alternatives",
          "[auto_param_widget][update_variant]") {
    auto schema = extractParameterSchema<test_dynamic::VariantTestParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Initially all three alternatives should be available
    auto json_before = widget.toJson();
    CHECK(json_before.find("\"kind\":\"OptionA\"") != std::string::npos);

    // Restrict to OptionA and OptionC only
    widget.updateVariantAlternatives("selection", {"OptionA", "OptionC"});

    // Should still have OptionA selected (it was the current one and is allowed)
    auto json_after = widget.toJson();
    CHECK(json_after.find("\"kind\":\"OptionA\"") != std::string::npos);
}

TEST_CASE("updateVariantAlternatives switches selection when current is filtered out",
          "[auto_param_widget][update_variant]") {
    auto schema = extractParameterSchema<test_dynamic::VariantTestParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Select OptionB
    widget.fromJson(R"({"selection":{"kind":"OptionB","count":10},"label":""})");

    // Restrict to OptionA only — OptionB should be filtered out
    widget.updateVariantAlternatives("selection", {"OptionA"});

    auto json = widget.toJson();
    CHECK(json.find("\"kind\":\"OptionA\"") != std::string::npos);
}

TEST_CASE("updateVariantAlternatives restores full set",
          "[auto_param_widget][update_variant]") {
    auto schema = extractParameterSchema<test_dynamic::VariantTestParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Restrict to one
    widget.updateVariantAlternatives("selection", {"OptionB"});
    auto json1 = widget.toJson();
    CHECK(json1.find("\"kind\":\"OptionB\"") != std::string::npos);

    // Restore all
    widget.updateVariantAlternatives("selection", {"OptionA", "OptionB", "OptionC"});

    // Should still be on OptionB since it's still allowed
    auto json2 = widget.toJson();
    CHECK(json2.find("\"kind\":\"OptionB\"") != std::string::npos);
}

// ============================================================================
// Tests: updateAllowedValues inside variant sub-rows
// ============================================================================

TEST_CASE("updateAllowedValues works for variant sub-row fields",
          "[auto_param_widget][update_allowed][variant]") {
    auto schema = extractParameterSchema<test_dynamic::NestedDynamicParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // InnerDynamic has data_key marked as dynamic_combo
    widget.updateAllowedValues("data_key", {"video1", "video2"});

    // The inner variant should be InnerDynamic by default
    auto json = widget.toJson();
    // data_key should be "(None)" → "" since that's the sentinel default
    CHECK(json.find("\"data_key\":\"\"") != std::string::npos);
}

// ============================================================================
// Tests: JSON round-trip with dynamic combo
// ============================================================================

TEST_CASE("JSON round-trip preserves dynamic combo selection",
          "[auto_param_widget][roundtrip]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    AutoParamWidget widget;
    widget.setSchema(schema);

    // Populate and select a value
    widget.updateAllowedValues("source", {"key1", "key2", "key3"});
    widget.fromJson(R"({"source":"key2","offset":5})");

    auto json = widget.toJson();
    CHECK(json.find("\"source\":\"key2\"") != std::string::npos);
    CHECK(json.find("\"offset\":5") != std::string::npos);

    // Round-trip: read back
    AutoParamWidget widget2;
    widget2.setSchema(schema);
    widget2.updateAllowedValues("source", {"key1", "key2", "key3"});
    widget2.fromJson(json);

    auto json2 = widget2.toJson();
    CHECK(json == json2);
}

// ============================================================================
// Tests: ParameterFieldDescriptor flags
// ============================================================================

TEST_CASE("ParameterFieldDescriptor dynamic_combo defaults to false",
          "[parameter_schema][dynamic_combo]") {
    ParameterFieldDescriptor const desc;
    CHECK_FALSE(desc.dynamic_combo);
    CHECK_FALSE(desc.include_none_sentinel);
}

TEST_CASE("extractParameterSchema with ParameterUIHints sets dynamic_combo",
          "[parameter_schema][dynamic_combo]") {
    auto schema = extractParameterSchema<test_dynamic::DynamicSourceParams>();

    auto * source = schema.field("source");
    REQUIRE(source != nullptr);
    CHECK(source->dynamic_combo);
    CHECK(source->include_none_sentinel);
    CHECK(source->tooltip == "Select a data source");
    CHECK(source->display_name == "Data Source");

    auto * offset = schema.field("offset");
    REQUIRE(offset != nullptr);
    CHECK_FALSE(offset->dynamic_combo);
    CHECK_FALSE(offset->include_none_sentinel);
}
