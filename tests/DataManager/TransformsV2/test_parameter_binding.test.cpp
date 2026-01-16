/**
 * @file test_parameter_binding.test.cpp
 * @brief Unit tests for ParameterBinding utilities
 *
 * Tests cover:
 * 1. Templated binding application (compile-time type known)
 * 2. Type-erased binding application (runtime dispatch)
 * 3. Error handling for missing keys and type mismatches
 * 4. Registry-based binding applicator registration
 */

#include "transforms/v2/core/PipelineValueStore.hpp"
#include "transforms/v2/extension/ParameterBinding.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>
#include <string>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinRel;

// ============================================================================
// Test Parameter Structs
// ============================================================================

/**
 * @brief Simple test parameters for binding tests
 */
struct SimpleTestParams {
    float mean = 0.0f;
    float std_dev = 1.0f;
    int count = 0;
};

/**
 * @brief Parameters with nested types for complex binding tests
 */
struct ComplexTestParams {
    float threshold = 0.5f;
    int64_t alignment_time = 0;
    std::string label = "default";
    bool enabled = true;
};

// Register binding applicators for test params
namespace {
    [[maybe_unused]] auto const reg_simple = RegisterBindingApplicator<SimpleTestParams>();
    [[maybe_unused]] auto const reg_complex = RegisterBindingApplicator<ComplexTestParams>();
}

// ============================================================================
// Templated Binding Tests
// ============================================================================

TEST_CASE("ParameterBinding - Templated binding", "[transforms][v2][binding]") {
    PipelineValueStore store;
    store.set("computed_mean", 0.5f);
    store.set("computed_std", 0.1f);
    store.set("sample_count", 100);

    SECTION("Single binding") {
        SimpleTestParams base{.mean = 0.0f, .std_dev = 1.0f, .count = 0};
        std::map<std::string, std::string> bindings = {
            {"mean", "computed_mean"}
        };

        auto result = applyBindings(base, bindings, store);

        REQUIRE_THAT(result.mean, WithinRel(0.5f, 0.0001f));
        REQUIRE_THAT(result.std_dev, WithinRel(1.0f, 0.0001f));  // Unchanged
        REQUIRE(result.count == 0);  // Unchanged
    }

    SECTION("Multiple bindings") {
        SimpleTestParams base{.mean = 0.0f, .std_dev = 1.0f, .count = 0};
        std::map<std::string, std::string> bindings = {
            {"mean", "computed_mean"},
            {"std_dev", "computed_std"},
            {"count", "sample_count"}
        };

        auto result = applyBindings(base, bindings, store);

        REQUIRE_THAT(result.mean, WithinRel(0.5f, 0.0001f));
        REQUIRE_THAT(result.std_dev, WithinRel(0.1f, 0.0001f));
        REQUIRE(result.count == 100);
    }

    SECTION("Empty bindings returns base params unchanged") {
        SimpleTestParams base{.mean = 1.0f, .std_dev = 2.0f, .count = 3};
        std::map<std::string, std::string> bindings;

        auto result = applyBindings(base, bindings, store);

        REQUIRE_THAT(result.mean, WithinRel(1.0f, 0.0001f));
        REQUIRE_THAT(result.std_dev, WithinRel(2.0f, 0.0001f));
        REQUIRE(result.count == 3);
    }
}

TEST_CASE("ParameterBinding - Type conversions via JSON", "[transforms][v2][binding]") {
    PipelineValueStore store;
    store.set("int_value", 42);
    store.set("float_value", 3.14f);

    SECTION("Int from store binds to int field") {
        SimpleTestParams base{};
        std::map<std::string, std::string> bindings = {{"count", "int_value"}};

        auto result = applyBindings(base, bindings, store);
        REQUIRE(result.count == 42);
    }

    SECTION("Float from store binds to float field") {
        SimpleTestParams base{};
        std::map<std::string, std::string> bindings = {{"mean", "float_value"}};

        auto result = applyBindings(base, bindings, store);
        REQUIRE_THAT(result.mean, WithinRel(3.14f, 0.0001f));
    }

    SECTION("Int64 binds to int64_t field") {
        store.set("timestamp", int64_t{1000000});
        ComplexTestParams base{};
        std::map<std::string, std::string> bindings = {{"alignment_time", "timestamp"}};

        auto result = applyBindings(base, bindings, store);
        REQUIRE(result.alignment_time == 1000000);
    }

    SECTION("String binds to string field") {
        store.set("name", std::string("test_label"));
        ComplexTestParams base{};
        std::map<std::string, std::string> bindings = {{"label", "name"}};

        auto result = applyBindings(base, bindings, store);
        REQUIRE(result.label == "test_label");
    }
}

