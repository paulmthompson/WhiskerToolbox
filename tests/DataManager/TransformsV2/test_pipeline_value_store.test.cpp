/**
 * @file test_pipeline_value_store.test.cpp
 * @brief Unit tests for PipelineValueStore
 *
 * Tests cover:
 * 1. Type-safe value storage and retrieval
 * 2. JSON serialization for binding
 * 3. Type conversions
 * 4. Merge and clear operations
 * 5. Query methods
 */

#include "transforms/v2/core/PipelineValueStore.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinRel;

// ============================================================================
// Type-Safe Storage Tests
// ============================================================================

TEST_CASE("PipelineValueStore - Float storage", "[transforms][v2][value_store]") {
    PipelineValueStore store;

    SECTION("Store and retrieve float") {
        store.set("mean", 0.5f);
        
        REQUIRE(store.contains("mean"));
        auto value = store.getFloat("mean");
        REQUIRE(value.has_value());
        REQUIRE_THAT(*value, WithinRel(0.5f, 0.0001f));
    }

    SECTION("Float with negative value") {
        store.set("offset", -3.14f);
        
        auto value = store.getFloat("offset");
        REQUIRE(value.has_value());
        REQUIRE_THAT(*value, WithinRel(-3.14f, 0.0001f));
    }

    SECTION("Float as JSON") {
        store.set("std_dev", 0.123f);
        
        auto json = store.getJson("std_dev");
        REQUIRE(json.has_value());
        // JSON should be a valid float string
        REQUIRE(json->find("0.123") != std::string::npos);
    }
}

TEST_CASE("PipelineValueStore - Integer storage", "[transforms][v2][value_store]") {
    PipelineValueStore store;

    SECTION("Store int, retrieve as int64") {
        store.set("count", 42);
        
        auto value = store.getInt("count");
        REQUIRE(value.has_value());
        REQUIRE(*value == 42);
    }

    SECTION("Store int64_t directly") {
        int64_t large_value = 1'000'000'000'000LL;
        store.set("timestamp", large_value);
        
        auto value = store.getInt("timestamp");
        REQUIRE(value.has_value());
        REQUIRE(*value == large_value);
    }

    SECTION("Negative integer") {
        store.set("offset", -100);
        
        auto value = store.getInt("offset");
        REQUIRE(value.has_value());
        REQUIRE(*value == -100);
    }

    SECTION("Integer as JSON") {
        store.set("trial_index", 5);
        
        auto json = store.getJson("trial_index");
        REQUIRE(json.has_value());
        REQUIRE(*json == "5");
    }
}

TEST_CASE("PipelineValueStore - String storage", "[transforms][v2][value_store]") {
    PipelineValueStore store;

    SECTION("Store and retrieve string") {
        store.set("label", std::string("test_label"));
        
        auto value = store.getString("label");
        REQUIRE(value.has_value());
        REQUIRE(*value == "test_label");
    }

    SECTION("Empty string") {
        store.set("empty", std::string(""));
        
        auto value = store.getString("empty");
        REQUIRE(value.has_value());
        REQUIRE(value->empty());
    }

    SECTION("String as JSON (quoted)") {
        store.set("name", std::string("test"));
        
        auto json = store.getJson("name");
        REQUIRE(json.has_value());
        REQUIRE(*json == "\"test\"");  // Should be JSON-quoted
    }
}

// ============================================================================
// Type Conversion Tests
// ============================================================================

TEST_CASE("PipelineValueStore - Type conversions", "[transforms][v2][value_store]") {
    PipelineValueStore store;

    SECTION("Int to float conversion") {
        store.set("integer", 42);
        
        auto as_float = store.getFloat("integer");
        REQUIRE(as_float.has_value());
        REQUIRE_THAT(*as_float, WithinRel(42.0f, 0.0001f));
    }

    SECTION("Float to int conversion (truncation)") {
        store.set("float_val", 3.7f);
        
        auto as_int = store.getInt("float_val");
        REQUIRE(as_int.has_value());
        REQUIRE(*as_int == 3);  // Truncated
    }

    SECTION("String cannot convert to numeric") {
        store.set("text", std::string("hello"));
        
        auto as_float = store.getFloat("text");
        auto as_int = store.getInt("text");
        
        REQUIRE_FALSE(as_float.has_value());
        REQUIRE_FALSE(as_int.has_value());
    }

    SECTION("Numeric cannot convert to string") {
        store.set("number", 42);
        
        auto as_string = store.getString("number");
        REQUIRE_FALSE(as_string.has_value());
    }
}

// ============================================================================
// Query Methods Tests
// ============================================================================

TEST_CASE("PipelineValueStore - Query methods", "[transforms][v2][value_store]") {
    PipelineValueStore store;

    SECTION("Empty store") {
        REQUIRE(store.empty());
        REQUIRE(store.size() == 0);
        REQUIRE_FALSE(store.contains("any_key"));
        REQUIRE(store.keys().empty());
    }

    SECTION("Non-existent key returns nullopt") {
        auto value = store.getFloat("missing");
        REQUIRE_FALSE(value.has_value());
        
        auto json = store.getJson("missing");
        REQUIRE_FALSE(json.has_value());
    }

    SECTION("Size tracking") {
        REQUIRE(store.size() == 0);
        
        store.set("a", 1);
        REQUIRE(store.size() == 1);
        
        store.set("b", 2);
        REQUIRE(store.size() == 2);
        
        store.set("a", 3);  // Overwrite
        REQUIRE(store.size() == 2);
    }

    SECTION("Keys enumeration") {
        store.set("alpha", 1);
        store.set("beta", 2.0f);
        store.set("gamma", std::string("three"));
        
        auto keys = store.keys();
        REQUIRE(keys.size() == 3);
        
        // Check all keys are present (order not guaranteed)
        REQUIRE(std::find(keys.begin(), keys.end(), "alpha") != keys.end());
        REQUIRE(std::find(keys.begin(), keys.end(), "beta") != keys.end());
        REQUIRE(std::find(keys.begin(), keys.end(), "gamma") != keys.end());
    }
}

