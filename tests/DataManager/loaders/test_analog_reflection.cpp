#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "utils/json_reflection.hpp"

#include <nlohmann/json.hpp>
#include <string>

using namespace WhiskerToolbox::Reflection;
using Catch::Matchers::ContainsSubstring;

// Minimal test struct to verify reflect-cpp works
struct SimpleStruct {
    std::string name = "default";
    int value = 42;
};

// Struct with optional fields for defaults
struct SimpleStructWithOptionals {
    std::string name;
    std::optional<int> value;
};

TEST_CASE("Reflect-cpp basic functionality", "[reflection][basic]") {
    SECTION("Simple struct parsing - all fields provided") {
        std::string json_str = R"({"name":"test","value":123})";
        auto result = rfl::json::read<SimpleStruct>(json_str);
        
        INFO("Result bool: " << static_cast<bool>(result));
        REQUIRE(result);
        
        auto obj = result.value();
        REQUIRE(obj.name == "test");
        REQUIRE(obj.value == 123);
    }
    
    SECTION("Simple struct with optional fields - defaults work") {
        std::string json_str = R"({"name":"test"})";
        auto result = rfl::json::read<SimpleStructWithOptionals>(json_str);
        
        INFO("Result bool with optionals: " << static_cast<bool>(result));
        REQUIRE(result);
        
        auto obj = result.value();
        REQUIRE(obj.name == "test");
        REQUIRE(!obj.value.has_value()); // Optional field not provided, so it's empty
    }
    
    SECTION("Simple struct with optional fields - value provided") {
        std::string json_str = R"({"name":"test","value":99})";
        auto result = rfl::json::read<SimpleStructWithOptionals>(json_str);
        
        REQUIRE(result);
        
        auto obj = result.value();
        REQUIRE(obj.name == "test");
        REQUIRE(obj.value.has_value());
        REQUIRE(obj.value.value() == 99);
    }
}

TEST_CASE("BinaryAnalogLoaderOptions - Default Values1", "[reflection][analog][binary]") {
    nlohmann::json json_obj = {
        {"filepath", "test.bin"},
        {"parent_dir", "/data"},
        {"header_size", 256},
        {"num_channels", 4},
        {"use_memory_mapped", true},
        {"offset", 100},
        {"stride", 2},
        {"binary_data_type", "int16"},
        {"scale_factor", 0.5f},
        {"offset_value", -1.0f},
        {"num_samples", 10000}
    };
    
    auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
    
    REQUIRE(result);
    
    auto opts = result.value();
    REQUIRE(opts.filepath == "test.bin");
    REQUIRE(opts.parent_dir.value_or("") == "/data");
    REQUIRE(opts.header_size.value().value() == 256);
    REQUIRE(opts.num_channels.value().value() == 4);
    REQUIRE(opts.use_memory_mapped.value_or(false) == true);
    REQUIRE(opts.offset.value().value() == 100);
    REQUIRE(opts.stride.value().value() == 2);
    REQUIRE(opts.binary_data_type.value_or("") == "int16");
    REQUIRE(opts.scale_factor.value_or(0.0f) == 0.5f);
    REQUIRE(opts.offset_value.value_or(0.0f) == -1.0f);
    REQUIRE(opts.num_samples.value_or(0) == 10000);
}