TEST_CASE("ParameterBinding - Error handling", "[transforms][v2][binding]") {
    PipelineValueStore store;
    store.set("existing", 1.0f);

    SECTION("Missing store key throws") {
        SimpleTestParams base{};
        std::map<std::string, std::string> bindings = {{"mean", "missing_key"}};

        REQUIRE_THROWS_AS(
            applyBindings(base, bindings, store),
            std::runtime_error);
    }

    SECTION("tryApplyBindings returns nullopt on error") {
        SimpleTestParams base{.mean = 5.0f};
        std::map<std::string, std::string> bindings = {{"mean", "missing_key"}};

        auto result = tryApplyBindings(base, bindings, store);
        
        REQUIRE_FALSE(result.has_value());
    }
}

// ============================================================================
// Type-Erased Binding Tests
// ============================================================================

TEST_CASE("ParameterBinding - Type-erased binding", "[transforms][v2][binding]") {
    PipelineValueStore store;
    store.set("computed_mean", 0.75f);
    store.set("computed_std", 0.25f);

    SECTION("Erased binding with registered type") {
        std::any base = SimpleTestParams{.mean = 0.0f, .std_dev = 1.0f, .count = 0};
        std::map<std::string, std::string> bindings = {
            {"mean", "computed_mean"},
            {"std_dev", "computed_std"}
        };

        auto result_any = applyBindingsErased(
            typeid(SimpleTestParams),
            base,
            bindings,
            store);

        auto result = std::any_cast<SimpleTestParams>(result_any);
        REQUIRE_THAT(result.mean, WithinRel(0.75f, 0.0001f));
        REQUIRE_THAT(result.std_dev, WithinRel(0.25f, 0.0001f));
    }

    SECTION("Empty bindings returns original") {
        std::any base = SimpleTestParams{.mean = 1.0f, .std_dev = 2.0f, .count = 3};
        std::map<std::string, std::string> bindings;

        auto result_any = applyBindingsErased(
            typeid(SimpleTestParams),
            base,
            bindings,
            store);

        auto result = std::any_cast<SimpleTestParams>(result_any);
        REQUIRE_THAT(result.mean, WithinRel(1.0f, 0.0001f));
    }

    SECTION("tryApplyBindingsErased returns original on error") {
        std::any base = SimpleTestParams{.mean = 5.0f, .std_dev = 6.0f, .count = 7};
        std::map<std::string, std::string> bindings = {{"mean", "missing"}};

        auto result_any = tryApplyBindingsErased(
            typeid(SimpleTestParams),
            base,
            bindings,
            store);

        // Should return original params on failure
        auto result = std::any_cast<SimpleTestParams>(result_any);
        REQUIRE_THAT(result.mean, WithinRel(5.0f, 0.0001f));
    }
}

// ============================================================================
// Registry Tests
// ============================================================================

TEST_CASE("ParameterBinding - Registry", "[transforms][v2][binding]") {

    SECTION("Registered types have applicators") {
        REQUIRE(hasBindingApplicator(typeid(SimpleTestParams)));
        REQUIRE(hasBindingApplicator(typeid(ComplexTestParams)));
    }

    SECTION("Unregistered types don't have applicators") {
        struct UnregisteredParams {
            int value = 0;
        };

        REQUIRE_FALSE(hasBindingApplicator(typeid(UnregisteredParams)));
    }

    SECTION("Applying to unregistered type throws") {
        struct UnregisteredParams {
            int value = 0;
        };

        std::any base = UnregisteredParams{.value = 1};
        std::map<std::string, std::string> bindings = {{"value", "key"}};
        PipelineValueStore store;
        store.set("key", 42);

        REQUIRE_THROWS_AS(
            applyBindingsErased(typeid(UnregisteredParams), base, bindings, store),
            std::runtime_error);
    }
}

// ============================================================================
// Complex Params Tests
// ============================================================================

TEST_CASE("ParameterBinding - Complex params", "[transforms][v2][binding]") {
    PipelineValueStore store;
    store.set("thresh", 0.75f);
    store.set("time", int64_t{5000});
    store.set("name", std::string("custom"));

    SECTION("Mixed type bindings") {
        ComplexTestParams base{
            .threshold = 0.5f,
            .alignment_time = 0,
            .label = "default",
            .enabled = true
        };

        std::map<std::string, std::string> bindings = {
            {"threshold", "thresh"},
            {"alignment_time", "time"},
            {"label", "name"}
        };

        auto result = applyBindings(base, bindings, store);

        REQUIRE_THAT(result.threshold, WithinRel(0.75f, 0.0001f));
        REQUIRE(result.alignment_time == 5000);
        REQUIRE(result.label == "custom");
        REQUIRE(result.enabled == true);  // Unchanged
    }

    SECTION("Partial binding preserves unbound fields") {
        ComplexTestParams base{
            .threshold = 0.1f,
            .alignment_time = 100,
            .label = "original",
            .enabled = false
        };

        std::map<std::string, std::string> bindings = {
            {"threshold", "thresh"}
        };

        auto result = applyBindings(base, bindings, store);

        REQUIRE_THAT(result.threshold, WithinRel(0.75f, 0.0001f));  // Bound
        REQUIRE(result.alignment_time == 100);  // Preserved
        REQUIRE(result.label == "original");    // Preserved
        REQUIRE(result.enabled == false);       // Preserved
    }
}