// ============================================================================
// Mutation Methods Tests
// ============================================================================

TEST_CASE("PipelineValueStore - Merge operation", "[transforms][v2][value_store]") {
    SECTION("Merge non-overlapping stores") {
        PipelineValueStore store1;
        store1.set("a", 1);
        store1.set("b", 2.0f);
        
        PipelineValueStore store2;
        store2.set("c", 3);
        store2.set("d", std::string("four"));
        
        store1.merge(store2);
        
        REQUIRE(store1.size() == 4);
        REQUIRE(store1.contains("a"));
        REQUIRE(store1.contains("b"));
        REQUIRE(store1.contains("c"));
        REQUIRE(store1.contains("d"));
    }

    SECTION("Merge overwrites existing keys") {
        PipelineValueStore store1;
        store1.set("key", 1);
        
        PipelineValueStore store2;
        store2.set("key", 999);
        
        store1.merge(store2);
        
        REQUIRE(store1.size() == 1);
        auto value = store1.getInt("key");
        REQUIRE(value.has_value());
        REQUIRE(*value == 999);
    }
}

TEST_CASE("PipelineValueStore - Erase operation", "[transforms][v2][value_store]") {
    PipelineValueStore store;
    store.set("keep", 1);
    store.set("remove", 2);

    SECTION("Erase existing key") {
        bool erased = store.erase("remove");
        
        REQUIRE(erased);
        REQUIRE(store.size() == 1);
        REQUIRE_FALSE(store.contains("remove"));
        REQUIRE(store.contains("keep"));
    }

    SECTION("Erase non-existent key") {
        bool erased = store.erase("missing");
        
        REQUIRE_FALSE(erased);
        REQUIRE(store.size() == 2);
    }
}

TEST_CASE("PipelineValueStore - Clear operation", "[transforms][v2][value_store]") {
    PipelineValueStore store;
    store.set("a", 1);
    store.set("b", 2.0f);
    store.set("c", std::string("three"));
    
    REQUIRE(store.size() == 3);
    
    store.clear();
    
    REQUIRE(store.empty());
    REQUIRE(store.size() == 0);
    REQUIRE_FALSE(store.contains("a"));
}

// ============================================================================
// Raw Variant Access Tests
// ============================================================================

TEST_CASE("PipelineValueStore - Raw variant access", "[transforms][v2][value_store]") {
    PipelineValueStore store;
    store.set("float_val", 1.5f);
    store.set("int_val", int64_t{42});
    store.set("string_val", std::string("hello"));

    SECTION("Get raw float variant") {
        auto val = store.get("float_val");
        REQUIRE(val.has_value());
        REQUIRE(std::holds_alternative<float>(*val));
        REQUIRE_THAT(std::get<float>(*val), WithinRel(1.5f, 0.0001f));
    }

    SECTION("Get raw int variant") {
        auto val = store.get("int_val");
        REQUIRE(val.has_value());
        REQUIRE(std::holds_alternative<int64_t>(*val));
        REQUIRE(std::get<int64_t>(*val) == 42);
    }

    SECTION("Get raw string variant") {
        auto val = store.get("string_val");
        REQUIRE(val.has_value());
        REQUIRE(std::holds_alternative<std::string>(*val));
        REQUIRE(std::get<std::string>(*val) == "hello");
    }

    SECTION("Get missing key returns nullopt") {
        auto val = store.get("missing");
        REQUIRE_FALSE(val.has_value());
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("PipelineValueStore - Edge cases", "[transforms][v2][value_store]") {

    SECTION("Overwrite with different type") {
        PipelineValueStore store;
        store.set("key", 42);
        
        auto int_val = store.getInt("key");
        REQUIRE(int_val.has_value());
        REQUIRE(*int_val == 42);
        
        // Overwrite with float
        store.set("key", 3.14f);
        
        auto float_val = store.getFloat("key");
        REQUIRE(float_val.has_value());
        REQUIRE_THAT(*float_val, WithinRel(3.14f, 0.0001f));
        
        // Int access should still work via conversion
        int_val = store.getInt("key");
        REQUIRE(int_val.has_value());
        REQUIRE(*int_val == 3);  // Truncated
    }

    SECTION("Very large integers") {
        PipelineValueStore store;
        int64_t big = 9'000'000'000'000'000'000LL;
        store.set("big", big);
        
        auto value = store.getInt("big");
        REQUIRE(value.has_value());
        REQUIRE(*value == big);
    }

    SECTION("Special float values") {
        PipelineValueStore store;
        store.set("zero", 0.0f);
        
        auto zero = store.getFloat("zero");
        REQUIRE(zero.has_value());
        REQUIRE(*zero == 0.0f);
    }
}