TEST_CASE("BinaryAnalogLoaderOptions - Default Values2", "[reflection][analog][binary]") {
    nlohmann::json json_obj = {
        {"filepath", "minimal.bin"}
        // All other fields should use their default values
    };
    
    std::string json_str = json_obj.dump();
    INFO("JSON input: " << json_str);
    
    // Try parsing directly with rfl to get better error messages
    try {
        auto result_direct = rfl::json::read<BinaryAnalogLoaderOptions>(json_str);
        if (!result_direct) {
            INFO("Direct parse failed");
        } else {
            INFO("Direct parse succeeded!");
        }
    } catch (const std::exception& e) {
        INFO("Exception from rfl::json::read: " << e.what());
    }
    
    auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
    
    REQUIRE(result);
    
    auto opts = result.value();
    REQUIRE(opts.filepath == "minimal.bin");
    // Optional fields should either not be set or use defaults via toLegacy()
    REQUIRE((!opts.parent_dir.has_value() || opts.parent_dir.value() == "."));
    REQUIRE((!opts.header_size.has_value() || opts.header_size.value().value() == 0));
    REQUIRE((!opts.num_channels.has_value() || opts.num_channels.value().value() == 1));
    REQUIRE((!opts.use_memory_mapped.has_value() || opts.use_memory_mapped.value() == false));
    REQUIRE((!opts.offset.has_value() || opts.offset.value().value() == 0));
    REQUIRE((!opts.stride.has_value() || opts.stride.value().value() == 1));
    REQUIRE((!opts.binary_data_type.has_value() || opts.binary_data_type.value() == "int16"));
    REQUIRE((!opts.scale_factor.has_value() || opts.scale_factor.value() == 1.0f));
    REQUIRE((!opts.offset_value.has_value() || opts.offset_value.value() == 0.0f));
    REQUIRE((!opts.num_samples.has_value() || opts.num_samples.value() == 0));
}

TEST_CASE("BinaryAnalogLoaderOptions - Validation", "[reflection][analog][binary]") {
    SECTION("Negative header_size should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.bin"},
            {"header_size", -10}
        };
        
        auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
    
    SECTION("Zero num_channels should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.bin"},
            {"num_channels", 0}
        };
        
        auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
    
    SECTION("Zero stride should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.bin"},
            {"stride", 0}
        };
        
        auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
    
    SECTION("Invalid binary_data_type should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.bin"},
            {"binary_data_type", "invalid_type"}
        };
        
        auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
    
    SECTION("Valid binary_data_type types should pass") {
        std::vector<std::string> valid_types = {"int16", "float32", "int8", "uint16", "float64"};
        
        for (const auto& type : valid_types) {
            nlohmann::json json_obj = {
                {"filepath", "test.bin"},
                {"binary_data_type", type}
            };
            
            auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
            REQUIRE(result);
            REQUIRE(result.value().binary_data_type == type);
        }
    }
}

TEST_CASE("BinaryAnalogLoaderOptions - Serialization Round-trip", "[reflection][analog][binary]") {
    // Create JSON directly with all fields
    nlohmann::json json_obj = {
        {"filepath", "roundtrip.bin"},
        {"parent_dir", "/test"},
        {"header_size", 512},
        {"num_channels", 8},
        {"use_memory_mapped", true},
        {"offset", 200},
        {"stride", 4},
        {"binary_data_type", "float32"},
        {"scale_factor", 2.5f},
        {"offset_value", 1.5f},
        {"num_samples", 5000}
    };
    
    // Parse
    auto result = parseJson<BinaryAnalogLoaderOptions>(json_obj);
    REQUIRE(result);
    
    auto original = result.value();
    
    // Serialize back
    auto json_roundtrip = toJson(original);
    
    // Parse again
    auto result2 = parseJson<BinaryAnalogLoaderOptions>(json_roundtrip);
    REQUIRE(result2);
    
    auto parsed = result2.value();
    REQUIRE(parsed.filepath == original.filepath);
    REQUIRE(parsed.parent_dir == original.parent_dir);
    REQUIRE(parsed.header_size == original.header_size);
    REQUIRE(parsed.num_channels == original.num_channels);
    REQUIRE(parsed.use_memory_mapped == original.use_memory_mapped);
    REQUIRE(parsed.offset == original.offset);
    REQUIRE(parsed.stride == original.stride);
    REQUIRE(parsed.binary_data_type == original.binary_data_type);
    REQUIRE(parsed.scale_factor == original.scale_factor);
    REQUIRE(parsed.offset_value == original.offset_value);
    REQUIRE(parsed.num_samples == original.num_samples);
}


TEST_CASE("CSVAnalogLoaderOptions - Basic Parsing", "[reflection][analog][csv]") {
    nlohmann::json json_obj = {
        {"filepath", "test.csv"},
        {"delimiter", ";"},
        {"has_header", true},
        {"single_column_format", false},
        {"time_column", 1},
        {"data_column", 2}
    };
    
    auto result = parseJson<CSVAnalogLoaderOptions>(json_obj);
    
    REQUIRE(result);
    
    auto opts = result.value();
    REQUIRE(opts.filepath == "test.csv");
    REQUIRE(opts.delimiter == ";");
    REQUIRE(opts.has_header == true);
    REQUIRE(opts.single_column_format == false);
    REQUIRE(opts.time_column.value() == 1);
    REQUIRE(opts.data_column.value() == 2);
}

TEST_CASE("CSVAnalogLoaderOptions - Default Values", "[reflection][analog][csv]") {
    nlohmann::json json_obj = {
        {"filepath", "minimal.csv"}
    };
    
    auto result = parseJson<CSVAnalogLoaderOptions>(json_obj);
    
    REQUIRE(result);
    
    auto opts = result.value();
    REQUIRE(opts.filepath == "minimal.csv");
    
    // Optional fields should not be set when not provided in JSON
    REQUIRE_FALSE(opts.delimiter.has_value());
    REQUIRE_FALSE(opts.has_header.has_value());
    REQUIRE_FALSE(opts.single_column_format.has_value());
    REQUIRE_FALSE(opts.time_column.has_value());
    REQUIRE_FALSE(opts.data_column.has_value());
    
    // Check defaults via helper methods
    REQUIRE(opts.getDelimiter() == ",");
    REQUIRE(opts.getHasHeader() == false);
    REQUIRE(opts.getSingleColumnFormat() == true);
    REQUIRE(opts.getTimeColumn() == 0);
    REQUIRE(opts.getDataColumn() == 1);
}

TEST_CASE("CSVAnalogLoaderOptions - Validation", "[reflection][analog][csv]") {
    SECTION("Negative time_column should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.csv"},
            {"time_column", -1}
        };
        
        auto result = parseJson<CSVAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
    
    SECTION("Negative data_column should fail") {
        nlohmann::json json_obj = {
            {"filepath", "test.csv"},
            {"data_column", -5}
        };
        
        auto result = parseJson<CSVAnalogLoaderOptions>(json_obj);
        REQUIRE_FALSE(result);
    }
}

TEST_CASE("CSVAnalogLoaderOptions - Serialization Round-trip", "[reflection][analog][csv]") {
    CSVAnalogLoaderOptions original;
    original.filepath = "roundtrip.csv";
    original.delimiter = "\t";
    original.has_header = true;
    original.single_column_format = false;
    original.time_column = 3;
    original.data_column = 5;
    
    // Serialize
    auto json_obj = toJson(original);
    
    // Deserialize
    auto result = parseJson<CSVAnalogLoaderOptions>(json_obj);
    REQUIRE(result);
    
    auto parsed = result.value();
    REQUIRE(parsed.filepath == original.filepath);
    REQUIRE(parsed.delimiter == original.delimiter);
    REQUIRE(parsed.has_header == original.has_header);
    REQUIRE(parsed.single_column_format == original.single_column_format);
    REQUIRE(parsed.time_column.value() == original.time_column.value());
    REQUIRE(parsed.data_column.value() == original.data_column.value());
}


// Note: Schema generation doesn't work with custom validators yet
// TODO: Implement to_schema() for ValidDataType once reflect-cpp pattern is clearer
/*
TEST_CASE("JSON Schema Generation", "[reflection][analog][schema]") {
    SECTION("BinaryAnalogLoaderOptions schema") {
        auto schema = generateSchema<BinaryAnalogLoaderOptions>();
        
        REQUIRE_FALSE(schema.empty());
        
        // Schema should contain field names
        REQUIRE_THAT(schema, ContainsSubstring("filename"));
        REQUIRE_THAT(schema, ContainsSubstring("header_size"));
        REQUIRE_THAT(schema, ContainsSubstring("num_channels"));
        REQUIRE_THAT(schema, ContainsSubstring("data_type"));
    }
    
    SECTION("CSVAnalogLoaderOptions schema") {
        auto schema = generateSchema<CSVAnalogLoaderOptions>();
        
        REQUIRE_FALSE(schema.empty());
        
        // Schema should contain field names
        REQUIRE_THAT(schema, ContainsSubstring("filepath"));
        REQUIRE_THAT(schema, ContainsSubstring("delimiter"));
        REQUIRE_THAT(schema, ContainsSubstring("time_column"));
        REQUIRE_THAT(schema, ContainsSubstring("data_column"));
    }
}
*/
